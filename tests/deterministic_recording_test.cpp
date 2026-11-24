// SPDX-License-Identifier: GPL-3.0-or-later
// Recording job lifecycle on a synthetic 64x64, 30-frame job with radiance:
// cancel, resume, finish, encode and verify; size, settings and lock checks.
// The 600-frame fault-injection run is tests/recording_fault_tests.cpp.
#include "gravastar/recording/recorder.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

using namespace gravastar::recording;
namespace fs = std::filesystem;
namespace {
int failures = 0, checks = 0;
void check(bool pass, const std::string &name) {
    ++checks;
    if (!pass) {
        ++failures;
        std::cerr << "FAIL " << name << '\n';
    }
}
template <class F> bool throws(F action) {
    try {
        action();
    } catch (const std::exception &) {
        return true;
    }
    return false;
}
} // namespace

int main() {
    try {
        const fs::path dir = fs::temp_directory_path() / ("gravastar-recording-" + std::to_string(getpid()));
        fs::remove_all(dir);
        TimelineConfig config;
        config.width = 64;
        config.height = 64;
        config.fps = {30, 1};
        config.frame_count = 30;
        config.radiance_channels = 2;
        const gravastar::tools::Json scene{{"source", "synthetic gradient test pattern"}};
        int calls = 0;
        const FrameSource pattern = [&](std::uint64_t index, double time) {
            ++calls;
            Frame f;
            f.display = {config.width, config.height, {}};
            for (int y = 0; y < config.height; ++y)
                for (int x = 0; x < config.width; ++x) {
                    f.display.rgb.push_back(std::uint8_t(x * 255 / (config.width - 1)));
                    f.display.rgb.push_back(std::uint8_t(y * 255 / (config.height - 1)));
                    f.display.rgb.push_back(std::uint8_t((index * 7) % 256));
                    f.radiance.push_back(std::sin(0.1 * x + time));
                    f.radiance.push_back(double(index));
                }
            return f;
        };
        check(config.time_at_frame(15) == 0.5, "frame 15 at 30 fps is 0.5 s exactly");
        check(config.duration().num == 1 && config.duration().den == 1, "30 frames at 30 fps last 1 s");
        {
            RecordingSession s(dir, config, scene, pattern);
            const auto r = s.open();
            check(r.new_job && s.completed() == 0, "new job starts empty");
            for (int i = 0; i < 10; ++i)
                s.render_next();
            check(s.completed() == 10, "ten frames rendered");
            // A second writer is refused while this one holds the lock.
            RecordingSession other(dir, config, scene, pattern);
            check(throws([&] { other.open(); }), "concurrent writer refused");
            s.cancel();
            check(!s.render_next() && s.cancelled(), "cancel stops at the frame boundary");
        }
        check(fs::exists(dir / "frames" / "frame_000009.png") &&
                  !fs::exists(dir / "frames" / "frame_000010.png") &&
                  fs::exists(dir / "radiance" / "frame_000009.npy") && !fs::exists(dir / "job.lock"),
              "ten frames and their radiance on disk, lock released");
        {
            TimelineConfig changed = config;
            changed.fps = {25, 1};
            RecordingSession s(dir, changed, scene, pattern);
            check(throws([&] { s.open(); }), "resume with different settings refused");
        }
        {
            RecordingSession s(dir, config, {{"source", "something else"}}, pattern);
            check(throws([&] { s.open(); }), "resume with a different scene refused");
        }
        calls = 0;
        {
            RecordingSession s(dir, config, scene, pattern);
            const auto r = s.open();
            check(!r.new_job && r.kept == 10 && r.quarantined == 0, "resume keeps the ten verified frames");
            s.render_all();
            check(s.complete() && calls == 20, "resume renders only the twenty missing frames");
            const auto e = s.encode();
            check(e.success, "encode succeeds: " + e.failure);
            check(e.probe.value("width", 0) == 64 && e.probe.value("height", 0) == 64 &&
                      e.probe.value("r_frame_rate", std::string()) == "30/1" &&
                      e.probe.value("nb_read_frames", std::string()) == "30",
                  "ffprobe sees 64x64, 30/1, 30 frames");
            check(fs::exists(dir / "video.mp4") && !fs::exists(dir / "video.partial.mp4"),
                  "verified video renamed into place");
        }
        {
            const fs::path bad_dir = dir.string() + "-bad";
            fs::remove_all(bad_dir);
            RecordingSession bad(bad_dir, config, scene, [&](std::uint64_t, double) {
                Frame f;
                f.display = {1, 1, {1, 2, 3}};
                return f;
            });
            bad.open();
            check(throws([&] { bad.render_next(); }), "wrong-size frame rejected");
            check(!fs::exists(bad_dir / "frames" / "frame_000000.png"), "rejected frame not written");
            fs::remove_all(bad_dir);
        }
        fs::remove_all(dir);
    } catch (const std::exception &e) {
        std::cerr << "recording lifecycle: " << e.what() << '\n';
        return 1;
    }
    std::cout << (failures ? "FAIL " : "PASS ") << checks << " recording lifecycle checks\n";
    return failures ? 1 : 0;
}
