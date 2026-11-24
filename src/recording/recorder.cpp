// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/recording/recorder.hpp"
#include "gravastar/tools/process.hpp"
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

namespace gravastar::recording {
namespace fs = std::filesystem;
using tools::Json;

namespace {
constexpr int schema = 2;

std::string host_name() {
    char name[256]{};
    if (gethostname(name, sizeof name - 1) != 0)
        return "unknown";
    return name;
}
bool process_alive(long pid) {
    return pid > 0 && (kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM);
}
std::string key(std::uint64_t index) {
    std::ostringstream s;
    s << std::setfill('0') << std::setw(6) << index;
    return s.str();
}
// Leftover from tools::atomic_bytes: ".<name>.XXXXXX".
bool temporary_name(const std::string &name) {
    return name.size() > 8 && name[0] == '.' && name[name.size() - 7] == '.';
}
// "frame_NNNNNN.<ext>" -> index, or -1.
long long parse_frame_name(const std::string &name, const std::string &extension) {
    if (name.size() != 12 + extension.size() || name.compare(0, 6, "frame_") != 0 ||
        name.compare(12, extension.size(), extension) != 0)
        return -1;
    long long index = 0;
    for (std::size_t i = 6; i < 12; ++i) {
        if (name[i] < '0' || name[i] > '9')
            return -1;
        index = index * 10 + (name[i] - '0');
    }
    return index;
}
} // namespace

double TimelineConfig::time_at_frame(std::uint64_t i) const {
    // i * den is exact for any realistic job; divide once.
    return static_cast<double>(static_cast<long double>(i * fps.den) / fps.num);
}
Rational TimelineConfig::duration() const {
    std::uint64_t num = frame_count * fps.den, den = fps.num;
    const auto g = std::gcd(num, den);
    return {num / g, den / g};
}
std::string RecordingSession::frame_name(std::uint64_t index, const char *extension) {
    return "frame_" + key(index) + extension;
}

RecordingSession::RecordingSession(fs::path job_directory, TimelineConfig config, Json scene,
                                   FrameSource source, Options options)
    : dir_(std::move(job_directory)), config_(config), scene_(std::move(scene)),
      source_(std::move(source)), options_(options) {
    if (!source_)
        throw std::invalid_argument("recording requires a frame source");
    if (config_.width < 1 || config_.height < 1 || config_.width > 16384 ||
        config_.height > 16384 || config_.fps.num == 0 || config_.fps.den == 0 ||
        config_.frame_count < 1 || config_.frame_count > 10'000'000 ||
        config_.radiance_channels < 0 || config_.radiance_channels > 64)
        throw std::invalid_argument("invalid recording timeline");
    const auto g = std::gcd(config_.fps.num, config_.fps.den);
    config_.fps = {config_.fps.num / g, config_.fps.den / g};
    const auto d = config_.duration();
    // The content hash covers everything that determines the frames.
    const Json content{
        {"schema_version", schema},
        {"timeline",
         {{"width", config_.width},
          {"height", config_.height},
          {"fps", {config_.fps.num, config_.fps.den}},
          {"frame_count", config_.frame_count},
          {"duration_seconds", {d.num, d.den}},
          {"radiance_channels", config_.radiance_channels}}},
        {"frame_format", "PNG 8-bit RGB, no alpha, gAMA 1/2.2 (diagnostic display mapping)"},
        {"radiance_format", config_.radiance_channels
                                ? Json("NumPy .npy 1.0 <f8, shape (height, width, channels)")
                                : Json(nullptr)},
        {"scene", scene_}};
    hash_ = tools::sha256(content.dump());
    job_ = content;
    job_["content_sha256"] = hash_;
}

RecordingSession::~RecordingSession() { release_lock(); }

void RecordingSession::take_lock() {
    const auto path = dir_ / "job.lock";
    const Json mine{{"pid", static_cast<long>(getpid())}, {"host", host_name()}};
    for (int attempt = 0; attempt < 2; ++attempt) {
        const int fd = ::open(path.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0644);
        if (fd >= 0) {
            const auto text = mine.dump() + "\n";
            const bool ok = write(fd, text.data(), text.size()) == ssize_t(text.size()) &&
                            fsync(fd) == 0;
            close(fd);
            if (!ok) {
                fs::remove(path);
                throw std::runtime_error("cannot write job.lock");
            }
            locked_ = true;
            return;
        }
        if (errno != EEXIST)
            throw std::runtime_error("cannot create job.lock: errno " + std::to_string(errno));
        Json held;
        try {
            held = Json::parse(tools::read_file(path));
        } catch (const std::exception &) {
            throw std::runtime_error("job.lock exists and is unreadable; inspect it by hand");
        }
        const long pid = held.value("pid", 0L);
        const std::string host = held.value("host", std::string());
        if (host != host_name())
            throw std::runtime_error("job is locked by host " + host + " (pid " +
                                     std::to_string(pid) + ")");
        if (process_alive(pid))
            throw std::runtime_error("job is locked by running process " + std::to_string(pid));
        if (!options_.recover_stale_lock)
            throw std::runtime_error("stale job.lock from dead process " + std::to_string(pid) +
                                     "; reopen with recover_stale_lock to reclaim it");
        fs::create_directories(dir_ / "logs");
        std::ofstream(dir_ / "logs" / "locks.log", std::ios::app)
            << "reclaimed stale lock of dead pid " << pid << " on " << host << '\n';
        fs::remove(path);
        report_.stale_lock_recovered = true;
    }
    throw std::runtime_error("could not take job.lock");
}

void RecordingSession::release_lock() noexcept {
    if (!locked_)
        return;
    try {
        const auto path = dir_ / "job.lock";
        const auto held = Json::parse(tools::read_file(path));
        if (held.value("pid", 0L) == static_cast<long>(getpid()))
            fs::remove(path);
    } catch (...) {
    }
    locked_ = false;
}

void RecordingSession::save_progress() {
    const auto done = completed();
    tools::atomic_json(dir_ / "progress.json",
                       {{"schema_version", schema},
                        {"content_sha256", hash_},
                        {"frame_count", config_.frame_count},
                        {"completed", done},
                        {"state", done == config_.frame_count ? "complete" : "in_progress"},
                        {"frames", frames_}});
}

void RecordingSession::quarantine(const fs::path &file, const std::string &why) {
    const auto target_dir = dir_ / "logs" / "quarantine";
    fs::create_directories(target_dir);
    fs::path target;
    for (int n = 0;; ++n) {
        target = target_dir / (file.filename().string() + "." + std::to_string(n));
        if (!fs::exists(target))
            break;
    }
    fs::rename(file, target);
    std::ofstream(dir_ / "logs" / "quarantine.log", std::ios::app)
        << file.filename().string() << " -> " << target.filename().string() << ": " << why
        << '\n';
}

// Empty when the bytes are a valid frame of this job, else the reason.
std::string RecordingSession::verify_frame(std::uint64_t, const std::string &bytes) const {
    try {
        const auto image = decode_png(bytes);
        if (image.width != config_.width || image.height != config_.height)
            return "frame size differs from the timeline";
        return {};
    } catch (const std::exception &e) {
        return e.what();
    }
}

std::uint64_t RecordingSession::completed() const { return frames_.size(); }

ResumeReport RecordingSession::open() {
    if (opened_)
        throw std::logic_error("recording job already open");
    report_ = {};
    fs::create_directories(dir_ / "frames");
    fs::create_directories(dir_ / "logs");
    if (config_.radiance_channels)
        fs::create_directories(dir_ / "radiance");
    take_lock();
    try {
        const auto job_path = dir_ / "job.json";
        if (fs::exists(job_path)) {
            const auto existing = Json::parse(tools::read_file(job_path));
            if (existing.value("content_sha256", std::string()) != hash_)
                throw std::runtime_error(
                    "incompatible resume: the existing job has different settings or scene "
                    "(content hash " +
                    existing.value("content_sha256", std::string("?")) + ", requested " + hash_ +
                    "); create a new job directory");
        } else {
            tools::atomic_json(job_path, job_);
            report_.new_job = true;
        }
        frames_ = Json::object();
        const auto progress_path = dir_ / "progress.json";
        if (fs::exists(progress_path)) {
            const auto progress = Json::parse(tools::read_file(progress_path));
            if (progress.value("content_sha256", std::string()) == hash_)
                frames_ = progress.value("frames", Json::object());
        }
        // Leftover temporaries from interrupted atomic writes.
        for (const auto &sub : {dir_, dir_ / "frames", dir_ / "radiance"}) {
            if (!fs::exists(sub))
                continue;
            for (const auto &entry : fs::directory_iterator(sub))
                if (entry.is_regular_file() && temporary_name(entry.path().filename().string())) {
                    fs::remove(entry.path());
                    ++report_.temporaries;
                }
        }
        const bool radiance = config_.radiance_channels > 0;
        const std::size_t radiance_bytes =
            radiance ? encode_npy(config_.height, config_.width, config_.radiance_channels,
                                  std::vector<double>(std::size_t(config_.width) * config_.height *
                                                      config_.radiance_channels))
                           .size()
                     : 0;
        Json reconciled = Json::object();
        std::set<std::string> on_disk; // frame keys with a file, kept or not
        for (const auto &entry : fs::directory_iterator(dir_ / "frames")) {
            if (!entry.is_regular_file())
                continue;
            const auto name = entry.path().filename().string();
            const auto index = parse_frame_name(name, ".png");
            if (index < 0 || std::uint64_t(index) >= config_.frame_count) {
                quarantine(entry.path(), "not a frame of this timeline");
                ++report_.extra;
                continue;
            }
            const auto k = key(std::uint64_t(index));
            on_disk.insert(k);
            const auto bytes = tools::read_file(entry.path());
            const auto sha = tools::sha256(bytes);
            const auto radiance_path = dir_ / "radiance" / frame_name(std::uint64_t(index), ".npy");
            std::string radiance_sha;
            if (radiance && fs::exists(radiance_path)) {
                const auto r = tools::read_file(radiance_path);
                if (r.size() == radiance_bytes)
                    radiance_sha = tools::sha256(r);
            }
            const bool recorded = frames_.contains(k);
            if (recorded && frames_[k].value("png_sha256", std::string()) == sha &&
                (!radiance || frames_[k].value("radiance_sha256", std::string()) == radiance_sha)) {
                reconciled[k] = frames_[k];
                ++report_.kept;
                continue;
            }
            const auto problem = verify_frame(std::uint64_t(index), bytes);
            if (recorded || !problem.empty() || (radiance && radiance_sha.empty())) {
                // A recorded frame whose bytes changed, or a damaged/incomplete one.
                quarantine(entry.path(), recorded ? "contents differ from progress.json"
                                         : problem.empty() ? "radiance missing or damaged"
                                                           : problem);
                if (radiance && fs::exists(radiance_path))
                    quarantine(radiance_path, "its display frame was quarantined");
                ++report_.quarantined;
                continue;
            }
            // Crash window: renamed into place, checkpoint not yet written.
            Json record{{"png_sha256", sha}, {"png_bytes", bytes.size()}};
            if (radiance)
                record["radiance_sha256"] = radiance_sha;
            reconciled[k] = record;
            ++report_.adopted;
        }
        // Recorded frames whose file is gone (quarantined ones are counted
        // as quarantined, not missing).
        for (const auto &[k, value] : frames_.items())
            if (!on_disk.contains(k))
                ++report_.missing;
        if (radiance)
            for (const auto &entry : fs::directory_iterator(dir_ / "radiance")) {
                const auto index = parse_frame_name(entry.path().filename().string(), ".npy");
                if (index < 0 || !reconciled.contains(key(std::uint64_t(index)))) {
                    quarantine(entry.path(), "radiance without a recorded display frame");
                    ++report_.extra;
                }
            }
        frames_ = std::move(reconciled);
        save_progress();
    } catch (...) {
        release_lock();
        throw;
    }
    opened_ = true;
    return report_;
}

bool RecordingSession::render_next() {
    if (!opened_)
        throw std::logic_error("open() the recording job first");
    if (cancelled_)
        return false;
    std::uint64_t index = 0;
    while (index < config_.frame_count && frames_.contains(key(index)))
        ++index;
    if (index == config_.frame_count)
        return false;
    Frame frame;
    try {
        frame = source_(index, config_.time_at_frame(index));
    } catch (const RecordingCancelled &) {
        cancelled_ = true;
        return false;
    }
    if (frame.display.width != config_.width || frame.display.height != config_.height)
        throw std::runtime_error("frame source returned a frame of the wrong size");
    const std::size_t channels = std::size_t(config_.radiance_channels);
    if (frame.radiance.size() != std::size_t(config_.width) * config_.height * channels)
        throw std::runtime_error("frame source returned the wrong amount of radiance data");
    const auto png = encode_png(frame.display);
    Json record{{"png_sha256", tools::sha256(png)}, {"png_bytes", png.size()}};
    // Radiance first, display frame second, checkpoint last: a display frame
    // on disk always has its radiance, so a crash leaves a complete frame or
    // an orphan radiance file that open() quarantines.
    if (channels) {
        const auto npy = encode_npy(config_.height, config_.width, config_.radiance_channels,
                                    frame.radiance);
        tools::atomic_bytes(dir_ / "radiance" / frame_name(index, ".npy"), npy);
        record["radiance_sha256"] = tools::sha256(npy);
    }
    tools::atomic_bytes(dir_ / "frames" / frame_name(index, ".png"), png);
    frames_[key(index)] = record;
    save_progress();
    return completed() < config_.frame_count;
}

void RecordingSession::render_all() {
    while (render_next()) {
    }
}

EncodeReport RecordingSession::encode(const std::string &ffmpeg, const std::string &ffprobe) {
    EncodeReport out;
    if (!opened_)
        throw std::logic_error("open() the recording job first");
    auto fail = [&](std::string why) {
        out.failure = std::move(why);
        tools::atomic_json(dir_ / "logs" / "encode.json",
                           {{"success", false}, {"failure", out.failure}, {"probe", out.probe}});
        return out;
    };
    if (!complete())
        return fail("incomplete sequence: " + std::to_string(completed()) + " of " +
                    std::to_string(config_.frame_count) + " frames");
    if (config_.width % 2 || config_.height % 2)
        return fail("yuv420p video needs even width and height");
    // Only a complete, unchanged, gap-free sequence is encoded.
    std::size_t files = 0;
    for (const auto &entry : fs::directory_iterator(dir_ / "frames"))
        files += entry.is_regular_file();
    if (files != config_.frame_count)
        return fail("frames/ holds " + std::to_string(files) + " files; reopen the job to "
                    "reconcile extra or temporary files");
    for (std::uint64_t i = 0; i < config_.frame_count; ++i) {
        const auto path = dir_ / "frames" / frame_name(i, ".png");
        if (!fs::exists(path) ||
            tools::sha256(tools::read_file(path)) != frames_[key(i)].value("png_sha256", ""))
            return fail("frame " + std::to_string(i) +
                        " is missing or changed since it was recorded; reopen to reconcile");
    }
    const auto partial = dir_ / "video.partial.mp4", video = dir_ / "video.mp4";
    fs::remove(partial);
    const std::string rate = std::to_string(config_.fps.num) + "/" + std::to_string(config_.fps.den);
    const std::vector<std::string> args{
        ffmpeg, "-hide_banner", "-nostdin", "-y", "-framerate", rate, "-start_number", "0",
        "-i", (dir_ / "frames" / "frame_%06d.png").string(), "-frames:v",
        std::to_string(config_.frame_count), "-an", "-c:v", "libx264", "-preset", "medium",
        "-crf", "17", "-pix_fmt", "yuv420p", "-color_primaries", "bt709", "-color_trc", "bt709",
        "-colorspace", "bt709", "-movflags", "+faststart", "-f", "mp4", partial.string()};
    const auto log = (dir_ / "logs" / "ffmpeg.log").string();
    int code = 0;
    try {
        code = tools::run_capture(args, (dir_ / "logs" / "ffmpeg.stdout.log").string(), log);
    } catch (const std::exception &e) {
        fs::remove(partial);
        return fail(std::string("encoder could not start: ") + e.what());
    }
    if (code != 0) {
        fs::remove(partial);
        return fail("ffmpeg exited with " + std::to_string(code) + "; see logs/ffmpeg.log");
    }
    const auto probe_path = dir_ / "logs" / "ffprobe.json";
    try {
        code = tools::run_capture(
            {ffprobe, "-v", "error", "-count_frames", "-select_streams", "v:0", "-show_entries",
             "stream=codec_name,pix_fmt,width,height,r_frame_rate,nb_read_frames,duration:frame=pts",
             "-of",
             "json", partial.string()},
            probe_path.string(), (dir_ / "logs" / "ffprobe.err.log").string());
    } catch (const std::exception &e) {
        fs::remove(partial);
        return fail(std::string("ffprobe could not start: ") + e.what());
    }
    if (code != 0) {
        fs::remove(partial);
        return fail("ffprobe exited with " + std::to_string(code));
    }
    try {
        const auto parsed = Json::parse(tools::read_file(probe_path));
        out.probe = parsed.at("streams").at(0);
        // Presentation timestamps of the decoded frames must strictly increase.
        const auto &decoded = parsed.at("frames");
        bool monotonic = true;
        for (std::size_t i = 1; i < decoded.size(); ++i)
            monotonic = monotonic && decoded[i].at("pts").get<long long>() >
                                         decoded[i - 1].at("pts").get<long long>();
        out.probe["decoded_timestamps"] = decoded.size();
        out.probe["timestamps_monotonic"] = monotonic;
    } catch (const std::exception &e) {
        fs::remove(partial);
        return fail(std::string("unreadable ffprobe output: ") + e.what());
    }
    const auto &p = out.probe;
    const auto d = config_.duration();
    const double expected = double(d.num) / double(d.den), frame = double(config_.fps.den) / config_.fps.num;
    std::string problem;
    if (p.value("width", 0) != config_.width || p.value("height", 0) != config_.height)
        problem = "resolution";
    else if (p.value("r_frame_rate", std::string()) != rate)
        problem = "frame rate " + p.value("r_frame_rate", std::string());
    else if (p.value("nb_read_frames", std::string()) != std::to_string(config_.frame_count))
        problem = "decoded frame count " + p.value("nb_read_frames", std::string());
    else if (p.value("decoded_timestamps", 0u) != config_.frame_count ||
             !p.value("timestamps_monotonic", false))
        problem = "frame timestamps missing or not strictly increasing";
    else if (std::abs(std::stod(p.value("duration", std::string("0"))) - expected) > frame)
        problem = "duration " + p.value("duration", std::string());
    if (!problem.empty()) {
        fs::remove(partial);
        return fail("video check failed: " + problem);
    }
    fs::rename(partial, video);
    out.success = true;
    out.video = video.string();
    tools::atomic_json(dir_ / "logs" / "encode.json",
                       {{"success", true}, {"video", out.video}, {"probe", out.probe},
                        {"expected_duration_seconds", {d.num, d.den}}});
    return out;
}
} // namespace gravastar::recording
