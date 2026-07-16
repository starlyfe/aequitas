#pragma once

#include "sim/hex.h"
#include "sim/world.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <optional>

struct GLFWwindow;
struct GLFWcursor;

namespace aeq {

// Orbit camera: target sits on the y=0 plane; distance/yaw/pitch describe the eye offset.
class Camera {
public:
    void process_input(GLFWwindow* window, float dt, bool ui_wants_mouse, bool ui_wants_keyboard);

    glm::vec3 eye_position() const;
    glm::mat4 view_matrix() const;
    glm::mat4 proj_matrix(float aspect) const;
    glm::mat4 view_proj(float aspect) const;

    // Unprojects the cursor, raycasts terrain tops (when world is given), returns the hex under it.
    std::optional<Hex> pick(GLFWwindow* window, const World* world = nullptr) const;

    glm::vec3 target{0.f, 0.f, 0.f};
    float distance = 22.f;
    float yaw = 0.f;          // radians, around +Y
    float pitch = 0.8f;       // radians, elevation above horizon

    float fov_y = glm::radians(50.f);
    float near_plane = 0.1f;
    float far_plane = 500.f;

private:
    double last_cursor_x_ = 0.0;
    double last_cursor_y_ = 0.0;
    bool dragging_orbit_ = false; // middle mouse
    bool dragging_pan_ = false;   // right mouse
    bool has_last_cursor_ = false;
    GLFWcursor* pan_cursor_ = nullptr;
};

} // namespace aeq
