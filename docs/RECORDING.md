# Deterministic recording (Phase F, 2026-11-24)

`gravastar_record` renders a numbered frame sequence on the GPU tracer
(`GPU_TRACER.md`) into a resumable job directory, then encodes and verifies a
video. The job can be done in parts: stop it at any time (Ctrl+C, a time
budget or a frame limit) and run the same command again to carry on. This
implements the PLAN.md "Deterministic recording specification".

```text
R=build/<desktop>/gravastar_record
S=benchmarks/recording_acceptance_v1.json

$R --spec $S --job out/acceptance --stop-after-minutes 480   # one part (8 h)
$R --spec $S --job out/acceptance --encode                    # finish and encode
$R --spec $S --job out/acceptance --verify 0,137,299,599      # re-render and compare
$R --job out/acceptance --summary --output results/p5_recording_acceptance.json
$R --spec $S --measure 0 --output results/p5_recording_measure_f000.json
```

Overrides become part of the job identity: `--precision fp32|fp64`,
`--tolerance T|scene`, `--width W`, `--height H`.

## Recording spec v1

```json
{
  "schema_version": 1,
  "scene": "optics_keplerian_stars_v1.json",
  "timeline": {"width": 1920, "height": 1080, "fps": [60, 1], "frame_count": 600},
  "renderer": {"precision": "fp32", "tolerance": 1e-6, "samples_per_pixel": 1},
  "radiance": false,
  "camera_track": {
    "mode": "stationary_snapshots",
    "interpolation": "piecewise_linear_r_theta_phi",
    "keys": [{"frame": 0, "r": 8.0, "theta": 1.0472, "phi": 0.0}, "..."]
  }
}
```

- **Scene:** a path relative to the spec file. It must use angular
  coordinates, since the GPU tracer requires them. Its `image` size is ignored;
  the timeline's width and height are used.
- **Timeline:** a rational frame rate. Frame `i` is at time `i·den/num`,
  computed exactly and converted to double once. Frames run from `0` to
  `frame_count − 1`, so a 600-frame, 60 fps job lasts exactly 10 s with no
  duplicate endpoint frame.
- **Renderer:** `fp32` or `fp64` kernel. `tolerance` sets the absolute and
  relative tolerance, sets the event tolerance 10× looser, and raises the
  null-drift limit to at least 1000× the tolerance (`optics::with_tolerance`).
  `null` keeps the scene's own settings. `samples_per_pixel` must be 1: one
  deterministic ray through each pixel centre, so no seeds or accumulation are
  involved.
- **Camera track:** keys sit at integer frames and must cover frame 0 to the
  last frame. Between keys, r, θ and φ are interpolated linearly, and each key
  is hit exactly. φ is not wrapped, so the keys set the direction of travel.
  Keys must stay inside the studio's orbit limits (r in [2.5, 0.9·outer],
  θ in [0.05, π − 0.05]).
  **Stationary snapshots:** each frame's observer is at rest, in the scene's
  observer frame, at the interpolated position. Camera motion between frames
  adds no aberration and no Doppler shift. Geometry and emission do not change
  with time.

The acceptance spec makes one seamless orbit. φ goes from 0 to 2π·599/600 at
a steady π/300 per frame. The camera dips from θ = 60° to 80° and moves in
from r = 8 to 6.5 at frame 300, then returns.

## Job directory

```text
job.json                 spec, scene file, renderer identity, formats, content hash
progress.json            per-frame PNG (and radiance) SHA-256 and size; atomic
job.lock                 {pid, host} of the one writer
frames/frame_000000.png  display frames
radiance/frame_000000.npy  optional scientific output
logs/render.jsonl        one line per session start and per rendered frame
logs/encode.json, ffmpeg.log, ffprobe.json   encoding record
logs/quarantine/, quarantine.log, locks.log  set-aside files and lock recoveries
video.mp4                only after a verified encode
```

