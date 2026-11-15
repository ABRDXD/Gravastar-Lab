# GravastarLab User & Developer Guide

## 1. Prerequisites & Environment

GravastarLab is a pure C++20 project developed and verified on Linux (Arch Linux / Linux 6.x) with Vulkan 1.4 compute support.

### Required System Tools
* **C++ Compiler**: GCC $\ge 13$ or Clang $\ge 16$ supporting `-std=c++20`.
* **Build System**: CMake $\ge 3.25$ and Ninja.
* **GPU & Drivers**: Vulkan 1.3+ driver (NVIDIA / AMD / Intel) with `VK_KHR_swapchain_maintenance1`.
* **Shader Compiler**: `glslc` (from the Vulkan SDK or package manager).
* **Media Tools**: External `/usr/bin/ffmpeg` and `/usr/bin/ffprobe`.

---

## 2. Building the Project

Configure and build using the default CMake preset:

```bash
# Configure headless release build
cmake --preset headless-release

# Build all libraries, shaders, and executables
cmake --build build/headless-release
```

---

## 3. Running the Test Suite

Execute all 15 native C++20 tests via CTest:

```bash
ctest --test-dir build/headless-release --output-on-failure
```

To run individual focused validation targets:
```bash
# Geodesic 113-bit quad-precision cross-validation:
./build/headless-release/gravastar_geodesic_cross_val

# Equatorial turning point convergence test:
./build/headless-release/gravastar_turning_point_test

# GPU vs CPU geodesic trajectory verification:
./build/headless-release/gravastar_vulkan_ray_test

# Vulkan device audit:
./build/headless-release/gravastar_vulkan_audit

# Presentation studio swapchain smoke check:
./build/headless-release/gravastar_studio --smoke

# Deterministic recording & FFmpeg container export:
./build/headless-release/gravastar_recording_test
```

---

## 4. Vulkan GPU Headless Raymarcher

Render a full numerical relativistic raymarch image on the GPU:

```bash
# Default 128x128 resolution:
./build/headless-release/gravastar_vulkan_render

# Custom resolution (e.g. 512x512):
./build/headless-release/gravastar_vulkan_render 512 512
```
The output is written to `results/vulkan_gravastar_render.ppm` and can be converted to PNG via:
```bash
ffmpeg -y -i results/vulkan_gravastar_render.ppm results/vulkan_gravastar_render.png
```

---

## 5. Desktop Presentation Studio

Launch the interactive desktop presentation studio:

```bash
# Launch interactive window
./build/headless-release/gravastar_studio --width 1280 --height 720

# Command-line options:
./build/headless-release/gravastar_studio --help
```
* **Controls**: Press `Escape` to close the window. Resizing the window automatically triggers swapchain recreation without stalling the rendering pipeline.

---

## 6. Deterministic Recording Pipeline

The recording engine provides rational frame timing ($t_i = i / \text{fps}$), atomic file durability, and external FFmpeg encoding:

```cpp
#include "gravastar/recording/recorder.hpp"

gravastar::recording::TimelineConfig config;
config.width = 1920;
config.height = 1080;
config.fps = {60, 1};
config.duration_seconds = 10.0; // 600 frames

gravastar::recording::RecordingSession session("results/job_run", config, params);
session.initialize();
session.render_all();
session.encode_video(); // Exports verified video.mp4 via FFmpeg
```
