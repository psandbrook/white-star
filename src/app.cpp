#include <app.hpp>

#include <glm/ext/matrix_transform.hpp>
#include <ogrsf_frmts.h>
#include <whereami.h>

namespace {

constexpr i32 updates_per_s = 60;
constexpr f64 update_s = 1.0 / updates_per_s;

const char* const app_name = "White Star";

App* get_app(void* ptr) {
    return static_cast<App*>(ptr);
}

App* get_app(GLFWwindow* const window) {
    return static_cast<App*>(glfwGetWindowUserPointer(window));
}

extern "C" void glfw_error_callback(int error_code, const char* description) {
    ABORT_F("GLFW error {:#x}: {}", error_code, description);
}

extern "C" void glfw_key_callback(GLFWwindow* window, int key, int /* scancode */, int action, int /* mods */) {
    App* const app = get_app(window);

    switch (key) {
    case GLFW_KEY_ESCAPE: {
        if (action == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }
    } break;

    case GLFW_KEY_W: {
        if (action == GLFW_PRESS) {
            app->wireframe_render = !app->wireframe_render;
        }
    } break;
    }
}

extern "C" void glfw_cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    App* const app = get_app(window);

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        constexpr f64 factor = 0.001;

        const f64 xrel = xpos - app->cursor_xpos;
        const f64 yrel = ypos - app->cursor_ypos;

        const f64 x_angle = factor * -xrel;
        const glm::mat4 x_mat = glm::rotate(glm::mat4(1.0f), static_cast<f32>(x_angle), app->camera_up);

        const f64 y_angle = factor * -yrel;
        const glm::vec3 right = glm::normalize(glm::cross(app->camera_up, app->camera_pos - app->camera_target));
        const glm::mat4 y_mat = glm::rotate(glm::mat4(1.0f), static_cast<f32>(y_angle), right);

        app->camera_pos = glm::vec3(y_mat * x_mat * glm::vec4(app->camera_pos, 1.0f));
    }

    app->cursor_xpos = xpos;
    app->cursor_ypos = ypos;
}

extern "C" void glfw_scroll_callback(GLFWwindow* window, double /* xoffset */, double yoffset) {
    App* const app = get_app(window);
    const f64 distance = yoffset * 0.1 * static_cast<f64>(glm::length(app->camera_target - app->camera_pos));
    const glm::vec3 front = glm::normalize(app->camera_target - app->camera_pos);
    const glm::mat4 mat = glm::translate(glm::mat4(1.0f), static_cast<f32>(distance) * front);
    app->camera_pos = glm::vec3(mat * glm::vec4(app->camera_pos, 1.0f));
}

extern "C" void glfw_framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    App* const app = get_app(window);
    app->framebuffer_width = width;
    app->framebuffer_height = height;
}

f32 srgb_to_linear(const f32 value) {
    if (value < 0.04045f) {
        return value / 12.92f;
    } else {
        return std::pow((value + 0.055f) / 1.055f, 2.4f);
    }
}
} // namespace

extern "C" {

void* app_init() {
    App* app = new App();
    app->init();
    return app;
}

void app_destroy(void* ptr) {
    App* app = get_app(ptr);
    app->destroy();
}

int app_update(void* ptr) {
    App* app = get_app(ptr);
    return app->update();
}

#ifdef HOT_RELOAD
void app_load(void* ptr) {
    App* app = get_app(ptr);
    app->load();
}

void app_unload(void* ptr) {
    (void)ptr;
}
#endif
}

void App::init() {

    {
        const int size = wai_getExecutablePath(nullptr, 0, nullptr);
        CHECK_F(size != -1);

        std::vector<char> buffer(static_cast<size_t>(size + 1));
        wai_getExecutablePath(buffer.data(), static_cast<int>(buffer.size()), nullptr);

        executable_dir_path = Path(buffer.data()).parent_path();
    }

    GDALAllRegister();

    glfwSetErrorCallback(glfw_error_callback);
    CHECK_F(glfwInit());

    counts_per_s = glfwGetTimerFrequency();
    last_count = glfwGetTimerValue();

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    CHECK_NOTNULL_F(monitor);

    const GLFWvidmode* video_mode = glfwGetVideoMode(monitor);
    CHECK_NOTNULL_F(video_mode);

    glfwWindowHint(GLFW_RESIZABLE, true);
    glfwWindowHint(GLFW_MAXIMIZED, true);
    glfwWindowHint(GLFW_CENTER_CURSOR, false);
    glfwWindowHint(GLFW_RED_BITS, video_mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS, video_mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, video_mode->blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, video_mode->refreshRate);
    glfwWindowHint(GLFW_SRGB_CAPABLE, true);
    glfwWindowHint(GLFW_SAMPLES, render_samples);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(video_mode->width, video_mode->height, app_name, nullptr, nullptr);
    CHECK_NOTNULL_F(window);

    glfwMakeContextCurrent(window);

    glfwSetWindowUserPointer(window, this);

    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    glfwGetCursorPos(window, &cursor_xpos, &cursor_ypos);

    renderer.init(this);

    load();
}

void App::load() {
    glfwSetErrorCallback(glfw_error_callback);
    glfwSetKeyCallback(window, glfw_key_callback);
    glfwSetCursorPosCallback(window, glfw_cursor_pos_callback);
    glfwSetScrollCallback(window, glfw_scroll_callback);
    glfwSetFramebufferSizeCallback(window, glfw_framebuffer_size_callback);
}

void App::destroy() {
    glfwTerminate();
}

bool App::update() {
    const u64 new_count = glfwGetTimerValue();
    const f64 elapsed_s = static_cast<f64>(new_count - last_count) / static_cast<f64>(counts_per_s);
    last_count = new_count;
    lag_s += elapsed_s;

    // Process input
    if (process_events()) {
        return true;
    }

    // Update
    i32 updates = 0;
    while (lag_s >= update_s && updates < updates_per_s) {
        step();
        lag_s -= update_s;
        ++updates;
    }

    // Render
    renderer.render();

    return false;
}

bool App::process_events() {
    glfwPollEvents();
    return glfwWindowShouldClose(window);
}

void App::step() {}

Path App::get_resource_path(const Path& path) {
    Path result = executable_dir_path;
    result /= "data";
    result /= path;
    return result;
}
