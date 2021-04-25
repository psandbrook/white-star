#pragma once

#include <utility.hpp>

#include <filesystem>
#include <string>

struct Path : public std::filesystem::path {
    Path() = default;
    Path(const char* const str) : std::filesystem::path(std::filesystem::u8path(str)) {}
    Path(const std::string& str) : std::filesystem::path(std::filesystem::u8path(str)) {}
    Path(std::filesystem::path path) : std::filesystem::path(std::move(path)) {}
};

void init_resource_path();
Path get_resource_path(const Path& path);
