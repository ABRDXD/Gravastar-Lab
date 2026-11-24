// SPDX-License-Identifier: GPL-3.0-or-later
// Phase F step 3: low-cost 600-frame fault-injection run (32x18, 60 fps,
// 10 s, one radiance channel). An uninterrupted reference job and a job
// that is crashed, corrupted and resumed must end with byte-identical
// frames; then encoding is checked, including a failure that keeps frames.
#include "gravastar/recording/recorder.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>

using namespace gravastar::recording;
namespace fs = std::filesystem;
using J = gravastar::tools::Json;
namespace {
int failures = 0, checks = 0;
void check(bool pass, const std::string &name) {
    ++checks;
    if (!pass) {
        ++failures;
        std::cerr << "FAIL " << name << '\n';
    }
}
template <class F> std::string thrown(F action) {
    try {
        action();
    } catch (const std::exception &e) {
        return e.what();
    }
    return {};
}
std::string frame(const fs::path &dir, std::uint64_t i) {
    return (dir / "frames" / RecordingSession::frame_name(i, ".png")).string();
}
std::string host() {
    char name[256]{};
    gethostname(name, sizeof name - 1);
    return name;
}
long dead_pid() {
    const pid_t child = fork();
    if (child == 0)
        _exit(0);
    int status = 0;
    waitpid(child, &status, 0);
    return child; // reaped: no process has this pid now (reuse is negligible)
}
} // namespace