- **Display frames:** PNG, 8-bit RGB, no alpha, gAMA 1/2.2, no text chunks
  (vendored LodePNG). Pixels come from `optics::sample_color`, the diagnostic
  display mapping:
  - disk: grey, v = (I/(1+I))^(1/2.2) of the linear intensity;
  - escaped rays: a blue tint scaled by the star background;
  - captured rays: black.
- **Radiance** (`"radiance": true`): NumPy `.npy` 1.0, `<f8`, shape
  (height, width, 3). The channels are:
  - 0: linear disk intensity, 0 off the disk;
  - 1: linear background intensity, 0 without a background;
  - 2: the `optics::Outcome` code.

  At 1080p this is 50 MB per frame, so the acceptance spec leaves it off.

## Resume, lock and failure rules

- **Content hash.** `job.json` holds a SHA-256 over:
  - the timeline, formats, spec and scene file;
  - the renderer identity: executable and kernel SHA-256, GPU name, vendor
    and device IDs, driver and API versions.

  A resume with any difference is refused, and the message suggests a new
  job directory. So **do not rebuild, and do not update the GPU driver,
  between parts.**
- **Write order.** Each frame is written radiance first, display second and
  checkpoint last. Each write goes to a temporary file, is flushed and closed,
  then atomically renamed.
- **Reconciliation on open.** Leftover temporary files are removed. A complete,
  valid frame from the crash window (renamed but not checkpointed) is adopted.
  These files are moved to `logs/quarantine/`, never deleted:
  - corrupt, truncated or wrong-size frames;
  - frames whose bytes differ from the checkpoint;
  - out-of-range or unknown files;
  - orphan radiance files.

  Recorded frames whose file is gone are re-rendered.
- **Lock.** A live writer is refused. A lock left by a dead process on this
  host is reclaimed only with `--recover-stale-lock`, and the recovery is
  logged. A lock from another host is always refused.
- **Interruption.** The first Ctrl+C or SIGTERM stops at the next chunk of
  2¹⁷ rays. The current frame is abandoned and never written; a second Ctrl+C
  kills at once, and the atomic writes keep the job consistent either way.
  The bounded work unit is one frame, since there is no sample-level resume.
- **Failures.** A full disk, a permissions error, a lost GPU or an exception
  stops the run with the completed frames intact. An encoder failure keeps
  every frame and leaves no video.

## Encoding and verification

`encode` runs only on a complete sequence that is still unchanged and
gap-free: every hash is re-checked, and there must be no extra files in
`frames/`. FFmpeg is called with an argument array:

```text
-framerate num/den -start_number 0 -i frames/frame_%06d.png -frames:v N -an
-c:v libx264 -preset medium -crf 17 -pix_fmt yuv420p
-color_primaries bt709 -color_trc bt709 -colorspace bt709 -movflags +faststart
```

Output goes to `video.partial.mp4`, with the log in `logs/ffmpeg.log`. The
video is then decoded with ffprobe (`-count_frames`) and must pass every
check:

- the resolution;
- the rational frame rate, e.g. `60/1`;
- the decoded frame count;
- a presentation timestamp on every frame, strictly increasing;
- a duration within one frame period of N·den/num.

Only then is it renamed to `video.mp4`. Width and height must be even
(yuv420p).

## What "deterministic" means here

- **Same executable, kernel, GPU and driver:** frames are byte-identical,
  whether a job is rendered in one go or in parts. `--verify` checks this by
  re-rendering frames from scratch and comparing bytes with the resumed job.
  The CTest smoke job (64×36, 8 frames, with radiance) is recorded in two
  parts, then frames 0, 2, 3 and 7 are re-rendered and must match.
  The CPU fallback's results do not depend on its thread count.
- **Other GPUs, drivers or builds:** no bit-identical promise. Such a resume
  is refused. Agreement across devices is a numerical question, covered by the
  GPU-vs-CPU comparisons in `GPU_TRACER.md`.

## Tests

