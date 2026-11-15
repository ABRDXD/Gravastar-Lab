// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/recording/recorder.hpp"
#include "gravastar/optics/renderer.hpp"
#include "gravastar/tools/artifacts.hpp"
#include "gravastar/tools/process.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace gravastar::recording {

namespace fs = std::filesystem;
using tools::Json;

namespace {

std::string format_frame_filename(std::uint64_t idx) {
    std::ostringstream ss;
    ss << "frame_" << std::setfill('0') << std::setw(6) << idx << ".ppm";
    return ss.str();
}

std::string serialize_image_ppm(const optics::ImageBuffer& buffer) {
    std::string bytes;
    std::string header = "P6\n" + std::to_string(buffer.width) + " " +
                         std::to_string(buffer.height) + "\n255\n";
    bytes.reserve(header.size() + buffer.width * buffer.height * 3);
    bytes.append(header);

    for (int y = 0; y < buffer.height; ++y) {
        for (int x = 0; x < buffer.width; ++x) {
            const float* p = buffer.at(x, y);
            uint8_t r = static_cast<uint8_t>(std::clamp(p[0], 0.0f, 1.0f) * 255.0f + 0.5f);
            uint8_t g = static_cast<uint8_t>(std::clamp(p[1], 0.0f, 1.0f) * 255.0f + 0.5f);
            uint8_t b = static_cast<uint8_t>(std::clamp(p[2], 0.0f, 1.0f) * 255.0f + 0.5f);
            bytes.push_back(static_cast<char>(r));
            bytes.push_back(static_cast<char>(g));
            bytes.push_back(static_cast<char>(b));
        }
    }
    return bytes;
}

} // namespace

RecordingSession::RecordingSession(fs::path job_directory,
                                   TimelineConfig config,
                                   geometry::Parameters geom_params)
    : job_dir_(std::move(job_directory)), config_(config), geom_(geom_params) {
    progress_.total_frames = config_.frame_count();
}

void RecordingSession::initialize() {
    fs::create_directories(job_dir_ / "frames");

    fs::path job_json_path = job_dir_ / "job.json";
    Json job_meta = {
        {"schema_version", 1},
        {"width", config_.width},
        {"height", config_.height},
        {"fps_num", config_.fps.num},
        {"fps_den", config_.fps.den},
        {"duration_seconds", config_.duration_seconds},
        {"total_frames", config_.frame_count()},
        {"camera", {
            {"r", config_.camera_r},
            {"theta", config_.camera_theta},
            {"phi_start", config_.camera_phi_start},
            {"orbit_speed", config_.orbit_speed_rad_per_sec},
            {"fov_deg", config_.fov_deg}
        }},
        {"geometry", {
            {"M", geom_.mass},
            {"a", geom_.spin},
            {"Lambda", geom_.cosmological_constant},
            {"r_in", geom_.r_in},
            {"r_out", geom_.r_out}
        }}
    };
    tools::atomic_json(job_json_path, job_meta);

    // Inspect existing frames for resumption
    std::uint64_t found_frames = 0;
    while (found_frames < config_.frame_count()) {
        fs::path frame_path = job_dir_ / "frames" / format_frame_filename(found_frames);
        if (!fs::exists(frame_path) || fs::file_size(frame_path) == 0) {
            break;
        }
        ++found_frames;
    }

    progress_.completed_frames = found_frames;
    progress_.status = (found_frames == config_.frame_count()) ? "completed" : "in_progress";

    Json prog_meta = {
        {"completed_frames", progress_.completed_frames},
        {"total_frames", progress_.total_frames},
        {"status", progress_.status}
    };
    tools::atomic_json(job_dir_ / "progress.json", prog_meta);
}

