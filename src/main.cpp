#ifdef HOT_RELOAD
    #include <dlfcn.h>

    #include <filesystem>

namespace fs = std::filesystem;

#else
    #include "app.hpp"
#endif

#include "utility.hpp"

int main(int argc, char** argv) {

    loguru::init(argc, argv);

#ifdef HOT_RELOAD
    const char* const lib_name = "libwhite_star_lib.so";

    using AppInitFn = void* (*)();
    using AppDestroyFn = void (*)(void*);
    using AppUpdateFn = int (*)(void*);
    using AppLoadFn = void (*)(void*);
    using AppUnloadFn = void (*)(void*);

    void* app_lib = nullptr;
    AppInitFn app_init = nullptr;
    AppDestroyFn app_destroy = nullptr;
    AppUpdateFn app_update = nullptr;
    AppLoadFn app_load = nullptr;
    AppUnloadFn app_unload = nullptr;

    fs::file_time_type last_lib_time = fs::last_write_time(lib_name);

    auto load_app_lib = [&] {
        app_lib = dlopen(lib_name, RTLD_NOW);
        CHECK_NOTNULL_F(app_lib);
        app_init = reinterpret_cast<AppInitFn>(dlsym(app_lib, "app_init"));
        app_destroy = reinterpret_cast<AppDestroyFn>(dlsym(app_lib, "app_destroy"));
        app_update = reinterpret_cast<AppUpdateFn>(dlsym(app_lib, "app_update"));
        app_load = reinterpret_cast<AppLoadFn>(dlsym(app_lib, "app_load"));
        app_unload = reinterpret_cast<AppUnloadFn>(dlsym(app_lib, "app_unload"));
    };

    auto unload_app_lib = [&] {
        int result = dlclose(app_lib);
        CHECK_F(result == 0);
        app_lib = nullptr;
        app_init = nullptr;
        app_destroy = nullptr;
        app_update = nullptr;
        app_load = nullptr;
        app_unload = nullptr;
    };

    load_app_lib();
#endif

    void* ptr = app_init();
    DEFER([&] { app_destroy(ptr); });

    while (true) {
        if (app_update(ptr)) {
            break;
        }

#ifdef HOT_RELOAD
        std::error_code error;
        fs::file_time_type new_lib_time = fs::last_write_time(lib_name, error);
        if (new_lib_time > last_lib_time) {
            app_unload(ptr);
            unload_app_lib();

            load_app_lib();
            app_load(ptr);
        }
        last_lib_time = new_lib_time;
#endif
    }

    return 0;
}
