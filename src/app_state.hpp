#pragma once

#include <utility.hpp>

#include <GLFW/glfw3.h>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

struct AppState {
    GLFWwindow* window;

    i32 framebuffer_width;
    i32 framebuffer_height;

    f64 cursor_xpos;
    f64 cursor_ypos;

    glm::vec3 camera_pos = {0.0f, 0.0f, 4.0f};
    glm::vec3 camera_target = {0.0f, 0.0f, 0.0f};
    glm::vec3 camera_up = {0.0f, 1.0f, 0.0f};
    f32 fovy = glm::radians(60.0f);

    bool wireframe_render = false;

    AppState() = default;
    explicit AppState(GLFWwindow* window);

    // Returns true if the application should exit
    bool process_events();

    void update(f64 seconds);
};
