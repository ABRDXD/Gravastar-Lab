// SPDX-License-Identifier: GPL-3.0-or-later
// gravastar_record: deterministic, resumable recording on the GPU renderer
// (Phase F). Spec format, modes and the determinism statement:
// docs/RECORDING.md.
//
//   record   gravastar_record --spec S --job DIR [--stop-after-minutes M]
//                             [--max-frames N] [--encode] [--recover-stale-lock]
//   verify   gravastar_record --spec S --job DIR --verify 0,137,599 [--output F]
//   measure  gravastar_record --spec S --measure FRAME [--output F]
//   summary  gravastar_record --job DIR --summary [--output F]
//
// Overrides (part of the job's identity): --precision fp32|fp64,
// --tolerance T|scene, --width W, --height H.
#include "gravastar/optics/scene.hpp"
#include "gravastar/recording/recorder.hpp"
#include "gravastar/rendering/GpuTracer.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <chrono>
#include <csignal>
#include <ctime>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <unistd.h>

namespace o = gravastar::optics;
namespace rec = gravastar::recording;
namespace gr = gravastar::rendering;
namespace u = gravastar::tools;
using J = u::Json;
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

namespace {
std::atomic<bool> interrupted{false};
static_assert(std::atomic<bool>::is_always_lock_free);
// First Ctrl+C stops at the next chunk (the current frame is abandoned, never
// written); a second one kills the process at once.
extern "C" void on_signal(int signal) {
    interrupted = true;
    std::signal(signal, SIG_DFL);
}

double seconds_since(Clock::time_point t) {
    return std::chrono::duration<double>(Clock::now() - t).count();
}
std::string now_utc() {
    const auto t = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&t, &tm);
    std::ostringstream s;
    s << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return s.str();
}
// Runs f(i) for i in [0, n) on every hardware thread; rethrows the first error.
template <class F> void parallel_for(std::size_t n, F f) {
    const std::size_t threads = std::max(1u, std::thread::hardware_concurrency());
    std::exception_ptr error;
    std::mutex m;
    {
        std::vector<std::jthread> pool;
        for (std::size_t w = 0; w < threads; ++w)
            pool.emplace_back([&, w] {
                try {
                    for (std::size_t i = n * w / threads, end = n * (w + 1) / threads; i < end; ++i)
                        f(i);
                } catch (...) {
                    std::lock_guard lock(m);
                    if (!error)
                        error = std::current_exception();
                }
            });
    }
    if (error)
        std::rethrow_exception(error);
}

struct Camera {
    double r, theta, phi;
};
struct Key {
    std::uint64_t frame;
    Camera camera;
};
struct Spec {
    J document;   // effective spec, overrides applied; frozen in job.json
    J scene_json; // the optics scene file's contents
    o::Scene scene;
    rec::TimelineConfig timeline;
    bool fp32{};
    std::optional<double> tolerance; // empty: the scene's own settings
    std::vector<Key> keys;
};

