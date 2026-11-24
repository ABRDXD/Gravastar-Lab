// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/recording/recorder.hpp"
#include "gravastar/tools/artifacts.hpp"
#include "gravastar/tools/process.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace gravastar::recording {

namespace fs = std::filesystem;
using tools::Json;

namespace {

std::string format_frame_filename(std::uint64_t idx) {
    std::ostringstream ss;
    ss << "frame_" << std::setfill('0') << std::setw(6) << idx << ".ppm";
    return ss.str();
}

// A frame must be exactly "P6\n<width> <height>\n255\n" plus width*height*3
// bytes, so a truncated or mis-sized frame is rejected before it is written.
void require_ppm(const std::string &bytes, int width, int height) {
    const std::string header = "P6\n" + std::to_string(width) + " " +
                               std::to_string(height) + "\n255\n";
    const auto expected = header.size() +
                          static_cast<std::size_t>(width) * height * 3;
    if (bytes.size() != expected || bytes.compare(0, header.size(), header) != 0)
        throw std::runtime_error("frame source returned a malformed or mis-sized PPM");
}

} // namespace

RecordingSession::RecordingSession(fs::path job_directory,
                                   TimelineConfig config,
                                   Json scene,
                                   FrameSource source)
    : job_dir_(std::move(job_directory)), config_(config),
      scene_(std::move(scene)), source_(std::move(source)) {
    if (!source_)
        throw std::invalid_argument("recording requires a frame source");
    if (config_.width < 1 || config_.height < 1 || config_.fps.num == 0 ||
        config_.fps.den == 0 || !(config_.duration_seconds > 0))
        throw std::invalid_argument("invalid recording timeline");
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
        {"scene", scene_}
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

    const std::string bytes = source_(frame_idx, t);
    require_ppm(bytes, config_.width, config_.height);

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
