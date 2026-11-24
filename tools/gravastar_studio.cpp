// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/platform/Window.hpp"
#include "gravastar/rendering/vulkan/WindowRenderer.hpp"

#include "studio_render.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <implot.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <numbers>
#include <optional>
#include <iostream>
#include <stdexcept>

namespace {
int integer(std::string_view text) {
    int value{};
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value < 0)
        throw std::invalid_argument("Expected a nonnegative integer");
    return value;
}
// ImGui and ImPlot contexts plus the GLFW backend. Declared after the window
// and before the renderer, so the renderer's ImGui Vulkan shutdown runs while
// the context exists and the GLFW shutdown runs while the window exists.
struct Interface final {
    explicit Interface(void *native_window) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGui::GetIO().IniFilename = nullptr; // no imgui.ini in the working directory
        if (!ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow *>(native_window), true))
            throw std::runtime_error("ImGui GLFW backend initialization failed");
    }
    ~Interface() {
        ImGui_ImplGlfw_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
    }
    Interface(const Interface &) = delete;
    Interface &operator=(const Interface &) = delete;
};
std::vector<std::uint32_t> read_shader(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error("Missing presentation shader: " + path.string());
    auto length = file.tellg();
    if (length < 20 || length > 16 * 1024 * 1024 || length % 4)
        throw std::runtime_error("Invalid shader file size");
    std::vector<std::uint32_t> result(static_cast<std::size_t>(length) / 4);
    file.seekg(0);
    if (!file.read(reinterpret_cast<char *>(result.data()), length))
        throw std::runtime_error("Shader read failed");
    return result;
}
} // namespace
int main(int argc, char **argv) {
    try {
        gravastar::rendering::vulkan::ComputeOptions options;
        bool smoke{};
        int width = 1001, height = 613, frames = 0;
        std::filesystem::path capture, shader_path;
        std::filesystem::path scene_path = "benchmarks/optics_keplerian_stars_v1.json";
        std::optional<std::array<double, 3>> start_camera;
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg(argv[i]);
            if (arg == "--help") {
                std::cout << "GravastarLab Desktop Studio\n"
                             "--device N | --validation auto|on|off | --width N | --height N\n"
                             "--frames N (0=until closed) | --capture image.ppm | --shader PATH\n"
                             "--scene benchmarks/X.json: angular-patch scene to render (FP64 GPU)\n"
                             "--camera r,theta,phi: start camera (clamped to the orbit limits)\n"
                             "--smoke: calibration pattern only; 24 frames, resize/minimize/restore "
                             "and exact compute-image captures\n"
                             "Escape closes the window. Minimized windows suspend GPU dispatch.\n";
                return 0;
            }
            if (arg == "--smoke") {
                smoke = true;
                continue;
            }
            if (++i == argc)
                throw std::invalid_argument("Missing value for " + std::string(arg));
            const std::string_view value(argv[i]);
            if (arg == "--device")
                options.device_index = integer(value);
            else if (arg == "--width")
                width = integer(value);
            else if (arg == "--height")
                height = integer(value);
            else if (arg == "--frames")
                frames = integer(value);
            else if (arg == "--capture")
                capture = value;
            else if (arg == "--shader")
                shader_path = value;
            else if (arg == "--scene")
                scene_path = value;
            else if (arg == "--camera") {
                std::array<double, 3> c{};
                if (std::sscanf(std::string(value).c_str(), "%lf,%lf,%lf", &c[0], &c[1], &c[2]) != 3)
                    throw std::invalid_argument("--camera expects r,theta,phi");
                start_camera = c;
            }
            else if (arg == "--validation") {
                if (value == "auto")
                    options.validation = gravastar::rendering::vulkan::ValidationMode::automatic;
                else if (value == "on")
                    options.validation = gravastar::rendering::vulkan::ValidationMode::required;
                else if (value == "off")
                    options.validation = gravastar::rendering::vulkan::ValidationMode::disabled;
                else
                    throw std::invalid_argument("--validation must be auto, on or off");
            } else
                throw std::invalid_argument("Unknown option: " + std::string(arg));
        }
        if (smoke) {
            frames = 24;
            if (capture.empty())
                capture = "out/window-smoke/final.ppm";
        }
        // The calibration pattern keeps the exact-pixel smoke checks; normal
        // runs show the progressive render through the view shader.
        const bool render_scene = !smoke && shader_path.empty();
        if (shader_path.empty())
            shader_path = smoke ? GRAVASTAR_PRESENTATION_SHADER : GRAVASTAR_VIEW_SHADER;
        const auto shader = read_shader(shader_path);
        std::optional<gravastar::optics::Scene> scene;
        if (render_scene) {
            if (scene_path.is_relative())
                scene_path = std::filesystem::path(GRAVASTAR_SOURCE_ROOT) / scene_path;
            std::ifstream file(scene_path);
            if (!file)
                throw std::runtime_error("Cannot open scene " + scene_path.string());
            scene = gravastar::optics::parse_scene(gravastar::tools::Json::parse(file));
            if (!scene->angular)
                throw std::runtime_error("The studio renders angular-patch scenes only");
        }
        gravastar::platform::Window window(
            width, height, "GravastarLab | Vulkan presentation diagnostic | Esc to close");
        Interface interface(window.native_handle());
        gravastar::rendering::vulkan::WindowRenderer renderer(window.surface_source(), options, shader);
        {
            const auto size = window.framebuffer_size();
            renderer.enable_overlay(size[0], size[1]);
        }
        std::array<float, 240> frame_ms{};
        std::size_t frame_slot = 0;
        // Render heights at the scene's aspect ratio (FP64: ~120 us per ray).
        constexpr std::array<int, 4> heights{96, 192, 288, 384};
        int quality = 1;
        std::optional<gravastar::studio::ProgressiveRender> render;
        // Orbit camera in the scene's BL coordinates around the axis. Elevation
        // stops 0.05 rad short of either pole (the launch frame's polar margin),
        // so the view never flips; distance stays inside the escape sphere.
        struct Camera {
            double r{}, theta{}, phi{};
        };
        Camera camera, home;
        std::vector<std::pair<std::string, Camera>> bookmarks;
        double r_min = 2.5, r_max = 9.0;
        if (scene) {
            home = {scene->position[1], scene->position[2], scene->position[3]};
            camera = home;
            r_max = 0.9 * scene->settings.outer_radius;
            if (start_camera)
                camera = {(*start_camera)[0], (*start_camera)[1], (*start_camera)[2]};
        }
        auto clamp_camera = [&] {
            camera.theta = std::clamp(camera.theta, 0.05, std::numbers::pi - 0.05);
            camera.r = std::clamp(camera.r, r_min, r_max);
            camera.phi = std::remainder(camera.phi, 2 * std::numbers::pi);
        };
        auto restart = [&] {
            gravastar::studio::RenderJob job{*scene, scene_path.filename().string()};
            job.scene.height = heights[quality];
            job.scene.width = std::max(1, (heights[quality] * scene->width) / scene->height);
            job.scene.position[1] = camera.r;
            job.scene.position[2] = camera.theta;
            job.scene.position[3] = camera.phi;
            render->start(std::move(job));
        };
        if (scene) {
            render.emplace(GRAVASTAR_TRACE64_SPV);
            clamp_camera();
            restart();
        }
        std::vector<std::uint32_t> view;
        int view_width = 0, view_height = 0;
        std::cout << "device=" << renderer.device().name
                  << " queue=" << renderer.device().compute_family << " validation="
                  << (renderer.validation_enabled() ? "enabled" : "disabled/unavailable")
                  << " presentation_fences=KHR_swapchain_maintenance1\n"
                  << std::flush;
        const auto start = std::chrono::steady_clock::now();
        auto minimized_at = start;
        bool resized{}, minimize_requested{}, minimize_observed{}, restored{}, second_resize{},
            initial_capture{}, resized_capture{};
        std::uint64_t title_frame = UINT64_MAX, frames_before_minimize{};
        while (!window.should_close()) {
            window.poll();
            auto statistics = renderer.statistics();
            const auto now = std::chrono::steady_clock::now();
            if (smoke && now - start > std::chrono::seconds(45))
                throw std::runtime_error(
                    "Window smoke test exceeded 45-second work deadline: presented=" +
                    std::to_string(statistics.presented) +
                    " acquire_retries=" + std::to_string(statistics.acquire_retries) +
                    " suspended=" + std::to_string(statistics.suspended) +
                    " recreations=" + std::to_string(statistics.recreations) +
                    " minimized=" + std::to_string(window.minimized()) +
                    " framebuffer=" + std::to_string(window.framebuffer_size()[0]) + "x" +
                    std::to_string(window.framebuffer_size()[1]) +
                    " validation_errors=" + std::to_string(statistics.validation_errors));
            if (smoke && statistics.presented >= 3 && !resized) {
                window.resize(817, 479);
                resized = true;
                window.poll();
            }
            if (smoke && resized && statistics.recreations >= 2 && !resized_capture) {
                // Capture only after drawing at least one frame into the replacement image.
                renderer.capture(capture.parent_path() / "resized.ppm");
                resized_capture = true;
            }
            if (smoke && statistics.presented >= 8 && !minimize_requested) {
                frames_before_minimize = statistics.presented;
                window.minimize();
                minimize_requested = true;
                minimized_at = now;
                window.poll();
            }
            if (smoke && minimize_requested && !restored) {
                if (window.minimized())
                    minimize_observed = true;
                if (now - minimized_at > std::chrono::milliseconds(50)) {
                    if (minimize_observed && renderer.statistics().presented != frames_before_minimize)
                        throw std::runtime_error("Frames were submitted while minimized");
                    window.restore();
                    window.resize(1023, 617);
                    restored = true;
                    second_resize = true;
                    window.poll();
                }
            }
            const auto extent = window.framebuffer_size();
            if (renderer.overlay_ready()) {
                renderer.begin_overlay_frame();
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();
                frame_ms[frame_slot++ % frame_ms.size()] = 1000.0F / ImGui::GetIO().Framerate;
                ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
                ImGui::Begin("GravastarLab studio");
                ImGui::TextUnformatted(render ? "Rendering the scene below."
                                              : "Calibration pattern: not a black-hole image.");
                ImGui::Text("Device: %s", renderer.device().name.c_str());
                const auto now_stats = renderer.statistics();
                ImGui::Text("Extent %u x %u, frames %llu", now_stats.width, now_stats.height,
                            static_cast<unsigned long long>(now_stats.presented));
                ImGui::Text("%.1f frames per second", ImGui::GetIO().Framerate);
                if (ImPlot::BeginPlot("Frame time (ms)", ImVec2(320, 120))) {
                    ImPlot::PlotLine("ms", frame_ms.data(), static_cast<int>(frame_ms.size()));
                    ImPlot::EndPlot();
                }
                ImGui::End();
                if (render) {
                    // Drag on the image to orbit, scroll to zoom (not over panels).
                    const auto &io = ImGui::GetIO();
                    bool moved = false;
                    if (!io.WantCaptureMouse && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
                        camera.phi -= 0.005 * io.MouseDelta.x;
                        camera.theta -= 0.005 * io.MouseDelta.y;
                        moved = io.MouseDelta.x != 0 || io.MouseDelta.y != 0;
                    }
                    if (!io.WantCaptureMouse && io.MouseWheel != 0) {
                        camera.r *= std::pow(0.9, io.MouseWheel);
                        moved = true;
                    }
                    if (moved) {
                        clamp_camera();
                        restart();
                    }
                    const auto st = render->status();
                    ImGui::SetNextWindowPos(ImVec2(12, 260), ImGuiCond_FirstUseEver);
                    ImGui::Begin("Render");
                    ImGui::Text("Scene: %s", scene_path.filename().string().c_str());
                    ImGui::TextUnformatted("FP64 GPU tracer, CPU fallback (docs/GPU_TRACER.md).");
                    ImGui::TextUnformatted("Colours: diagnostic mapping of linear data.");
                    const char *labels[] = {"96 rows", "192 rows", "288 rows", "384 rows"};
                    if (ImGui::Combo("Resolution", &quality, labels, 4))
                        restart();
                    if (ImGui::Button("Restart"))
                        restart();
                    ImGui::SeparatorText("Camera (ZAMO observer)");
                    ImGui::Text("r = %.3f   theta = %.4f   phi = %.4f", camera.r, camera.theta,
                                camera.phi);
                    float values[3] = {float(camera.r), float(camera.theta), float(camera.phi)};
                    bool typed = ImGui::DragFloat("r", &values[0], 0.02F, float(r_min), float(r_max));
                    typed |= ImGui::DragFloat("theta", &values[1], 0.002F, 0.05F,
                                              float(std::numbers::pi - 0.05));
                    typed |= ImGui::DragFloat("phi", &values[2], 0.005F, -float(std::numbers::pi),
                                              float(std::numbers::pi));
                    if (typed) {
                        camera = {values[0], values[1], values[2]};
                        clamp_camera();
                        restart();
                    }
                    if (ImGui::Button("Reset camera")) {
                        camera = home;
                        restart();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Bookmark") && bookmarks.size() < 16)
                        bookmarks.emplace_back("view " + std::to_string(bookmarks.size() + 1),
                                               camera);
                    for (std::size_t b = 0; b < bookmarks.size(); ++b) {
                        ImGui::PushID(static_cast<int>(b));
                        if (ImGui::SmallButton("Go")) {
                            camera = bookmarks[b].second;
                            restart();
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Delete")) {
                            bookmarks.erase(bookmarks.begin() + static_cast<std::ptrdiff_t>(b));
                            ImGui::PopID();
                            break;
                        }
                        ImGui::SameLine();
                        ImGui::Text("%s: r %.2f, theta %.3f, phi %.3f", bookmarks[b].first.c_str(),
                                    bookmarks[b].second.r, bookmarks[b].second.theta,
                                    bookmarks[b].second.phi);
                        ImGui::PopID();
                    }
                    const float progress =
                        st.rays_total ? float(st.rays_done) / float(st.rays_total) : 0.F;
                    ImGui::ProgressBar(progress);
                    if (st.rays_done == 0 && st.error.empty())
                        ImGui::TextUnformatted("Starting the GPU tracer (the first launch after a "
                                               "build compiles its shader; this can take a while).");
                    else if (st.finished)
                        ImGui::Text("Finished: %zu rays in %.1f s", st.rays_done, st.seconds);
                    else
                        ImGui::Text("Refining: %d-pixel blocks, %zu rays, %.1f s", st.level,
                                    st.rays_done, st.seconds);
                    ImGui::Text("GPU time %.2f s", st.gpu_milliseconds / 1000);
                    std::size_t fallback = 0;
                    for (std::size_t r = 1; r < 7; ++r)
                        fallback += st.fallback[r];
                    ImGui::Text("Re-traced on CPU: %zu (core %zu, near tangency %zu)", fallback,
                                st.fallback[1], st.fallback[2]);
                    ImGui::Text("Near-tangent flags on CPU re-traces: %zu", st.near_tangent);
                    if (!st.error.empty())
                        ImGui::TextColored(ImVec4(1, .4F, .4F, 1), "Error: %s", st.error.c_str());
                    ImGui::End();
                }
                ImGui::Render();
            }
            if (render && render->fetch(view, view_width, view_height))
                renderer.set_source(static_cast<std::uint32_t>(view_width),
                                    static_cast<std::uint32_t>(view_height), view);
            const auto status = renderer.draw(extent[0], extent[1], 0);
            if (status != gravastar::rendering::vulkan::FrameStatus::presented) {
                window.wait(0.01);
                continue;
            }
            statistics = renderer.statistics();
            if (smoke)
                std::cout << "smoke_frame=" << statistics.presented << " elapsed_ms="
                          << std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - start)
                                 .count()
                          << '\n'
                          << std::flush;
            if (smoke && !initial_capture) {
                renderer.capture(capture.parent_path() / "initial.ppm");
                initial_capture = true;
            }
            if (statistics.presented / 30 != title_frame) {
                title_frame = statistics.presented / 30;
                window.title("GravastarLab | Desktop Studio | " + renderer.device().name +
                             " | " + std::to_string(statistics.width) + " x " +
                             std::to_string(statistics.height) + " | Esc to close");
            }
            if (frames > 0 && statistics.presented >= static_cast<std::uint64_t>(frames))
                break;
        }
        if (!capture.empty())
            renderer.capture(capture);
        if (render) {
            const auto st = render->status();
            std::cout << "render generation=" << st.generation << " rays=" << st.rays_done << '/'
                      << st.rays_total << " level=" << st.level << " finished=" << st.finished
                      << " gpu_s=" << st.gpu_milliseconds / 1000 << " shown=" << view_width << 'x'
                      << view_height << " error=" << (st.error.empty() ? "none" : st.error)
                      << '\n';
        }
        renderer.finish();
        const auto result = renderer.statistics();
        std::cout << "presented=" << result.presented << " recreations=" << result.recreations
                  << " suspended=" << result.suspended
                  << " acquire_retries=" << result.acquire_retries
                  << " overlay_frames=" << result.overlay_frames
                  << " final_extent=" << result.width << 'x' << result.height
                  << " validation_errors=" << result.validation_errors << '\n';
        if (result.validation_errors)
            return 1;

        if (smoke &&
            (!resized || !resized_capture || !second_resize ||
             result.recreations < 2 || result.presented < 24 ||
             result.overlay_frames < 20))
            throw std::runtime_error(
                window.should_close()
                    ? "Window smoke cancelled: window closed before all 24 frames completed"
                    : "Window smoke lifecycle checks incomplete");
        std::cout << "PASS: Vulkan compute presentation"
                  << (smoke ? ", ImGui overlay, resize, and exact pixel readback" : "") << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "kerr_window: " << error.what() << '\n';
        return 1;
    }
}