Spec load_spec(const fs::path &path, const J &overrides) {
    Spec s;
    J d = J::parse(u::read_file(path));
    if (d.at("schema_version") != 1)
        throw std::invalid_argument("unsupported recording spec schema");
    for (const auto &[k, v] : overrides.items()) {
        if (k == "precision" || k == "tolerance")
            d["renderer"][k] = v;
        else
            d["timeline"][k] = v;
    }
    s.scene_json = J::parse(u::read_file(path.parent_path() / d.at("scene").get<std::string>()));
    s.scene = o::parse_scene(s.scene_json);
    if (!s.scene.angular)
        throw std::invalid_argument("recording needs an angular-patch scene (GPU tracer)");
    const auto &t = d.at("timeline");
    s.timeline.width = t.at("width");
    s.timeline.height = t.at("height");
    s.timeline.fps = {t.at("fps").at(0), t.at("fps").at(1)};
    s.timeline.frame_count = t.at("frame_count");
    s.timeline.radiance_channels = d.at("radiance").get<bool>() ? 3 : 0;
    const auto &r = d.at("renderer");
    const auto precision = r.at("precision").get<std::string>();
    if (precision != "fp32" && precision != "fp64")
        throw std::invalid_argument("renderer.precision must be fp32 or fp64");
    s.fp32 = precision == "fp32";
    if (!r.at("tolerance").is_null())
        s.tolerance = r.at("tolerance").get<double>();
    if (r.at("samples_per_pixel") != 1)
        throw std::invalid_argument(
            "samples_per_pixel must be 1: one deterministic ray through each pixel centre");
    const auto &c = d.at("camera_track");
    if (c.at("mode") != "stationary_snapshots" ||
        c.at("interpolation") != "piecewise_linear_r_theta_phi")
        throw std::invalid_argument("unsupported camera track mode or interpolation");
    const double r_min = 2.5, r_max = 0.9 * s.scene.settings.outer_radius;
    for (const auto &k : c.at("keys")) {
        Key key{k.at("frame"), {k.at("r"), k.at("theta"), k.at("phi")}};
        const auto &cam = key.camera;
        if (!std::isfinite(cam.phi) || !(cam.r >= r_min && cam.r <= r_max) ||
            !(cam.theta >= 0.05 && cam.theta <= std::numbers::pi - 0.05))
            throw std::invalid_argument("camera key outside r in [2.5, 0.9 outer], theta in "
                                        "[0.05, pi - 0.05]");
        if (!s.keys.empty() && key.frame <= s.keys.back().frame)
            throw std::invalid_argument("camera key frames must strictly increase");
        s.keys.push_back(key);
    }
    if (s.keys.empty() || s.keys.front().frame != 0 ||
        s.keys.back().frame != s.timeline.frame_count - 1)
        throw std::invalid_argument("camera keys must start at frame 0 and end at the last frame");
    s.document = d;
    return s;
}

// Piecewise linear in (r, theta, phi) between keys at integer frames; keys
// are hit exactly; phi is not wrapped, so keys give the direction of travel.
Camera camera_at(const std::vector<Key> &keys, std::uint64_t i) {
    for (std::size_t k = 0; k + 1 < keys.size(); ++k)
        if (i <= keys[k + 1].frame) {
            const auto &a = keys[k], &b = keys[k + 1];
            const double t = double(i - a.frame) / double(b.frame - a.frame);
            return {a.camera.r + t * (b.camera.r - a.camera.r),
                    a.camera.theta + t * (b.camera.theta - a.camera.theta),
                    a.camera.phi + t * (b.camera.phi - a.camera.phi)};
        }
    return keys.back().camera;
}

struct Renderer {
    o::Scene scene; // tolerance applied
    const char *spirv;
    gr::GpuTracer tracer;
    std::size_t batch;
    Renderer(const Spec &s, bool fp32, std::optional<double> tolerance)
        : scene(tolerance ? o::with_tolerance(s.scene, *tolerance) : s.scene),
          spirv(fp32 ? GRAVASTAR_TRACE32_SPV : GRAVASTAR_TRACE64_SPV),
          tracer(spirv, options(), fp32 ? gr::GpuTracer::Precision::fp32
                                        : gr::GpuTracer::Precision::fp64),
          // About 0.1-0.5 s of GPU work per dispatch for either precision.
          batch(fp32 ? 32768 : 4096) {}
    static gravastar::rendering::vulkan::ComputeOptions options() {
        gravastar::rendering::vulkan::ComputeOptions o;
        o.validation = gravastar::rendering::vulkan::ValidationMode::disabled;
        return o;
    }
    // What determines the frames besides the spec: frames are reproducible
    // bit for bit only with the same executable, kernel, device and driver.
    [[nodiscard]] J identity() const {
        const auto &d = tracer.device();
        return {{"executable_sha256", u::sha256(u::read_file("/proc/self/exe"))},
                {"kernel", fs::path(spirv).filename().string()},
                {"kernel_sha256", u::sha256(u::read_file(spirv))},
                {"precision", tracer.precision() == gr::GpuTracer::Precision::fp32 ? "FP32" : "FP64"},
                {"device", d.name},
                {"vendor_id", d.vendor_id},
                {"device_id", d.device_id},
                {"driver_version", d.driver_version},
                {"api_version", d.api_version},
                {"cpu_fallback", "CPU tracer at the same settings; results do not depend on the "
                                 "thread count"}};
    }
};

