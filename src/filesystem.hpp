#pragma once

#include <filesystem>
#include <string>

struct Path : public std::filesystem::path {
    Path() = default;
    Path(const char* const str) : std::filesystem::path(std::filesystem::u8path(str)) {}
    Path(const std::string& str) : std::filesystem::path(std::filesystem::u8path(str)) {}
    Path(std::filesystem::path path) : std::filesystem::path(std::move(path)) {}
};

std::string read_file(const Path& path);
