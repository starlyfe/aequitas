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

// Wall-clock day/night (independent of sim tick speed). ~3 minutes per full cycle.
constexpr float kDayLengthSeconds = 180.f;

float smooth01(float t) {
    t = std::clamp(t, 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

// Soft multi-stop sky: night → dawn → day → dusk → night.
glm::vec4 sky_color(float day_phase) {
    const float elev = std::sin(6.2831853f * day_phase);
    const glm::vec3 night(0.035f, 0.045f, 0.08f);
    const glm::vec3 dawn(0.62f, 0.40f, 0.30f);
    const glm::vec3 day(0.58f, 0.74f, 0.88f);
    const glm::vec3 dusk(0.48f, 0.32f, 0.38f);

    const float dayness = smooth01((elev + 0.12f) / 0.85f);
    const float duskness = smooth01(1.f - std::abs(std::fmod(day_phase + 0.75f, 1.f) - 0.5f) * 4.f);
    glm::vec3 color = glm::mix(night, day, dayness);
    color = glm::mix(color, dawn, (1.f - dayness) * smooth01(elev * 2.f + 0.4f) * 0.65f);
    color = glm::mix(color, dusk, duskness * (1.f - dayness) * 0.55f);
    return glm::vec4(color, 1.f);
}

glm::vec3 sun_direction(float day_phase) {
    const float phase = day_phase * 6.2831853f;
    const float elev = std::sin(phase);
    // Keep a soft fill light below the horizon instead of a hard clamp spike.
    const float elev_lit = elev > 0.f ? elev : 0.08f + elev * 0.05f;
    const float horiz = std::sqrt(std::max(0.f, 1.f - elev_lit * elev_lit));
    const glm::vec3 to_sun(std::cos(phase) * horiz, elev_lit, std::sin(phase) * horiz);
    return -glm::normalize(to_sun);
}

void lighting_params(float day_phase, float& ambient, float& sun_intensity) {
    const float elev = std::sin(6.2831853f * day_phase);
    const float dayness = smooth01((elev + 0.05f) / 0.7f);
    ambient = glm::mix(0.16f, 0.42f, dayness);
    sun_intensity = glm::mix(0.22f, 1.0f, smooth01((elev + 0.02f) / 0.55f));
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
    float day_phase = 0.18f; // start mid-morning

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

        // Daylight advances with wall clock (not sim speed), so 8x sim doesn't strobe the sky.
        day_phase = std::fmod(day_phase + static_cast<float>(dt_clamped) / kDayLengthSeconds, 1.f);
        if (day_phase < 0.f) {
            day_phase += 1.f;
        }

        camera.process_input(window, static_cast<float>(dt_clamped), hud.want_capture_mouse(),
                              hud.want_capture_keyboard());

        // Live hover hex for the Inspector coordinates readout.
        if (!hud.want_capture_mouse()) {
            hud.hover_tile = camera.pick(window, &sim.world());
        } else {
            hud.hover_tile.reset();
        }

        const bool mouse_down = !hud.want_capture_mouse() &&
                                 glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (mouse_down && !mouse_was_down) {
            if (const std::optional<Hex> hex = camera.pick(window, &sim.world())) {
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

        const glm::vec4 clear_color = sky_color(day_phase);
        const glm::vec3 sun_dir = sun_direction(day_phase);
        float ambient = 0.35f;
        float sun_intensity = 1.f;
        lighting_params(day_phase, ambient, sun_intensity);

        FrameContext frame;
        if (!ctx.begin_frame(frame)) {
            continue;
        }

        hud.begin_frame();
        hud.draw(sim, paused, step_once, speed_multiplier, mode, static_cast<float>(fps_smoothed));

        renderer.draw(ctx, frame, camera, sim.view(), hud.selected_agent, hud.selected_tile, tick_alpha, sun_dir,
                       clear_color, ambient, sun_intensity);

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