struct FrameStats {
    double seconds{}, gpu_milliseconds{}, fallback_seconds{};
    std::array<std::size_t, 7> fallback{};
    std::map<std::string, std::size_t> outcomes;
    std::size_t near_tangent{};
    [[nodiscard]] J json() const {
        J f = J::object();
        for (std::size_t i = 1; i < fallback.size(); ++i)
            f[gr::GpuTracer::reasons[i]] = fallback[i];
        return {{"seconds", seconds},         {"gpu_milliseconds", gpu_milliseconds},
                {"cpu_fallback_seconds", fallback_seconds}, {"cpu_fallback", f},
                {"outcomes", outcomes},       {"near_tangent", near_tangent}};
    }
};
struct Rendered {
    rec::Frame frame;
    std::vector<std::uint8_t> outcome;
    FrameStats stats;
};

// One frame: one ray through each pixel centre, from a camera at rest in the
// scene's observer frame at `camera` (a stationary snapshot).
// Radiance channels: 0 linear disk intensity (0 off the disk), 1 linear
// background intensity (0 without a background), 2 outcome code
// (optics::Outcome). Throws RecordingCancelled when interrupted.
Rendered render(Renderer &renderer, const rec::TimelineConfig &timeline, Camera camera) {
    const auto begin = Clock::now();
    auto s = renderer.scene;
    s.width = timeline.width;
    s.height = timeline.height;
    s.position[1] = camera.r;
    s.position[2] = camera.theta;
    s.position[3] = camera.phi;
    const auto patch = camera.theta <= std::numbers::pi / 2 ? o::Chart::north : o::Chart::south;
    const std::size_t n = std::size_t(s.width) * s.height;
    const auto channels = std::size_t(timeline.radiance_channels);
    Rendered out;
    out.frame.display = {s.width, s.height, std::vector<std::uint8_t>(3 * n)};
    out.frame.radiance.assign(channels * n, 0.0);
    out.outcome.assign(n, 0);
    constexpr std::size_t chunk = std::size_t(1) << 17; // bounds memory per trace call
    for (std::size_t first = 0; first < n; first += chunk) {
        if (interrupted)
            throw rec::RecordingCancelled();
        const std::size_t count = std::min(chunk, n - first);
        std::vector<o::State> initial(count), start(count);
        parallel_for(count, [&](std::size_t i) {
            const auto p = first + i;
            initial[i] = o::scene_pixel(s, int(p % std::size_t(s.width)), int(p / std::size_t(s.width)));
            start[i] = o::transform_state(initial[i], o::Chart::boyer_lindquist, patch);
        });
        const auto traced = renderer.tracer.trace(s, start, patch, renderer.batch, &interrupted);
        if (traced.cancelled)
            throw rec::RecordingCancelled();
        out.stats.gpu_milliseconds += traced.gpu_milliseconds;
        out.stats.fallback_seconds += traced.fallback_seconds;
        for (std::size_t k = 0; k < traced.fallback.size(); ++k)
            out.stats.fallback[k] += traced.fallback[k];
        parallel_for(count, [&](std::size_t i) {
            const auto p = first + i;
            const auto &t = traced.rays[i].trace;
            const auto sample = o::ray_sample(s, initial[i], t);
            const auto c = o::sample_color(t.outcome, sample);
            std::copy(c.begin(), c.end(), out.frame.display.rgb.begin() + std::ptrdiff_t(3 * p));
            out.outcome[p] = std::uint8_t(t.outcome);
            if (channels) {
                double *v = out.frame.radiance.data() + channels * p;
                v[0] = t.outcome == o::Outcome::emitted ? sample.intensity.value_or(0) : 0;
                v[1] = sample.background.value_or(0);
                v[2] = double(int(t.outcome));
            }
        });
        for (std::size_t i = 0; i < count; ++i) {
            const auto &t = traced.rays[i].trace;
            ++out.stats.outcomes[std::string(o::outcome_name(t.outcome))];
            out.stats.near_tangent += !t.tangencies.empty();
        }
    }
    out.stats.seconds = seconds_since(begin);
    return out;
}

J job_description(const Spec &spec, const Renderer &renderer) {
    return {{"recording_spec", spec.document},
            {"optics_scene", spec.scene_json},
            {"renderer", renderer.identity()},
            {"camera", "stationary snapshots: each frame's observer is at rest in the scene's "
                       "observer frame at the interpolated position; camera motion between "
                       "frames adds no aberration or Doppler shift"},
            {"display_mapping", "optics::sample_color (diagnostic): disk v = (I/(1+I))^(1/2.2) "
                                "grey; background blue tint; capture black"}};
}

