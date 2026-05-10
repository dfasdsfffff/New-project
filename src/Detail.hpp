#pragma once
//! @file Detail.hpp 内部工具函数：字符串处理、大小写转换、全词匹配和文本替换

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace cppwordkit::detail {
//! @namespace cppwordkit::detail 内部实现工具函数，不对外暴露

//! 将 string_view 转为 std::string
inline std::string toString(std::string_view value) {
    return std::string(value.begin(), value.end());
}

//! 将 ASCII 字符串转为小写（非 ASCII 字符不受影响）
inline std::string lowerAscii(std::string_view value) {
    std::string result(value.begin(), value.end());
    std::ranges::transform(result, result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
}

//! 判断字符是否为"单词字符"（字母数字或下划线），用于全词匹配的边界检测
inline bool isWordChar(char ch) {
    const auto uch = static_cast<unsigned char>(ch);
    return std::isalnum(uch) || ch == '_';
}

//! 检查 text 中 position 开始、长度为 length 的子串是否为完整单词
//! 左右两侧均不被单词字符包围才算匹配
inline bool wholeWordMatch(std::string_view text, std::size_t position, std::size_t length) {
    const bool leftOk = position == 0 || !isWordChar(text[position - 1]);
    const auto right = position + length;
    const bool rightOk = right >= text.size() || !isWordChar(text[right]);
    return leftOk && rightOk;
}

//! 在字符串中替换所有匹配项
//! @param text 待处理的字符串（原地修改）
//! @param search 搜索文本
//! @param replacement 替换文本
//! @param matchCase 是否区分大小写
//! @param wholeWord 是否全词匹配
//! @return 替换次数
//! @note 不区分大小写时，同时维护原文本和小写版两个字符串以保持位置同步
inline std::size_t replaceAllInString(
    std::string& text,
    std::string_view search,
    std::string_view replacement,
    bool matchCase,
    bool wholeWord
) {
    if (search.empty()) {
        return 0;
    }

    std::size_t count = 0;
    std::size_t position = 0;

    if (matchCase) {
        while ((position = text.find(search, position)) != std::string::npos) {
            if (wholeWord && !wholeWordMatch(text, position, search.size())) {
                position += search.size();
                continue;
            }
            text.replace(position, search.size(), replacement);
            position += replacement.size();
            ++count;
        }
        return count;
    }

    // 不区分大小写：同时在原文本和小写副本上操作，保证替换后位置一致
    auto haystack = lowerAscii(text);
    const auto needle = lowerAscii(search);
    while ((position = haystack.find(needle, position)) != std::string::npos) {
        if (wholeWord && !wholeWordMatch(text, position, search.size())) {
            position += search.size();
            continue;
        }
        text.replace(position, search.size(), replacement);
        haystack.replace(position, search.size(), lowerAscii(replacement));
        position += replacement.size();
        ++count;
    }
    return count;
}

} // namespace cppwordkit::detail
