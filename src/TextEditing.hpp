#pragma once
// 位置编辑内部实现，不向应用层暴露 libxml2 节点。
#include "cppwordkit/Types.hpp"
#include <libxml/tree.h>
#include <string_view>

namespace cppwordkit::detail {
std::string paragraphEditingText(xmlNodePtr paragraph);
void insertParagraphText(xmlNodePtr paragraph, std::size_t offset, std::string_view text);
void deleteParagraphRange(xmlNodePtr paragraph, std::size_t offset, std::size_t length);
void styleParagraphRange(xmlNodePtr paragraph, std::size_t offset, std::size_t length,
    const TextStyle& style, void (*apply)(xmlNodePtr, const TextStyle&));
}
