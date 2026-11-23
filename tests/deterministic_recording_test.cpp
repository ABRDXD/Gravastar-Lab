// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/recording/recorder.hpp"
#include "gravastar/tools/testing.hpp"
#include <iostream>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>

using namespace gravastar;
using namespace gravastar::recording;
namespace fs = std::filesystem;

int main() {
    std::cout << "=== GravastarLab Deterministic Recording & Video Timeline Test ===\n";

    fs::path test_dir = "results/test_recording_session";
    fs::remove_all(test_dir);

    TimelineConfig config;
    config.width = 64;
    config.height = 64;
    config.fps = {30, 1};
    config.duration_seconds = 1.0; // 30 frames total

    // Synthetic deterministic pattern: no rendering, so this exercises only
    // scheduling, persistence, resume and encoding.
    const tools::Json scene = {{"source", "synthetic gradient test pattern"}};
    const FrameSource pattern = [&](std::uint64_t index, double) {
        std::string bytes = "P6\n" + std::to_string(config.width) + " " +
                            std::to_string(config.height) + "\n255\n";
        for (int y = 0; y < config.height; ++y)
            for (int x = 0; x < config.width; ++x) {
                bytes.push_back(static_cast<char>(x * 255 / (config.width - 1)));
                bytes.push_back(static_cast<char>(y * 255 / (config.height - 1)));
                bytes.push_back(static_cast<char>((index * 7) % 256));
            }
        return bytes;
    };

    GRAVASTAR_REQUIRE(config.frame_count() == 30, "Frame count calculation mismatch");

    // Phase 1: Fault Injection - render only 10 frames and simulate cancellation
    std::cout << "[Step 1] Initializing job and rendering 10 frames...\n";
    {
        RecordingSession session1(test_dir, config, scene, pattern);
        session1.initialize();
        GRAVASTAR_REQUIRE(session1.progress().completed_frames == 0, "Initial completed frames must be 0");

        for (int i = 0; i < 10; ++i) {
            bool cont = session1.render_frame();
            GRAVASTAR_REQUIRE(cont == true, "Session must indicate continuation before frame 10");
        }
        GRAVASTAR_REQUIRE(session1.progress().completed_frames == 10, "Expected 10 completed frames");

        session1.cancel();
        bool stopped = session1.render_frame();
        GRAVASTAR_REQUIRE(stopped == false, "Cancelled session must return false from render_frame");
        GRAVASTAR_REQUIRE(session1.progress().status == "cancelled", "Session status must be cancelled");
        std::cout << "Simulated interruption successfully at frame 10.\n";
    }

    // Verify disk state after interruption
    for (int i = 0; i < 10; ++i) {
        std::ostringstream ss;
        ss << "frame_" << std::setfill('0') << std::setw(6) << i << ".ppm";
        GRAVASTAR_REQUIRE(fs::exists(test_dir / "frames" / ss.str()), "Interrupted frame file must exist on disk");
    }
    GRAVASTAR_REQUIRE(!fs::exists(test_dir / "frames" / "frame_000010.ppm"), "Frame 10 must not exist after cancellation");

    // Phase 2: Resumption from Checkpoint
    std::cout << "[Step 2] Resuming job from existing checkpoint...\n";
    {
        RecordingSession session2(test_dir, config, scene, pattern);
        session2.initialize();
        std::cout << "Detected existing completed frames: " << session2.progress().completed_frames << "\n";
        GRAVASTAR_REQUIRE(session2.progress().completed_frames == 10, "Resumed session must detect exactly 10 existing frames");

        // Resume and render remaining 20 frames
        session2.render_all();
        std::cout << "Render finished. Total completed: " << session2.progress().completed_frames << "\n";
        GRAVASTAR_REQUIRE(session2.progress().completed_frames == 30, "Resumed session must finish all 30 frames");
        GRAVASTAR_REQUIRE(session2.progress().status == "completed", "Session status must be completed");

        // Phase 3: Video Encoding with FFmpeg & Container Verification
        std::cout << "[Step 3] Encoding to video.mp4 via FFmpeg and verifying...\n";
        bool encoded = session2.encode_video();
        GRAVASTAR_REQUIRE(encoded == true, "encode_video() must return true on success");
        GRAVASTAR_REQUIRE(fs::exists(test_dir / "video.mp4"), "video.mp4 must exist after encoding");
        GRAVASTAR_REQUIRE(fs::file_size(test_dir / "video.mp4") > 1000, "video.mp4 must not be an empty or truncated file");
        GRAVASTAR_REQUIRE(fs::exists(test_dir / "manifest.json"), "manifest.json must exist");
        GRAVASTAR_REQUIRE(!fs::exists(test_dir / "video.partial.mp4"), "video.partial.mp4 must be cleanly renamed");
        std::cout << "Video export and verification succeeded!\n";
    }

    // A mis-sized frame from the source is rejected before anything is written.
    {
        const fs::path bad_dir = test_dir.string() + "_malformed";
        fs::remove_all(bad_dir);
        RecordingSession bad(bad_dir, config, scene,
                             [](std::uint64_t, double) { return std::string("P6\n1 1\n255\nabc"); });
        bad.initialize();
        bool rejected = false;
        try {
            (void)bad.render_frame();
        } catch (const std::runtime_error &) {
            rejected = true;
        }
        GRAVASTAR_REQUIRE(rejected, "Malformed frame must be rejected");
        GRAVASTAR_REQUIRE(!fs::exists(bad_dir / "frames" / "frame_000000.ppm"),
                          "Rejected frame must not be written");
        fs::remove_all(bad_dir);
    }

    std::cout << "\nALL DETERMINISTIC RECORDING & RESUMPTION CHECKS PASSED!\n";
    return 0;
}
