// SPDX-License-Identifier: GPL-3.0-or-later
#include "studio_panels.hpp"
#include "gravastar/optics/background.hpp"
#include "gravastar/optics/core_chart.hpp"
#include <algorithm>
#include <fstream>
#include <imgui.h>
#include <implot.h>
#include <numbers>

namespace gravastar::studio {
namespace o = optics;
namespace g = geometry;

void place(Side side, float top, float width, float height) {
    const auto *viewport = ImGui::GetMainViewport();
    const float x = side == Side::left ? viewport->WorkPos.x + 12
                                       : viewport->WorkPos.x + viewport->WorkSize.x - width - 12;
    ImGui::SetNextWindowPos(ImVec2(x, viewport->WorkPos.y + top), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_FirstUseEver);
}

o::Scene load_scene(const std::filesystem::path &path) {
    std::ifstream file(path);
    if (!file)
        throw std::runtime_error("Cannot open scene " + path.string());
    return o::parse_scene(tools::Json::parse(file));
}

std::vector<SceneEntry> list_scenes(const std::filesystem::path &benchmarks) {
    std::vector<SceneEntry> out;
    for (const auto &entry : std::filesystem::directory_iterator(benchmarks)) {
        if (entry.path().extension() != ".json")
            continue;
        try {
            const auto s = load_scene(entry.path());
            if (s.angular &&
                (s.geometry.family == g::Family::toy || s.geometry.family == g::Family::kerr))
                out.push_back({entry.path().stem().string(), entry.path()});
        } catch (const std::exception &) {
            // Not an optics scene (e.g. sampling policies or P1 benchmarks).
        }
    }
    std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) { return a.name < b.name; });
    return out;
}

void science_panel(const o::Scene &s, const std::string &name) {
    place(Side::right, 12, 420, 220);
    ImGui::Begin("Science status");
    ImGui::Text("Scene: %s", name.c_str());
    ImGui::Text("Family: %s   M = %.3f   a = %.3f", std::string(g::family_name(s.geometry.family)).c_str(),
                s.geometry.mass, s.geometry.spin);
    if (s.geometry.family == g::Family::toy)
        ImGui::Text("Lambda = %.3f   shell [%.2f, %.2f] (project choice)",
                    s.geometry.cosmological_constant, s.geometry.r_in, s.geometry.r_out);
    ImGui::Text("Core continuation: %s", s.settings.continue_core ? "single-sheet Cartesian" : "off");
    if (s.settings.capture_radius)
        ImGui::Text("Capture proxy r = %.2f (not a horizon crossing)", *s.settings.capture_radius);
    ImGui::Text("Tolerance %.0e   escape r = %.1f   affine budget %.0f",
                s.settings.absolute_tolerance, s.settings.outer_radius, s.settings.maximum_affine);
    ImGui::SeparatorText("What this image is not");
    ImGui::BulletText("Stability: not assessed (research item R1, not started).");
    ImGui::BulletText("Disk: prescribed %s motion, not a self-consistent disk.",
                      s.disk_motion == o::DiskMotion::zamo ? "ZAMO" : "Keplerian");
    ImGui::BulletText("Background: artistic star field; only its redshift is physics.");
    ImGui::BulletText("Colours: diagnostic mapping, not linear radiance.");
    ImGui::End();
}

bool emission_panel(o::Scene &s, std::optional<o::StarCatalogue> &saved, std::string &message) {
    bool changed = false;
    place(Side::right, 244, 420, 140);
    ImGui::Begin("Emission");
    bool disk = s.settings.disk;
    if (ImGui::Checkbox("Disk", &disk)) {
        s.settings.disk = disk;
        changed = true;
    }
    ImGui::Text("Annulus [%.1f, %.1f], emissivity (r_in/r)^3, I_obs = g^4 I", s.settings.disk_inner,
                s.settings.disk_outer);
    int motion = s.disk_motion == o::DiskMotion::zamo ? 0 : 1;
    const char *motions[] = {"ZAMO", "Keplerian prograde"};
    if (ImGui::Combo("Disk motion", &motion, motions, 2)) {
        if (motion == 1 && !o::keplerian_annulus_supported(s.geometry, s.settings.disk_inner,
                                                           s.settings.disk_outer))
            message = "Keplerian refused: the annulus leaves the stable timelike orbits.";
        else {
            s.disk_motion = motion == 0 ? o::DiskMotion::zamo : o::DiskMotion::keplerian_prograde;
            message.clear();
            changed = true;
        }
    }
    bool stars = s.background.has_value();
    if (ImGui::Checkbox("Star background (separate channel)", &stars)) {
        if (stars)
            s.background = saved ? *saved : o::make_catalogue(o::StarField{});
        else {
            saved = s.background;
            s.background.reset();
        }
        changed = true;
    }
    if (!message.empty())
        ImGui::TextColored(ImVec4(1, .5F, .3F, 1), "%s", message.c_str());
    ImGui::End();
    return changed;
}