std::vector<std::uint64_t> parse_frames(const std::string &list, std::uint64_t count) {
    std::vector<std::uint64_t> out;
    std::stringstream in(list);
    for (std::string item; std::getline(in, item, ',');) {
        const auto v = std::stoull(item);
        if (v >= count)
            throw std::invalid_argument("frame " + item + " is outside the timeline");
        out.push_back(v);
    }
    if (out.empty())
        throw std::invalid_argument("no frames given");
    return out;
}

void write_output(const std::optional<fs::path> &output, const J &report) {
    if (output)
        u::atomic_json(*output, report);
    std::cout << report.dump(2) << '\n';
}

// ---------------------------------------------------------------- record
int record(const Spec &spec, const fs::path &job, std::optional<double> budget_minutes,
           std::optional<std::uint64_t> max_frames, bool encode, bool recover_stale_lock) {
    Renderer renderer(spec, spec.fp32, spec.tolerance);
    std::optional<std::pair<std::uint64_t, FrameStats>> last;
    const rec::FrameSource source = [&](std::uint64_t index, double) {
        auto r = render(renderer, spec.timeline, camera_at(spec.keys, index));
        last = {index, r.stats};
        return std::move(r.frame);
    };
    rec::Options options;
    options.recover_stale_lock = recover_stale_lock;
    rec::RecordingSession session(job, spec.timeline, job_description(spec, renderer), source,
                                  options);
    const auto opened = session.open();
    std::cout << "job " << job.string() << " (" << session.content_sha256().substr(0, 12) << ")"
              << (opened.new_job ? " created" : " resumed") << ": " << session.completed() << "/"
              << spec.timeline.frame_count << " frames done; kept " << opened.kept << ", adopted "
              << opened.adopted << ", quarantined " << opened.quarantined << ", missing "
              << opened.missing << ", extra " << opened.extra << ", temporaries removed "
              << opened.temporaries << (opened.stale_lock_recovered ? ", stale lock reclaimed" : "")
              << "\non " << renderer.tracer.device().name << " ("
              << (spec.fp32 ? "FP32" : "FP64") << ", tolerance "
              << (spec.tolerance ? std::to_string(*spec.tolerance) : std::string("scene")) << ")\n";
    std::ofstream log(job / "logs" / "render.jsonl", std::ios::app);
    log << J{{"session_start", now_utc()}, {"pid", long(getpid())},
             {"completed_at_start", session.completed()}, {"resume", J{{"kept", opened.kept},
             {"adopted", opened.adopted}, {"quarantined", opened.quarantined},
             {"missing", opened.missing}, {"extra", opened.extra}}}}.dump()
        << std::endl;
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    const auto begin = Clock::now();
    std::uint64_t rendered = 0;
    double frame_seconds = 0;
    std::string stop = "complete";
    while (!session.complete()) {
        if (interrupted) {
            stop = "interrupted";
            break;
        }
        if (budget_minutes && seconds_since(begin) >= 60 * *budget_minutes) {
            stop = "time budget reached";
            break;
        }
        if (max_frames && rendered >= *max_frames) {
            stop = "frame limit reached";
            break;
        }
        last.reset();
        session.render_next();
        if (session.cancelled() || !last) {
            stop = "interrupted (current frame abandoned, not written)";
            break;
        }
        ++rendered;
        frame_seconds += last->second.seconds;
        J line = last->second.json();
        line["frame"] = last->first;
        log << line.dump() << std::endl;
        const double each = frame_seconds / double(rendered);
        const auto left = spec.timeline.frame_count - session.completed();
        std::cout << "frame " << std::setw(6) << last->first << "  " << std::fixed
                  << std::setprecision(1) << last->second.seconds << " s  ("
                  << session.completed() << "/" << spec.timeline.frame_count << ", about "
                  << std::setprecision(1) << each * double(left) / 3600 << " h left)" << std::endl;
    }
    std::cout << "stopped: " << stop << "; " << rendered << " frames this part in "
              << std::setprecision(1) << seconds_since(begin) / 60 << " min; "
              << session.completed() << "/" << spec.timeline.frame_count << " done\n";
    if (!session.complete()) {
        std::cout << "resume later with the same command\n";
        return encode ? 3 : 0;
    }
    if (!encode)
        return 0;
    std::cout << "encoding...\n";
    const auto e = session.encode();
    if (!e.success) {
        std::cerr << "encode failed: " << e.failure << " (frames kept)\n";
        return 4;
    }
    std::cout << "video " << e.video << ": " << e.probe.value("width", 0) << "x"
              << e.probe.value("height", 0) << ", " << e.probe.value("r_frame_rate", std::string())
              << " fps, " << e.probe.value("nb_read_frames", std::string()) << " frames, "
              << e.probe.value("duration", std::string()) << " s\n";
    return 0;
}