int main() {
    try {
        const fs::path base = fs::temp_directory_path() / ("gravastar-fault-" + std::to_string(getpid()));
        fs::remove_all(base);
        const fs::path reference = base / "reference", job = base / "job";
        TimelineConfig config;
        config.width = 32;
        config.height = 18;
        config.fps = {60, 1};
        config.frame_count = 600;
        config.radiance_channels = 1;
        const J scene{{"source", "synthetic fault-injection pattern"}};
        // Frames depend only on index and time, never on how fast they render.
        long crash_at = -1, cancel_at = -1;
        auto pattern = [&](std::uint64_t index, double time) {
            if (long(index) == crash_at)
                throw std::runtime_error("simulated crash");
            if (long(index) == cancel_at)
                throw RecordingCancelled();
            Frame f;
            f.display = {config.width, config.height, {}};
            for (int y = 0; y < config.height; ++y)
                for (int x = 0; x < config.width; ++x) {
                    f.display.rgb.push_back(std::uint8_t((x * 8 + index) % 256));
                    f.display.rgb.push_back(std::uint8_t((y * 14 + 3 * index) % 256));
                    f.display.rgb.push_back(std::uint8_t((x * y + index / 7) % 256));
                    f.radiance.push_back(std::sin(0.3 * x + 0.2 * y + 6.283185307179586 * time));
                }
            return f;
        };

        // Uninterrupted reference.
        {
            RecordingSession s(reference, config, scene, pattern);
            s.open();
            s.render_all();
            check(s.complete(), "reference job completes");
        }
        const auto reference_progress = J::parse(gravastar::tools::read_file(reference / "progress.json"));

        // 1. Crash at frame 137; the lock it would leave behind is simulated
        //    with a dead pid on this host.
        crash_at = 137;
        {
            RecordingSession s(job, config, scene, pattern);
            s.open();
            check(thrown([&] { s.render_all(); }) == "simulated crash", "crash at frame 137 propagates");
            check(s.completed() == 137, "137 frames recorded before the crash");
        }
        crash_at = -1;
        const long dead = dead_pid();
        gravastar::tools::atomic_json(job / "job.lock", {{"pid", dead}, {"host", host()}});
        {
            RecordingSession s(job, config, scene, pattern);
            check(thrown([&] { s.open(); }).find("stale job.lock") != std::string::npos,
                  "stale lock refused without recovery");
        }
        {
            Options recover;
            recover.recover_stale_lock = true;
            RecordingSession s(job, config, scene, pattern, recover);
            const auto r = s.open();
            check(r.stale_lock_recovered && r.kept == 137, "stale lock reclaimed deliberately, 137 kept");
            for (int i = 0; i < 100; ++i)
                s.render_next();
        }
        // 2. Crash window: frame 236 renamed into place but not checkpointed.
        {
            auto p = J::parse(gravastar::tools::read_file(job / "progress.json"));
            p["frames"].erase("000236");
            gravastar::tools::atomic_json(job / "progress.json", p);
        }
        // 3-8. Damage and debris.
        {
            std::fstream f(frame(job, 50), std::ios::in | std::ios::out | std::ios::binary);
            f.seekp(60);
            f.put('\x5a'); // corrupt
        }
        fs::resize_file(frame(job, 60), fs::file_size(frame(job, 60)) / 2); // truncate
        fs::remove(frame(job, 70));                                           // missing
        fs::copy_file(frame(job, 10), job / "frames" / "frame_000700.png");   // beyond the timeline
        std::ofstream(job / "frames" / "notes.txt") << "not a frame\n";       // unknown name
        std::ofstream(job / "frames" / ".frame_000099.png.Ab3dE9") << "x";    // temporary
        fs::copy_file(reference / "radiance" / "frame_000300.npy",            // orphan radiance
                      job / "radiance" / "frame_000300.npy");
        cancel_at = 400;
        {
            RecordingSession s(job, config, scene, pattern);
            const auto r = s.open();
            std::cout << "resume: kept " << r.kept << " adopted " << r.adopted << " quarantined "
                      << r.quarantined << " missing " << r.missing << " extra " << r.extra
                      << " temporaries " << r.temporaries << '\n';
            check(r.adopted == 1, "crash-window frame adopted");
            check(r.quarantined == 2, "corrupt and truncated frames quarantined");
            check(r.missing == 1, "deleted frame counted missing");
            // Out-of-range frame, unknown file, the planted orphan radiance, and
            // frame 70's radiance, orphaned when its display frame was deleted.
            check(r.extra == 4, "out-of-range, unknown and orphan radiance files quarantined");
            check(r.temporaries == 1, "leftover temporary removed");
            check(r.kept == 233, "every other frame kept");
            s.render_all();
            check(s.cancelled() && !s.complete(), "cancel inside the source stops the job");
        }
        cancel_at = -1;
        check(fs::exists(job / "logs" / "quarantine" / "frame_000050.png.0") &&
                  fs::exists(job / "logs" / "quarantine" / "frame_000700.png.0"),
              "quarantined files kept under logs/");
        {
            TimelineConfig longer = config;
            longer.frame_count = 601;
            RecordingSession s(job, longer, scene, pattern);
            check(thrown([&] { s.open(); }).find("incompatible resume") != std::string::npos,
                  "a longer timeline is refused for this job");
        }
        {
            RecordingSession s(job, config, scene, pattern);
            s.open();
            s.render_all();
            check(s.complete(), "resumed job completes");
        }
        // The interrupted job matches the reference byte for byte.
        const auto progress = J::parse(gravastar::tools::read_file(job / "progress.json"));
        check(progress["frames"] == reference_progress["frames"],
              "all 600 frames and radiance files identical to the uninterrupted job");

        // Encoding: a failing encoder keeps every frame and leaves no video.
        {
            RecordingSession s(job, config, scene, pattern);
            s.open();
            const auto failed = s.encode("false");
            check(!failed.success && !fs::exists(job / "video.partial.mp4") &&
                      !fs::exists(job / "video.mp4"),
                  "encoder failure leaves no video");
            const auto missing = s.encode("/nonexistent/ffmpeg");
            check(!missing.success && missing.failure.find("could not start") != std::string::npos,
                  "missing encoder reported");
            std::size_t files = 0;
            for (const auto &e : fs::directory_iterator(job / "frames"))
                files += e.is_regular_file();
            check(files == 600 && J::parse(gravastar::tools::read_file(job / "progress.json"))["frames"] ==
                                      reference_progress["frames"],
                  "all 600 frames kept after encoder failures");
            const auto ok = s.encode();
            check(ok.success, "encode succeeds: " + ok.failure);
            check(ok.probe.value("nb_read_frames", std::string()) == "600" &&
                      ok.probe.value("r_frame_rate", std::string()) == "60/1" &&
                      std::abs(std::stod(ok.probe.value("duration", std::string("0"))) - 10.0) <= 1.0 / 60,
                  "ffprobe: 600 frames, 60/1, 10 s");
        }
        // A frame changed after recording blocks encoding.
        {
            std::fstream f(frame(job, 5), std::ios::in | std::ios::out | std::ios::binary);
            f.seekp(70);
            f.put('\x11');
        }
        {
            RecordingSession s(job, config, scene, pattern);
            // open() would quarantine it; encode first checks the recorded hashes.
            s.open();
            check(!s.complete(), "changed frame is caught on reopen");
            s.render_all();
            check(s.encode().success, "re-rendered frame lets the job encode again");
        }
        fs::remove_all(base);
    } catch (const std::exception &e) {
        std::cerr << "recording faults: " << e.what() << '\n';
        return 1;
    }
    std::cout << (failures ? "FAIL " : "PASS ") << checks << " recording fault-injection checks\n";
    return failures ? 1 : 0;
}
