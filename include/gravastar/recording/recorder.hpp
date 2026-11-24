// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Resumable, deterministic recording jobs (PLAN.md "Deterministic recording
// specification"; docs/RECORDING.md).
//
// Job directory:
//   job.json        frozen timeline, scene description, formats, content hash
//   progress.json   per-frame SHA-256 and sizes, written atomically per frame
//   job.lock        held by the one writer (pid, host)
//   frames/frame_000000.png       8-bit RGB display frames (image_io.hpp)
//   radiance/frame_000000.npy     optional float64 scientific output
//   logs/           encoder logs, lock recoveries, quarantined files
//   video.mp4       written only by encode(), after ffprobe verification
#include "gravastar/recording/image_io.hpp"
#include "gravastar/tools/artifacts.hpp"
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace gravastar::recording {

struct Rational {
    std::uint64_t num{60}, den{1};
};

struct TimelineConfig {
    int width{1920}, height{1080};
    Rational fps{60, 1};
    std::uint64_t frame_count{600};
    int radiance_channels{0}; // 0: no scientific output
    // Exact frame time i * den / num, converted to double only at the end.
    [[nodiscard]] double time_at_frame(std::uint64_t i) const;
    // Duration frame_count * den / num seconds, as a rational.
    [[nodiscard]] Rational duration() const;
};

struct Frame {
    Rgb8Image display;
    std::vector<double> radiance; // height * width * radiance_channels, or empty
};
// Produces the complete frame for a timeline index. Its result must depend
// only on the index and time (never on wall-clock or preview rate). It may
// throw RecordingCancelled to abandon the frame.
using FrameSource = std::function<Frame(std::uint64_t index, double time_seconds)>;
struct RecordingCancelled : std::runtime_error {
    RecordingCancelled() : std::runtime_error("recording cancelled") {}
};

struct Options {
    // A lock left by a dead process on this host is reclaimed only when set.
    bool recover_stale_lock{false};
};

struct ResumeReport {
    std::size_t kept{};          // recorded and verified
    std::size_t adopted{};       // complete file not yet recorded (crash window)
    std::size_t quarantined{};   // corrupt, truncated, mis-sized or mismatched
    std::size_t missing{};       // recorded but file gone
    std::size_t extra{};         // outside the timeline or unknown names
    std::size_t temporaries{};   // leftover temporary files removed
    bool new_job{};
    bool stale_lock_recovered{};
};

struct EncodeReport {
    bool success{};
    std::string video;           // path of the verified video
    std::string failure;         // why it failed; frames are always kept
    tools::Json probe;           // what ffprobe reported
};

class RecordingSession {
  public:
    // Throws if the directory holds a job with a different content hash.
    RecordingSession(std::filesystem::path job_directory, TimelineConfig config,
                     tools::Json scene, FrameSource source, Options options = {});
    ~RecordingSession(); // releases the lock if held
    RecordingSession(const RecordingSession &) = delete;
    RecordingSession &operator=(const RecordingSession &) = delete;

    // Lock, create or verify job.json, reconcile files with progress.json.
    ResumeReport open();
    // Renders the lowest missing frame. False when complete or cancelled.
    bool render_next();
    void render_all();
    // Takes effect at the next frame boundary (or inside the source).
    void cancel() noexcept { cancelled_ = true; }
    // Verifies the complete sequence, encodes to a temporary file, checks it
    // with ffprobe, then renames it. Frames are never removed.
    EncodeReport encode(const std::string &ffmpeg = "ffmpeg",
                        const std::string &ffprobe = "ffprobe");

    [[nodiscard]] std::uint64_t completed() const;
    [[nodiscard]] bool complete() const { return completed() == config_.frame_count; }
    [[nodiscard]] bool cancelled() const noexcept { return cancelled_; }
    [[nodiscard]] const std::string &content_sha256() const noexcept { return hash_; }
    [[nodiscard]] const std::filesystem::path &directory() const noexcept { return dir_; }
    static std::string frame_name(std::uint64_t index, const char *extension);

  private:
    void take_lock();
    void release_lock() noexcept;
    void save_progress();
    void quarantine(const std::filesystem::path &file, const std::string &why);
    std::string verify_frame(std::uint64_t index, const std::string &bytes) const;

    std::filesystem::path dir_;
    TimelineConfig config_;
    tools::Json scene_, job_;
    FrameSource source_;
    Options options_;
    std::string hash_;
    tools::Json frames_ = tools::Json::object(); // "000000" -> {png_sha256, png_bytes, ...}
    std::atomic<bool> cancelled_{false};
    bool locked_{}, opened_{};
    ResumeReport report_;
};
} // namespace gravastar::recording
