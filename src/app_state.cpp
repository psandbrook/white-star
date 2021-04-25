#include <app_state.hpp>

#include <glm/ext/matrix_transform.hpp>

namespace {

AppState* get_app_state(GLFWwindow* const window) {
    return static_cast<AppState*>(glfwGetWindowUserPointer(window));
}

extern "C" void glfw_key_callback(GLFWwindow* window, int key, int /* scancode */, int action, int /* mods */) {
    AppState* const app_state = get_app_state(window);

    switch (key) {
    case GLFW_KEY_ESCAPE: {
        if (action == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }
    } break;

    case GLFW_KEY_W: {
        if (action == GLFW_PRESS) {
            app_state->wireframe_render = !app_state->wireframe_render;
        }
    } break;
    }
}

extern "C" void glfw_cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    AppState* const app_state = get_app_state(window);

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        constexpr f64 factor = 0.001;

        const f64 xrel = xpos - app_state->cursor_xpos;
        const f64 yrel = ypos - app_state->cursor_ypos;

        const f64 x_angle = factor * -xrel;
        const glm::mat4 x_mat = glm::rotate(glm::mat4(1.0f), static_cast<f32>(x_angle), app_state->camera_up);

        const f64 y_angle = factor * -yrel;
        const glm::vec3 right =
                glm::normalize(glm::cross(app_state->camera_up, app_state->camera_pos - app_state->camera_target));
        const glm::mat4 y_mat = glm::rotate(glm::mat4(1.0f), static_cast<f32>(y_angle), right);

        app_state->camera_pos = glm::vec3(y_mat * x_mat * glm::vec4(app_state->camera_pos, 1.0f));
    }

    app_state->cursor_xpos = xpos;
    app_state->cursor_ypos = ypos;
}

extern "C" void glfw_scroll_callback(GLFWwindow* window, double /* xoffset */, double yoffset) {
    AppState* const app_state = get_app_state(window);
    const f64 distance =
            yoffset * 0.1 * static_cast<f64>(glm::length(app_state->camera_target - app_state->camera_pos));
    const glm::vec3 front = glm::normalize(app_state->camera_target - app_state->camera_pos);
    const glm::mat4 mat = glm::translate(glm::mat4(1.0f), static_cast<f32>(distance) * front);
    app_state->camera_pos = glm::vec3(mat * glm::vec4(app_state->camera_pos, 1.0f));
}

extern "C" void glfw_framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    AppState* const app_state = get_app_state(window);
    app_state->framebuffer_width = width;
    app_state->framebuffer_height = height;
}

f32 srgb_to_linear(const f32 value) {
    if (value < 0.04045f) {
        return value / 12.92f;
    } else {
        return std::pow((value + 0.055f) / 1.055f, 2.4f);
    }
}
} // namespace

AppState::AppState(GLFWwindow* const window) : window(window) {
    glfwSetWindowUserPointer(window, this);
    glfwSetKeyCallback(window, glfw_key_callback);
    glfwSetCursorPosCallback(window, glfw_cursor_pos_callback);
    glfwSetScrollCallback(window, glfw_scroll_callback);
    glfwSetFramebufferSizeCallback(window, glfw_framebuffer_size_callback);

    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    glfwGetCursorPos(window, &cursor_xpos, &cursor_ypos);
}

bool AppState::process_events() {
    glfwPollEvents();
    return glfwWindowShouldClose(window);
}

void AppState::update(const f64 seconds) {}
