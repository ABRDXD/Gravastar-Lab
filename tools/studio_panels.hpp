// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Studio panels beyond the render view: scene presets, emission, scientific
// status and single-pixel inspection (a CPU re-trace with its trajectory).
#include "gravastar/optics/scene.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gravastar::studio {
enum class Side { left, right };
// Default position and size of a panel (first use only; the user may move it).
void place(Side side, float top, float width, float height);

struct SceneEntry {
    std::string name;
    std::filesystem::path path;
};
// Scenes under benchmarks/ the GPU tracer supports (angular patches, toy or
// Kerr), sorted by name.
std::vector<SceneEntry> list_scenes(const std::filesystem::path &benchmarks);
optics::Scene load_scene(const std::filesystem::path &path);

// Model, numerics and honesty labels for the current scene.
void science_panel(const optics::Scene &, const std::string &name);
// Disk on/off, disk motion and the background channel. Returns true when the
// scene changed; `message` explains a refused change.
bool emission_panel(optics::Scene &scene, std::optional<optics::StarCatalogue> &saved_background,
                    std::string &message);

struct Inspection {
    int x{-1}, y{-1};
    optics::State initial{};
    optics::Trace trace;
    optics::RaySample sample;
    std::string error;
};
// CPU re-trace of one render pixel with its trajectory, as the CPU tracer
// would publish it.
Inspection inspect(const optics::Scene &render_scene, int x, int y);
void inspection_panel(const Inspection &);
void recording_panel();
} // namespace gravastar::studio
