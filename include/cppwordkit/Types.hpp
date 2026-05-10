#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
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

enum class ParagraphAlignment {
    Left,
    Center,
    Right,
    Justify
};

enum class LineSpacingRule {
    Auto,
    Exact,
    AtLeast
};

struct TextStyle {
    std::optional<std::string> fontFamily;
    std::optional<std::string> colorHex;
    std::optional<std::int32_t> fontSizeHalfPoints;
    std::optional<bool> bold;
    std::optional<bool> italic;
    std::optional<bool> underline;
};

struct ParagraphStyle {
    std::optional<ParagraphAlignment> alignment;
    std::optional<std::int32_t> lineSpacingTwips;
    std::optional<LineSpacingRule> lineSpacingRule;
    std::optional<std::int32_t> firstLineIndentTwips;
    std::optional<std::int32_t> leftIndentTwips;
    std::optional<std::int32_t> rightIndentTwips;
};

} // namespace cppwordkit