Inspection inspect(const o::Scene &render_scene, int x, int y) {
    Inspection out;
    out.x = x;
    out.y = y;
    try {
        auto s = render_scene;
        s.settings.save_trajectory = true;
        const auto patch =
            s.position[2] <= std::numbers::pi / 2 ? o::Chart::north : o::Chart::south;
        out.initial = o::scene_pixel(s, x, y);
        out.trace = o::trace_angular(
            s.geometry, o::transform_state(out.initial, o::Chart::boyer_lindquist, patch), patch,
            s.settings);
        out.sample = o::ray_sample(s, out.initial, out.trace);
    } catch (const std::exception &e) {
        out.error = e.what();
    }
    return out;
}

void inspection_panel(const Inspection &in) {
    place(Side::right, 396, 420, 560);
    ImGui::Begin("Pixel inspection");
    if (in.x < 0) {
        ImGui::TextUnformatted("Right-click a pixel of the image to re-trace it on the CPU.");
        ImGui::End();
        return;
    }
    ImGui::Text("Pixel (%d, %d)", in.x, in.y);
    if (!in.error.empty()) {
        ImGui::TextColored(ImVec4(1, .4F, .4F, 1), "Error: %s", in.error.c_str());
        ImGui::End();
        return;
    }
    const auto &t = in.trace;
    ImGui::Text("Outcome: %s %s", std::string(o::outcome_name(t.outcome)).c_str(), t.detail.c_str());
    if (in.sample.intensity)
        ImGui::Text("Disk intensity %.4g   redshift g = %.4f", *in.sample.intensity,
                    in.sample.redshift.value_or(0));
    if (in.sample.background)
        ImGui::Text("Background %.4g   g = %.4f", *in.sample.background,
                    in.sample.background_redshift.value_or(0));
    ImGui::Text("Steps %zu accepted, %zu rejected, %zu RHS calls, %zu chart switches", t.accepted,
                t.rejected, t.rhs_calls, t.chart_switches);
    ImGui::Text("Max null residual %.2e   energy drift %.2e   axial drift %.2e",
                t.maximum_null_error, t.energy_error, t.angular_momentum_error);
    ImGui::SeparatorText("Events");
    for (const auto &c : t.crossings)
        ImGui::BulletText("%s at s = %.6f (%s)", c.surface.c_str(), c.point.affine,
                          std::string(o::chart_name(c.point.chart)).c_str());
    for (const auto &n : t.tangencies)
        ImGui::BulletText("near tangency: %s, value %.2e, margin %.1e%s", n.surface.c_str(), n.value,
                          n.margin, n.terminal ? " (outcome may differ)" : "");
    if (t.crossings.empty() && t.tangencies.empty())
        ImGui::TextUnformatted("none");
    // Trajectory: r(s), and the path projected on the x-z plane. Core-patch
    // points use the Cartesian core coordinates (X, Z).
    std::vector<double> s, r, px, pz;
    for (const auto &p : t.trajectory) {
        s.push_back(p.affine);
        if (p.chart == o::Chart::core_cartesian) {
            r.push_back(std::hypot(p.state[1], p.state[2], p.state[3]));
            px.push_back(p.state[1]);
            pz.push_back(p.state[3]);
        } else {
            const auto n = o::sky_direction(p);
            r.push_back(p.state[1]);
            px.push_back(p.state[1] * n[0]);
            pz.push_back(p.state[1] * n[2]);
        }
    }
    if (!s.empty() && ImPlot::BeginPlot("r along the ray", ImVec2(-1, 160))) {
        ImPlot::SetupAxes("affine s (into the past)", "r");
        ImPlot::PlotLine("r", s.data(), r.data(), static_cast<int>(s.size()));
        ImPlot::EndPlot();
    }
    if (!px.empty() && ImPlot::BeginPlot("Path, x-z projection", ImVec2(-1, 220), ImPlotFlags_Equal)) {
        ImPlot::PlotLine("ray", px.data(), pz.data(), static_cast<int>(px.size()));
        ImPlot::EndPlot();
    }
    ImGui::End();
}

void recording_panel() {
    place(Side::left, 712, 360, 120);
    ImGui::Begin("Recording");
    ImGui::TextWrapped("Not connected to this renderer yet (Phase F: PNG frames, resumable jobs, "
                       "the 1920x1080 600-frame acceptance run). The recorder exists and is tested "
                       "only on synthetic 64x64 frames.");
    ImGui::End();
}
} // namespace gravastar::studio
