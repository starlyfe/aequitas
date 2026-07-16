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

    if (!ui_wants_mouse) {
        const bool rmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        const bool mmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;

        if (rmb && dragging_rmb_) {
            yaw += static_cast<float>(dx) * 0.006f;
            pitch += static_cast<float>(dy) * 0.006f;
            pitch = std::clamp(pitch, kMinPitch, kMaxPitch);
        }
        dragging_rmb_ = rmb;

        if (mmb && dragging_mmb_) {
            const glm::vec3 fwd = glm::normalize(glm::vec3(std::sin(yaw), 0.f, std::cos(yaw)));
            const glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0.f, 1.f, 0.f)));
            const float pan_speed = distance * 0.0016f;
            target -= right * static_cast<float>(dx) * pan_speed;
            target += fwd * static_cast<float>(dy) * pan_speed;
        }
        dragging_mmb_ = mmb;

        if (scroll != 0.0) {
            distance -= static_cast<float>(scroll) * distance * 0.12f;
            distance = std::clamp(distance, kMinDistance, kMaxDistance);
        }
    } else {
        dragging_rmb_ = false;
        dragging_mmb_ = false;
    }

    if (!ui_wants_keyboard) {
        const glm::vec3 fwd = glm::normalize(glm::vec3(std::sin(yaw), 0.f, std::cos(yaw)));
        const glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0.f, 1.f, 0.f)));
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

std::optional<Hex> Camera::pick(GLFWwindow* window) const {
    int w = 0;
    int h = 0;
    glfwGetWindowSize(window, &w, &h);
    if (w <= 0 || h <= 0) {
        return std::nullopt;
    }
    double mx = 0.0;
    double my = 0.0;
    glfwGetCursorPos(window, &mx, &my);

    const float ndc_x = static_cast<float>(2.0 * mx / w - 1.0);
    const float ndc_y = static_cast<float>(1.0 - 2.0 * my / h);
    const float aspect = static_cast<float>(w) / static_cast<float>(h);

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
    const float t = -origin.y / dir.y;
    if (t < 0.f) {
        return std::nullopt;
    }
    const glm::vec3 hit = origin + dir * t;
    return world_to_hex(Vec2{hit.x, hit.z});
}

} // namespace aeq