// ---------------------------------------------------------------- verify
// Re-renders frames from scratch (no job state) and compares them with the
// recorded hashes: the resumed job must equal an uninterrupted render.
int verify(const Spec &spec, const fs::path &job, const std::vector<std::uint64_t> &frames,
           const std::optional<fs::path> &output, bool check) {
    Renderer renderer(spec, spec.fp32, spec.tolerance);
    rec::RecordingSession probe(job, spec.timeline, job_description(spec, renderer),
                                [](std::uint64_t, double) -> rec::Frame {
                                    throw std::logic_error("not rendering");
                                });
    const auto job_json = J::parse(u::read_file(job / "job.json"));
    if (job_json.value("content_sha256", std::string()) != probe.content_sha256())
        throw std::runtime_error("this spec, build or device does not match the job");
    const auto progress = J::parse(u::read_file(job / "progress.json"));
    J results = J::array();
    bool all = true;
    for (const auto i : frames) {
        const auto key = rec::RecordingSession::frame_name(i, "").substr(6);
        if (!progress.at("frames").contains(key))
            throw std::runtime_error("frame " + std::to_string(i) + " is not recorded yet");
        const auto &recorded = progress["frames"][key];
        auto r = render(renderer, spec.timeline, camera_at(spec.keys, i));
        const auto png = u::sha256(rec::encode_png(r.frame.display));
        bool same = png == recorded.value("png_sha256", std::string());
        J item{{"frame", i}, {"png_sha256", png}, {"seconds", r.stats.seconds}};
        if (spec.timeline.radiance_channels) {
            const auto npy = u::sha256(rec::encode_npy(spec.timeline.height, spec.timeline.width,
                                                       spec.timeline.radiance_channels,
                                                       r.frame.radiance));
            same = same && npy == recorded.value("radiance_sha256", std::string());
            item["radiance_sha256"] = npy;
        }
        item["identical"] = same;
        all = all && same;
        results.push_back(item);
        std::cout << "frame " << i << (same ? " identical" : " DIFFERS") << " (" << std::fixed
                  << std::setprecision(1) << r.stats.seconds << " s)" << std::endl;
    }
    write_output(output, {{"schema_version", 1},
                          {"job", job.string()},
                          {"content_sha256", probe.content_sha256()},
                          {"frames", results},
                          {"all_identical", all},
                          {"meaning", "each frame re-rendered from scratch in a fresh process "
                                      "state and compared byte for byte with the resumed job"}});
    return check && !all ? 1 : 0;
}

