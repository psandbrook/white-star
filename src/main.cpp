#include <app_state.hpp>
#include <filesystem.hpp>
#include <render.hpp>
#include <utility.hpp>

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <ogrsf_frmts.h>

namespace {

const char* const app_name = "White Star";

extern "C" void glfw_error_callback(int error_code, const char* description) {
    ABORT_F("GLFW error {:#x}: {}", error_code, description);
}
} // namespace

int main(int argc, char** argv) {

    loguru::init(argc, argv);
    init_resource_path();
    GDALAllRegister();

    glfwSetErrorCallback(glfw_error_callback);
    CHECK_F(glfwInit());
    DEFER([] { glfwTerminate(); });

    GLFWwindow* window = nullptr;
    {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        CHECK_NOTNULL_F(monitor);

        const GLFWvidmode* video_mode = glfwGetVideoMode(monitor);
        CHECK_NOTNULL_F(video_mode);

        glfwWindowHint(GLFW_RESIZABLE, true);
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
        window = glfwCreateWindow(video_mode->width, video_mode->height, app_name, monitor, nullptr);
        CHECK_NOTNULL_F(window);

        glfwMakeContextCurrent(window);

        CHECK_F(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)));
    }

    auto app_state = AppState(window);
    auto renderer = Renderer(&app_state);

    // Main loop
    {
        constexpr i32 updates_per_s = 60;
        constexpr f64 update_s = 1.0 / updates_per_s;
        const u64 counts_per_s = glfwGetTimerFrequency();

        f64 lag_s = 0.0;
        u64 last_count = glfwGetTimerValue();

        while (true) {
            const u64 new_count = glfwGetTimerValue();
            const f64 elapsed_s = static_cast<f64>(new_count - last_count) / static_cast<f64>(counts_per_s);
            last_count = new_count;
            lag_s += elapsed_s;

            // Process input
            if (app_state.process_events()) {
                break;
            }

            // Update
            i32 updates = 0;
            while (lag_s >= update_s && updates < updates_per_s) {
                app_state.update(update_s);
                lag_s -= update_s;
                ++updates;
            }

            // Render
            renderer.render();
        }
    }

    return 0;
}
