// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "gravastar/geometry/geometry.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>

namespace gravastar::recording {

struct FrameRational {
    std::uint64_t num{60};
    std::uint64_t den{1};

    double to_double() const noexcept {
        return den ? static_cast<double>(num) / static_cast<double>(den) : 0.0;
    }
};

struct TimelineConfig {
    int width{128};
    int height{128};
    FrameRational fps{60, 1};
    double duration_seconds{10.0}; // Default 10 seconds (600 frames at 60 fps)
    
    // Orbital camera parameters
    double camera_r{10.0};
    double camera_theta{1.25};
    double camera_phi_start{0.0};
    double orbit_speed_rad_per_sec{0.2}; // Camera orbits azimuthally
    double fov_deg{60.0};
    bool fast_test_pattern{false}; // Low-cost deterministic test pattern to keep machine cool

    [[nodiscard]] std::uint64_t frame_count() const noexcept {
        return static_cast<std::uint64_t>(duration_seconds * fps.to_double() + 0.5);
    }

    [[nodiscard]] double time_at_frame(std::uint64_t frame_idx) const noexcept {
        return static_cast<double>(frame_idx) * static_cast<double>(fps.den) / static_cast<double>(fps.num);
    }
};

struct JobProgress {
    std::uint64_t completed_frames{0};
    std::uint64_t total_frames{0};
    std::string status{"idle"};
    bool cancelled{false};
};

class RecordingSession {
public:
    RecordingSession(std::filesystem::path job_directory,
                     TimelineConfig config,
                     geometry::Parameters geom_params);

    // Initialize job manifest or inspect existing directory for resumption
    void initialize();

    // Render next frame (returns true if more frames remain, false when finished or cancelled)
    bool render_frame();

    // Render all frames in sequence until complete
    void render_all();

    // Encode completed frames to MP4 using external FFmpeg and verify with ffprobe
    bool encode_video();

    [[nodiscard]] const JobProgress& progress() const noexcept { return progress_; }
    void cancel() noexcept { progress_.cancelled = true; }
    [[nodiscard]] const std::filesystem::path& job_directory() const noexcept { return job_dir_; }

private:
    std::filesystem::path job_dir_;
    TimelineConfig config_;
    geometry::Parameters geom_;
    JobProgress progress_;
};

} // namespace gravastar::recording
