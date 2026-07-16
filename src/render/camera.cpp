#include "render/camera.h"

#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace aeq {
namespace {

constexpr float kMinDistance = 4.f;
constexpr float kMaxDistance = 90.f;
constexpr float kMinPitch = glm::radians(8.f);
constexpr float kMaxPitch = glm::radians(85.f);

double g_scroll_delta = 0.0;
bool g_scroll_hooked = false;

void scroll_callback(GLFWwindow*, double /*xoffset*/, double yoffset) { g_scroll_delta += yoffset; }

// Horizontal look direction (from eye toward target, on the ground plane).
glm::vec3 ground_forward(float yaw) {
    return glm::normalize(glm::vec3(-std::sin(yaw), 0.f, -std::cos(yaw)));
}

glm::vec3 ground_right(float yaw) {
    const glm::vec3 fwd = ground_forward(yaw);
    return glm::normalize(glm::cross(fwd, glm::vec3(0.f, 1.f, 0.f)));
}

} // namespace

glm::vec3 Camera::eye_position() const {
    const glm::vec3 offset{distance * std::cos(pitch) * std::sin(yaw), distance * std::sin(pitch),
                            distance * std::cos(pitch) * std::cos(yaw)};
    return target + offset;
}

glm::mat4 Camera::view_matrix() const { return glm::lookAt(eye_position(), target, glm::vec3(0.f, 1.f, 0.f)); }

glm::mat4 Camera::proj_matrix(float aspect) const {
    glm::mat4 proj = glm::perspective(fov_y, aspect, near_plane, far_plane);
    proj[1][1] *= -1.f; // Vulkan clip-space Y is flipped relative to GL.
    return proj;
}

glm::mat4 Camera::view_proj(float aspect) const { return proj_matrix(aspect) * view_matrix(); }

void Camera::process_input(GLFWwindow* window, float dt, bool ui_wants_mouse, bool ui_wants_keyboard) {
    if (!g_scroll_hooked) {
        glfwSetScrollCallback(window, scroll_callback);
        g_scroll_hooked = true;
    }
    if (pan_cursor_ == nullptr) {
        pan_cursor_ = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    }

    double mx = 0.0;
    double my = 0.0;
    glfwGetCursorPos(window, &mx, &my);
    if (!has_last_cursor_) {
        last_cursor_x_ = mx;
        last_cursor_y_ = my;
        has_last_cursor_ = true;
    }
    const double dx = mx - last_cursor_x_;
    const double dy = my - last_cursor_y_;
    last_cursor_x_ = mx;
    last_cursor_y_ = my;

    const double scroll = g_scroll_delta;
    g_scroll_delta = 0.0;

    bool panning = false;
    if (!ui_wants_mouse) {
        // RMB = pan (grab map), MMB = orbit.
        const bool rmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        const bool mmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;

        if (mmb && dragging_orbit_) {
            // Natural orbit: drag right → yaw the other way (world turns under the grab).
            yaw -= static_cast<float>(dx) * 0.006f;
            pitch -= static_cast<float>(dy) * 0.006f;
            pitch = std::clamp(pitch, kMinPitch, kMaxPitch);
        }
        dragging_orbit_ = mmb;

        if (rmb && dragging_pan_) {
            const glm::vec3 fwd = ground_forward(yaw);
            const glm::vec3 right = ground_right(yaw);
            const float pan_speed = distance * 0.0016f;
            // Grab-the-map: content follows the hand (screen-right = +right, screen-down = -fwd).
            // Camera target moves opposite so the grabbed point stays under the cursor.
            target -= right * static_cast<float>(dx) * pan_speed;
            target += fwd * static_cast<float>(dy) * pan_speed;
            panning = true;
        }
        dragging_pan_ = rmb;

        if (scroll != 0.0) {
            distance -= static_cast<float>(scroll) * distance * 0.12f;
            distance = std::clamp(distance, kMinDistance, kMaxDistance);
        }
    } else {
        dragging_orbit_ = false;
        dragging_pan_ = false;
    }

    glfwSetCursor(window, panning ? pan_cursor_ : nullptr);

    if (!ui_wants_keyboard) {
        const glm::vec3 fwd = ground_forward(yaw);
        const glm::vec3 right = ground_right(yaw);
        const float speed = std::max(distance, 4.f) * 0.7f * dt;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            target += fwd * speed;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            target -= fwd * speed;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            target -= right * speed;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            target += right * speed;
        }
    }
}

std::optional<Hex> Camera::pick(GLFWwindow* window, const World* world) const {
    int fb_w = 0;
    int fb_h = 0;
    glfwGetFramebufferSize(window, &fb_w, &fb_h);
    int win_w = 0;
    int win_h = 0;
    glfwGetWindowSize(window, &win_w, &win_h);
    if (fb_w <= 0 || fb_h <= 0 || win_w <= 0 || win_h <= 0) {
        return std::nullopt;
    }

    double mx = 0.0;
    double my = 0.0;
    glfwGetCursorPos(window, &mx, &my);
    // glfw cursor is in window coords; convert to framebuffer pixels (DPI / Retina).
    const double scale_x = static_cast<double>(fb_w) / static_cast<double>(win_w);
    const double scale_y = static_cast<double>(fb_h) / static_cast<double>(win_h);
    mx *= scale_x;
    my *= scale_y;

    // With proj[1][1] flipped for Vulkan, unproject using Vulkan NDC (+Y down).
    const float ndc_x = static_cast<float>(2.0 * mx / static_cast<double>(fb_w) - 1.0);
    const float ndc_y = static_cast<float>(2.0 * my / static_cast<double>(fb_h) - 1.0);
    const float aspect = static_cast<float>(fb_w) / static_cast<float>(fb_h);

    const glm::mat4 inv_vp = glm::inverse(view_proj(aspect));
    glm::vec4 near_pt = inv_vp * glm::vec4(ndc_x, ndc_y, 0.f, 1.f);
    glm::vec4 far_pt = inv_vp * glm::vec4(ndc_x, ndc_y, 1.f, 1.f);
    if (std::fabs(near_pt.w) < 1e-8f || std::fabs(far_pt.w) < 1e-8f) {
        return std::nullopt;
    }
    near_pt /= near_pt.w;
    far_pt /= far_pt.w;

    const glm::vec3 origin(near_pt);
    const glm::vec3 dir = glm::normalize(glm::vec3(far_pt) - origin);
    if (std::fabs(dir.y) < 1e-6f) {
        return std::nullopt;
    }

    auto hit_y_plane = [&](float plane_y) -> std::optional<glm::vec3> {
        const float t = (plane_y - origin.y) / dir.y;
        if (t < 0.f) {
            return std::nullopt;
        }
        return origin + dir * t;
    };

    // Seed with the ground plane, then refine against each tile's prism top so the pick
    // matches what the eye sees (y=0 alone drifts toward the camera on raised biomes).
    std::optional<glm::vec3> hit = hit_y_plane(0.f);
    if (!hit.has_value()) {
        return std::nullopt;
    }

    Hex hex = world_to_hex(Vec2{hit->x, hit->z});
    if (world != nullptr) {
        for (int iter = 0; iter < 4; ++iter) {
            const Tile* tile = world->try_get(hex);
            const float height = tile != nullptr ? tile_surface_height(tile->biome) : 0.f;
            hit = hit_y_plane(height);
            if (!hit.has_value()) {
                break;
            }
            const Hex next = world_to_hex(Vec2{hit->x, hit->z});
            if (next == hex) {
                break;
            }
            hex = next;
        }
    }
    return hex;
}

} // namespace aeq