bool RecordingSession::render_frame() {
    if (progress_.cancelled) {
        progress_.status = "cancelled";
        return false;
    }

    if (progress_.completed_frames >= config_.frame_count()) {
        progress_.status = "completed";
        return false;
    }

    std::uint64_t frame_idx = progress_.completed_frames;
    double t = config_.time_at_frame(frame_idx);

    std::string bytes;
    if (config_.fast_test_pattern) {
        // Zero-cost deterministic test pattern (preserves thermal margins during tests)
        std::string header = "P6\n" + std::to_string(config_.width) + " " +
                             std::to_string(config_.height) + "\n255\n";
        bytes.reserve(header.size() + config_.width * config_.height * 3);
        bytes.append(header);
        for (int y = 0; y < config_.height; ++y) {
            for (int x = 0; x < config_.width; ++x) {
                uint8_t r = static_cast<uint8_t>(x * 255 / std::max(config_.width - 1, 1));
                uint8_t g = static_cast<uint8_t>(y * 255 / std::max(config_.height - 1, 1));
                uint8_t b = static_cast<uint8_t>((frame_idx * 7) % 256);
                bytes.push_back(static_cast<char>(r));
                bytes.push_back(static_cast<char>(g));
                bytes.push_back(static_cast<char>(b));
            }
        }
    } else {
        optics::RenderSettings settings;
        settings.camera.width = config_.width;
        settings.camera.height = config_.height;
        settings.camera.r = config_.camera_r;
        settings.camera.theta = config_.camera_theta;
        settings.camera.phi = config_.camera_phi_start + config_.orbit_speed_rad_per_sec * t;
        settings.camera.fov_deg = config_.fov_deg;

        settings.integrator.max_steps = 1500;
        settings.integrator.h0 = 0.02;
        settings.integrator.escape_radius = 25.0;

        optics::ImageBuffer buffer = optics::render_scene(geom_, settings);
        bytes = serialize_image_ppm(buffer);
    }

    fs::path frame_path = job_dir_ / "frames" / format_frame_filename(frame_idx);
    tools::atomic_bytes(frame_path, bytes);

    ++progress_.completed_frames;
    progress_.status = (progress_.completed_frames >= config_.frame_count()) ? "completed" : "in_progress";

    Json prog_meta = {
        {"completed_frames", progress_.completed_frames},
        {"total_frames", progress_.total_frames},
        {"status", progress_.status}
    };
    tools::atomic_json(job_dir_ / "progress.json", prog_meta);

    return progress_.completed_frames < config_.frame_count();
}

void RecordingSession::render_all() {
    while (render_frame()) {
    }
}

bool RecordingSession::encode_video() {
    if (progress_.completed_frames < config_.frame_count()) {
        throw std::runtime_error("Cannot encode video: incomplete frame sequence (" +
                                 std::to_string(progress_.completed_frames) + "/" +
                                 std::to_string(config_.frame_count()) + ")");
    }

    progress_.status = "encoding";
    fs::path video_path = job_dir_ / "video.mp4";
    fs::path video_partial = job_dir_ / "video.partial.mp4";
    fs::remove(video_partial);

    std::uint64_t fps_val = config_.fps.den ? (config_.fps.num / config_.fps.den) : 60;

    std::vector<std::string> ffmpeg_args = {
        "ffmpeg",
        "-hide_banner",
        "-nostdin",
        "-v", "error",
        "-y",
        "-framerate", std::to_string(fps_val),
        "-start_number", "0",
        "-i", (job_dir_ / "frames" / "frame_%06d.ppm").string(),
        "-frames:v", std::to_string(config_.frame_count()),
        "-an",
        "-c:v", "libx264",
        "-preset", "ultrafast",
        "-crf", "17",
        "-pix_fmt", "yuv420p",
        "-movflags", "+faststart",
        video_partial.string()
    };

    int code = tools::run(ffmpeg_args);
    if (code != 0) {
        progress_.status = "encoding_failed";
        throw std::runtime_error("FFmpeg encode failed with exit code: " + std::to_string(code));
    }

    // Atomic rename of encoded movie
    fs::rename(video_partial, video_path);

    // Probe with ffprobe
    std::vector<std::string> probe_args = {
        "ffprobe",
        "-v", "error",
        "-select_streams", "v:0",
        "-show_entries", "stream=width,height,r_frame_rate,nb_frames",
        "-of", "default=noprint_wrappers=1",
        video_path.string()
    };
    int probe_code = tools::run(probe_args);
    if (probe_code != 0) {
        throw std::runtime_error("ffprobe validation failed on output container");
    }

    progress_.status = "finished";
    Json manifest_meta = {
        {"status", "verified"},
        {"video_file", "video.mp4"},
        {"width", config_.width},
        {"height", config_.height},
        {"fps", fps_val},
        {"frames", config_.frame_count()},
        {"duration_sec", config_.duration_seconds}
    };
    tools::atomic_json(job_dir_ / "manifest.json", manifest_meta);
    return true;
}

} // namespace gravastar::recording