// ---------------------------------------------------------------- measure
// Speed and display accuracy of the recording paths on one real frame,
// against FP64 at the scene's own tolerance.
int measure(const Spec &spec, std::uint64_t frame, const std::optional<fs::path> &output) {
    struct Variant {
        std::string name;
        bool fp32;
        std::optional<double> tolerance;
    };
    std::vector<Variant> variants{{"fp64_scene_tolerance", false, std::nullopt}};
    if (spec.tolerance)
        variants.push_back({"fp64_tolerance", false, spec.tolerance});
    variants.push_back({"fp32_tolerance", true, spec.tolerance});
    const auto camera = camera_at(spec.keys, frame);
    std::vector<Rendered> images;
    J results = J::array();
    for (const auto &v : variants) {
        std::cout << "rendering frame " << frame << " as " << v.name << "..." << std::endl;
        Renderer renderer(spec, v.fp32, v.tolerance);
        images.push_back(render(renderer, spec.timeline, camera));
        const auto &img = images.back();
        const auto &ref = images.front();
        J item{{"variant", v.name},
               {"precision", v.fp32 ? "FP32" : "FP64"},
               {"tolerance", v.tolerance ? J(*v.tolerance) : J("scene")},
               {"frame_seconds", img.stats.seconds},
               {"estimated_job_hours", img.stats.seconds * double(spec.timeline.frame_count) / 3600},
               {"stats", img.stats.json()}};
        const std::size_t n = img.outcome.size();
        std::size_t differ = 0, over2 = 0, over8 = 0, over32 = 0, outcome = 0;
        int largest = 0;
        double squared = 0;
        for (std::size_t p = 0; p < n; ++p) {
            int d = 0;
            for (int c = 0; c < 3; ++c) {
                const int e = std::abs(int(img.frame.display.rgb[3 * p + c]) -
                                       int(ref.frame.display.rgb[3 * p + c]));
                d = std::max(d, e);
                squared += double(e) * e;
            }
            differ += d > 0;
            over2 += d > 2;
            over8 += d > 8;
            over32 += d > 32;
            largest = std::max(largest, d);
            outcome += img.outcome[p] != ref.outcome[p];
        }
        const double mse = squared / double(3 * n);
        item["versus_reference"] = {
            {"pixels", n},
            {"pixels_differing", differ},
            {"pixels_differing_more_than_2_levels", over2},
            {"pixels_differing_more_than_8_levels", over8},
            {"pixels_differing_more_than_32_levels", over32},
            {"largest_channel_difference", largest},
            {"outcome_changes", outcome},
            {"psnr_db", mse > 0 ? J(10 * std::log10(255.0 * 255.0 / mse)) : J("identical")}};
        results.push_back(item);
        std::cout << "  " << std::fixed << std::setprecision(1) << img.stats.seconds
                  << " s per frame, about " << img.stats.seconds * double(spec.timeline.frame_count) / 3600
                  << " h for the job; " << differ << " pixels differ from the reference ("
                  << over8 << " by more than 8 levels), " << outcome << " outcome changes"
                  << std::endl;
    }
    write_output(output, {{"schema_version", 1},
                          {"frame", frame},
                          {"camera", {camera.r, camera.theta, camera.phi}},
                          {"width", spec.timeline.width},
                          {"height", spec.timeline.height},
                          {"recording_spec", spec.document},
                          {"variants", results},
                          {"reference", "FP64 GPU tracer at the scene's own tolerance"}});
    return 0;
}

// ---------------------------------------------------------------- summary
std::uintmax_t directory_bytes(const fs::path &d) {
    std::uintmax_t total = 0;
    if (fs::exists(d))
        for (const auto &e : fs::directory_iterator(d))
            if (e.is_regular_file())
                total += e.file_size();
    return total;
}
int summary(const fs::path &job, const std::optional<fs::path> &output) {
    const auto job_json = J::parse(u::read_file(job / "job.json"));
    const auto progress = J::parse(u::read_file(job / "progress.json"));
    std::map<std::uint64_t, J> frames; // last log entry per frame wins
    std::size_t sessions = 0, productive = 0;
    bool counted = false;
    std::ifstream log(job / "logs" / "render.jsonl");
    for (std::string line; std::getline(log, line);) {
        if (line.empty())
            continue;
        const auto entry = J::parse(line);
        if (entry.contains("session_start")) {
            ++sessions;
            counted = false;
        } else if (entry.contains("frame")) {
            frames[entry["frame"].get<std::uint64_t>()] = entry;
            if (!counted)
                ++productive;
            counted = true;
        }
    }
    double seconds = 0, slowest = 0, fastest = 1e300;
    std::size_t fallback = 0;
    for (const auto &[i, e] : frames) {
        const double s = e["seconds"];
        seconds += s;
        slowest = std::max(slowest, s);
        fastest = std::min(fastest, s);
        for (const auto &[k, v] : e["cpu_fallback"].items())
            fallback += v.get<std::size_t>();
    }
    J encode = nullptr;
    if (fs::exists(job / "logs" / "encode.json"))
        encode = J::parse(u::read_file(job / "logs" / "encode.json"));
    const auto video = job / "video.mp4";
    const J report{
        {"schema_version", 1},
        {"job", job.string()},
        {"content_sha256", job_json["content_sha256"]},
        {"timeline", job_json["timeline"]},
        {"renderer", job_json["scene"]["renderer"]},
        {"completed", progress["completed"]},
        {"state", progress["state"]},
        {"sessions_started", sessions},
        {"sessions_that_rendered", productive},
        {"render_seconds_total", seconds},
        {"render_hours_total", seconds / 3600},
        {"frame_seconds",
         {{"mean", frames.empty() ? 0 : seconds / double(frames.size())},
          {"fastest", frames.empty() ? 0 : fastest},
          {"slowest", slowest}}},
        {"cpu_fallback_rays_total", fallback},
        {"storage_bytes",
         {{"frames", directory_bytes(job / "frames")},
          {"radiance", directory_bytes(job / "radiance")},
          {"video", fs::exists(video) ? fs::file_size(video) : 0},
          {"logs", directory_bytes(job / "logs")}}},
        {"encode", encode},
        {"note", "render time sums each frame's last logged render; time spent on abandoned "
                 "(interrupted) frames is not included"}};
    write_output(output, report);
    return 0;
}

