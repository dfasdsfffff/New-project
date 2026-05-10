#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace cppwordkit::detail {

inline std::string toString(std::string_view value) {
    return std::string(value.begin(), value.end());
}

inline std::string lowerAscii(std::string_view value) {
    std::string result(value.begin(), value.end());
    std::ranges::transform(result, result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
}

inline bool isWordChar(char ch) {
    const auto uch = static_cast<unsigned char>(ch);
    return std::isalnum(uch) || ch == '_';
}

inline bool wholeWordMatch(std::string_view text, std::size_t position, std::size_t length) {
    const bool leftOk = position == 0 || !isWordChar(text[position - 1]);
    const auto right = position + length;
    const bool rightOk = right >= text.size() || !isWordChar(text[right]);
    return leftOk && rightOk;
}

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
