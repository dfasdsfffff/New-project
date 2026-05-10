#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace cppwordkit {

using Path = std::filesystem::path;

struct TextRun {
    std::string text;
    std::size_t paragraphIndex{};
    std::size_t runIndex{};
};

struct TableCell {
    std::string text;
};

using TableRow = std::vector<TableCell>;
using TableData = std::vector<TableRow>;

struct ReplaceOptions {
    bool matchCase = true;
    bool wholeWord = false;
};

struct SaveOptions {
    bool overwrite = true;
};

struct ImageOptions {
    std::int64_t widthEmu = 914400;
    std::int64_t heightEmu = 914400;
    std::string description = "CppWordKit image";
};

} // namespace cppwordkit