[[noreturn]] void usage(const std::string &why) {
    throw std::invalid_argument(
        why + "\nusage: gravastar_record --spec S --job DIR [--stop-after-minutes M] "
              "[--max-frames N] [--encode] [--recover-stale-lock]\n"
              "       gravastar_record --spec S --job DIR --verify 0,137,599 [--check] [--output F]\n"
              "       gravastar_record --spec S --measure FRAME [--output F]\n"
              "       gravastar_record --job DIR --summary [--output F]\n"
              "overrides: --precision fp32|fp64 --tolerance T|scene --width W --height H");
}
} // namespace

int main(int argc, char **argv) {
    try {
        std::optional<fs::path> spec_path, job, output;
        std::optional<double> budget;
        std::optional<std::uint64_t> max_frames, measure_frame;
        std::optional<std::string> verify_list;
        bool encode = false, recover = false, show_summary = false, check = false;
        J overrides = J::object();
        for (int i = 1; i < argc; ++i) {
            const std::string a = argv[i];
            auto value = [&]() -> std::string {
                if (i + 1 >= argc)
                    usage(a + " needs a value");
                return argv[++i];
            };
            if (a == "--spec") spec_path = value();
            else if (a == "--job") job = value();
            else if (a == "--output") output = value();
            else if (a == "--stop-after-minutes") budget = std::stod(value());
            else if (a == "--max-frames") max_frames = std::stoull(value());
            else if (a == "--measure") measure_frame = std::stoull(value());
            else if (a == "--verify") verify_list = value();
            else if (a == "--encode") encode = true;
            else if (a == "--check") check = true;
            else if (a == "--recover-stale-lock") recover = true;
            else if (a == "--summary") show_summary = true;
            else if (a == "--precision") overrides["precision"] = value();
            else if (a == "--tolerance") {
                const auto v = value();
                overrides["tolerance"] = v == "scene" ? J(nullptr) : J(std::stod(v));
            } else if (a == "--width") overrides["width"] = std::stoi(value());
            else if (a == "--height") overrides["height"] = std::stoi(value());
            else usage("unknown option " + a);
        }
        if (show_summary) {
            if (!job)
                usage("--summary needs --job");
            return summary(*job, output);
        }
        if (!spec_path)
            usage("--spec is required");
        // Relative spec paths fall back to the source tree, as the experiments do.
        if (spec_path->is_relative() && !fs::exists(*spec_path))
            spec_path = fs::path(GRAVASTAR_SOURCE_ROOT) / *spec_path;
        const auto spec = load_spec(*spec_path, overrides);
        if (measure_frame) {
            if (*measure_frame >= spec.timeline.frame_count)
                usage("--measure frame is outside the timeline");
            return measure(spec, *measure_frame, output);
        }
        if (!job)
            usage("--job is required");
        if (verify_list)
            return verify(spec, *job, parse_frames(*verify_list, spec.timeline.frame_count), output,
                          check);
        return record(spec, *job, budget, max_frames, encode, recover);
    } catch (const std::exception &e) {
        std::cerr << "gravastar_record: " << e.what() << '\n';
        return 2;
    }
}