| Test | What it covers |
| --- | --- |
| `recording_image_io` | PNG and npy formats, round trips, rejection of malformed files |
| `recording_lifecycle` | 64×64, 30 frames: cancel, resume, lock, settings refusal, encode, probe |
| `recording_fault_injection` | 600 frames: crash, stale lock, crash window, corrupt, truncated, missing, extra, temporary, orphan radiance, cancel; byte-identical to an uninterrupted job; encoder failures keep frames |
| `recording_gpu_part1/part2/verify` | the real renderer: two-part smoke job, encode and probe, re-rendered frames identical |

## Speed and accuracy at 1080p (RTX 3050 laptop GPU, 12 CPU threads)

`--measure` on the acceptance spec compares each recording path with FP64
at the scene's own tolerance (1e-10). Measured 2026-11-25 with the fixed
kernel (below). Results are in `results/p5_recording_measure_f000.json` and
`results/p5_recording_measure_f300.json`. Each variant there records its
executable and kernel SHA-256. The table shows frame 0 / frame 300.

| Path | s per frame | 600-frame job | pixels differing | > 8 levels | > 32 levels | outcome changes | PSNR |
| --- | --- | --- | --- | --- | --- | --- | --- |
| FP64, scene tolerance (reference) | 205.7 / 178.3 | 30–34 h | — | — | — | — | — |
| FP64, 1e-6 | 92.5 / 83.4 | 14–15 h | 2.4% / 2.9% | 30 / 26 | 17 / 19 | 20 / 24 | 61.3 / 61.2 dB |
| FP32, 1e-6 | 17.9 / 22.0 | 3.0–3.7 h | 5.2% / 5.8% | 93 / 74 | 42 / 47 | 57 / 62 | 58.0 / 56.6 dB |

No image has a background pixel whose 48 neighbours (7×7) are all disk
(`background_pixels_surrounded_by_disk` = 0 in all six).

Almost all differing pixels are off by 1–2 levels: 99.9% at FP64 1e-6 and
99.5% at FP32. In FP32, 1.8 s of each frame is GPU work. Most of the rest
is the CPU re-trace of the 3.5–4.7% of rays that enter the core (12–16 s),
since the GPU kernel has no core patch. Each 1080p display frame is about
0.9 MB, so the 600 frames take about 540 MB.

**Fixed: rays passing through the disk (2026-11-25).** The earlier frames had
background pixels in the middle of the disk image. The earlier note counted
40 in FP32 frame 0 and one in the FP64 reference, at (86, 1012). Counted
from ray outcomes, as `--measure` now does, the old FP32 frame 0 had 56.
Each such pixel is a ray that crossed the disk plane inside the annulus
without the hit counting. The GPU kernel dropped a hit whose root lay in the
last bisection interval of a step (`GPU_TRACER.md`). All 56 and the
reference pixel were GPU-resolved, and the CPU tracer at the same settings
puts each on the disk; (86, 1012) hits it at r = 5.006. With the fix the
reference gained exactly that ray (1,170,051 to 1,170,052 disk hits), and no
image has such pixels. Rays whose escape hit was dropped used to run on to
the affine budget and be re-traced on the CPU; that count fell from 73 to 3
in FP32 frame 0.

Before the fix (2026-11-24). The first three rows also predate the
per-image camera observer, which removed 12–15 s of CPU work per frame
without changing a byte:

| Path | s per frame | 600-frame job | pixels differing | > 8 levels | > 32 levels | outcome changes | PSNR |
| --- | --- | --- | --- | --- | --- | --- | --- |
| FP64, scene tolerance (reference) | 250.6 / 223.8 | 37–42 h | — | — | — | — | — |
| FP64, 1e-6 | 130.8 / 121.3 | 20–22 h | 2.4% / 2.9% | 87 / 95 | 65 / 76 | 75 / 86 | 52.8 / 54.1 dB |
| FP32, 1e-6 | 33.8 / 36.6 | 5.6–6.1 h | 5.2% / 5.8% | 153 / 147 | 92 / 109 | 115 / 131 | 52.3 / 53.0 dB |
| FP32, 1e-6, per-image observer (frames 0, 1) | 22.4 / 20.2 | about 3.5 h | | | | | |
