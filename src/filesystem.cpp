#include "filesystem.hpp"

std::string read_file(const Path& path) {
    std::ifstream stream(path);
    CHECK_F(bool(stream));

    std::stringstream source_buffer;
    source_buffer << stream.rdbuf();
    return source_buffer.str();
}
