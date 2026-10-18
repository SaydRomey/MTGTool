#pragma once

#include <filesystem>
#include <string>

namespace mtg {

std::string read_text_file(const std::filesystem::path& path);
std::string decompress_xz_file(const std::filesystem::path& path);

} // namespace mtg
