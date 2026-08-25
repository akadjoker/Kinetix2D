#pragma once

#include <filesystem>
#include <string>

namespace k2d_tests
{

inline std::string tempPath(const char *fileName)
{
    std::error_code error;
    std::filesystem::path directory = std::filesystem::temp_directory_path(error);
    if (error || directory.empty())
        directory = std::filesystem::current_path(error);
    return (directory / fileName).generic_string();
}

} // namespace k2d_tests
