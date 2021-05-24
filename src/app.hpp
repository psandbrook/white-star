#pragma once

#include <filesystem.hpp>
#include <render.hpp>
#include <utility.hpp>

#include <GLFW/glfw3.h>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

inline constexpr i32 render_samples = 8;

extern "C" {

void* app_init();
void app_destroy(void* ptr);
int app_update(void* ptr);

#ifdef HOT_RELOAD
void app_load(void* ptr);
void app_unload(void* ptr);
#endif
}

struct App {
    GLFWwindow* window;
    Path executable_dir_path;
    Renderer renderer;

    u64 counts_per_s;
    f64 lag_s = 0.0;
    u64 last_count;

    i32 framebuffer_width;
    i32 framebuffer_height;

    f64 cursor_xpos;
    f64 cursor_ypos;

    glm::vec3 camera_pos = {0.0f, 0.0f, 4.0f};
    glm::vec3 camera_target = {0.0f, 0.0f, 0.0f};
    glm::vec3 camera_up = {0.0f, 1.0f, 0.0f};
    f32 fovy = glm::radians(60.0f);

    bool wireframe_render = false;

    void init();
    void load();
    void destroy();
    bool update();

    // Returns true if the application should exit
    bool process_events();

    void step();

    Path get_resource_path(const Path& path);
};
