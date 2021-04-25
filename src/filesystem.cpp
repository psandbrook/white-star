#include <filesystem.hpp>

#include <whereami.h>

namespace {

Path executable_path;
Path executable_dir_path;

} // namespace

void init_resource_path() {
    const int size = wai_getExecutablePath(nullptr, 0, nullptr);
    CHECK_F(size != -1);

    std::vector<char> buffer(static_cast<size_t>(size + 1));
    wai_getExecutablePath(buffer.data(), static_cast<int>(buffer.size()), nullptr);

    executable_path = Path(buffer.data());
    executable_dir_path = executable_path.parent_path();
}

Path get_resource_path(const Path& path) {
    Path result = executable_dir_path;
    result /= "data";
    result /= path;
    return result;
}
