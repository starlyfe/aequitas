#include "render/camera.h"
#include "render/renderer.h"
#include "render/vk_context.h"

#include "sim/agent.h"
#include "sim/hex.h"
#include "sim/params.h"
#include "sim/simulation.h"
#include "sim/world.h"

#include "ui/hud.h"

#include "aequitas_version.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <optional>

using namespace aeq;

namespace {

void glfw_error_callback(int code, const char* description) {
    std::fprintf(stderr, "GLFW error %d: %s\n", code, description);
}

// Lerp night -> dawn -> day -> dusk -> night across one tick (one full day/night cycle),
// matching ARCHITECTURE.md's sun_elevation = sin(2*pi*tick_fraction).
glm::vec4 sky_color(float tick_fraction) {
    const float elevation = std::sin(6.2831853f * tick_fraction);
    const glm::vec3 night(0.02f, 0.03f, 0.07f);
    const glm::vec3 dawn(0.55f, 0.42f, 0.32f);
    const glm::vec3 day(0.53f, 0.72f, 0.86f);

    const float t = std::clamp((elevation + 1.f) * 0.5f, 0.f, 1.f);
    const glm::vec3 low = glm::mix(night, dawn, std::clamp(t * 2.f, 0.f, 1.f));
    const glm::vec3 color = glm::mix(low, day, std::clamp(t * 2.f - 1.f, 0.f, 1.f));
    return glm::vec4(color, 1.f);
}

glm::vec3 sun_direction(float tick_fraction) {
    const float phase = tick_fraction * 6.2831853f;
    const float elevation = std::max(std::sin(phase), 0.15f);
    const float horiz = std::sqrt(std::max(0.f, 1.f - elevation * elevation));
    const glm::vec3 to_sun(std::cos(phase) * horiz, elevation, std::sin(phase) * horiz);
    return -glm::normalize(to_sun);
}

} // namespace

int main() {
    std::printf("Aequitas %s — Vulkan render layer\n", AEQUITAS_VERSION_STRING);

    glfwSetErrorCallback(glfw_error_callback);
    if (glfwInit() == GLFW_FALSE) {
        std::fprintf(stderr, "glfwInit failed\n");
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Aequitas", nullptr, nullptr);
    if (window == nullptr) {
        std::fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return 1;
    }

#ifndef NDEBUG
    constexpr bool kEnableValidation = true;
#else
    constexpr bool kEnableValidation = false;
#endif

    VkContext ctx;
    ctx.init(window, kEnableValidation);

    glfwSetWindowUserPointer(window, &ctx);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int, int) {
        auto* c = static_cast<VkContext*>(glfwGetWindowUserPointer(w));
        if (c != nullptr) {
            c->on_resize();
        }
    });

    SimConfig sim_cfg;
    sim_cfg.seed = 42;
    Simulation sim;
    sim.init(sim_cfg);

    Renderer renderer;
    renderer.init(ctx);
    renderer.bake_terrain(ctx, sim.world());

    Camera camera;
    camera.target = glm::vec3(0.f, 0.f, 0.f);

    Hud hud;
    hud.init(ctx, window);

    bool paused = false;
    bool step_once = false;
    int speed_multiplier = 1;
    SimMode mode = SimMode::Observer;

    constexpr double kBaseTickSeconds = 1.0;
    constexpr double kMaxSpeedBudgetSeconds = 0.004;

    double accumulator = 0.0;
    double fps_smoothed = 60.0;
    bool mouse_was_down = false;
    float tick_alpha = 0.f;

    auto last_time = std::chrono::steady_clock::now();

    while (glfwWindowShouldClose(window) == GLFW_FALSE) {
        glfwPollEvents();

        const auto now = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double>(now - last_time).count();
        last_time = now;
        const double dt_clamped = std::clamp(dt, 0.0, 0.25);
        if (dt_clamped > 0.0) {
            fps_smoothed = fps_smoothed * 0.9 + (1.0 / dt_clamped) * 0.1;
        }

        camera.process_input(window, static_cast<float>(dt_clamped), hud.want_capture_mouse(),
                              hud.want_capture_keyboard());

        const bool mouse_down = !hud.want_capture_mouse() &&
                                 glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (mouse_down && !mouse_was_down) {
            if (const std::optional<Hex> hex = camera.pick(window)) {
                std::optional<int> nearest_agent;
                for (const Agent& a : sim.agents()) {
                    if (a.alive && a.pos == *hex) {
                        nearest_agent = a.id;
                        break;
                    }
                }
                if (nearest_agent.has_value()) {
                    hud.selected_agent = nearest_agent;
                    hud.selected_tile.reset();
                } else {
                    hud.selected_tile = hex;
                    hud.selected_agent.reset();
                }
            }
        }
        mouse_was_down = mouse_down;

        if (!paused) {
            accumulator += dt_clamped;
        }

        if (step_once) {
            sim.tick();
            step_once = false;
            accumulator = 0.0;
            tick_alpha = 0.f;
        } else if (!paused) {
            if (speed_multiplier <= 0) {
                const auto budget_start = std::chrono::steady_clock::now();
                while (std::chrono::duration<double>(std::chrono::steady_clock::now() - budget_start).count() <
                       kMaxSpeedBudgetSeconds) {
                    sim.tick();
                }
                accumulator = 0.0;
                tick_alpha = 0.f;
            } else {
                const double tick_seconds = kBaseTickSeconds / static_cast<double>(speed_multiplier);
                int ticks_done = 0;
                while (accumulator >= tick_seconds && ticks_done < 1000) {
                    sim.tick();
                    accumulator -= tick_seconds;
                    ++ticks_done;
                }
                tick_alpha = static_cast<float>(std::clamp(accumulator / tick_seconds, 0.0, 1.0));
            }
        }

        const glm::vec4 clear_color = sky_color(tick_alpha);
        const glm::vec3 sun_dir = sun_direction(tick_alpha);

        FrameContext frame;
        if (!ctx.begin_frame(frame)) {
            continue;
        }

        hud.begin_frame();
        hud.draw(sim, paused, step_once, speed_multiplier, mode, static_cast<float>(fps_smoothed));

        renderer.draw(ctx, frame, camera, sim.view(), hud.selected_agent, hud.selected_tile, tick_alpha, sun_dir,
                       clear_color);

        hud.end_frame(ctx, frame);
        ctx.end_frame(frame);
    }

    ctx.wait_idle();
    hud.shutdown(ctx);
    renderer.shutdown(ctx);
    ctx.shutdown();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
