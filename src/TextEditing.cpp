// Unicode 位置编辑：只拆分命中的 Run，保留属性、超链接及未编辑的 XML。
#include "TextEditing.hpp"
#include "cppwordkit/Error.hpp"
#include <algorithm>
#include <memory>
#include <limits>
#include <vector>

namespace cppwordkit::detail {
namespace {
using Node = std::unique_ptr<xmlNode, decltype(&xmlFreeNode)>;
constexpr const char* wordNamespace = "http://schemas.openxmlformats.org/wordprocessingml/2006/main";
bool named(xmlNodePtr node, const char* name) {
    return node->type == XML_ELEMENT_NODE && node->ns &&
        xmlStrEqual(node->ns->href, BAD_CAST wordNamespace) && xmlStrEqual(node->name, BAD_CAST name);
}
std::string content(xmlNodePtr node) {
    const auto value = std::unique_ptr<xmlChar, decltype(xmlFree)>(xmlNodeGetContent(node), xmlFree);
    return value ? reinterpret_cast<const char*>(value.get()) : "";
}

// 返回 Unicode 标量的字节边界，同时拒绝非法 UTF-8 和 XML 1.0 禁止的字符。
std::vector<std::size_t> boundaries(std::string_view text) {
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw WordProcessingException("Text exceeds the XML text node size limit");
    std::vector<std::size_t> result{0};
    for (std::size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i]);
        std::size_t count = first < 0x80 ? 1 : first >= 0xc2 && first <= 0xdf ? 2 :
            first >= 0xe0 && first <= 0xef ? 3 : first >= 0xf0 && first <= 0xf4 ? 4 : 0;
        if (!count || count > text.size() - i) throw WordProcessingException("Invalid UTF-8 text");
        std::uint32_t scalar = first & (count == 1 ? 0x7f : count == 2 ? 0x1f : count == 3 ? 0x0f : 0x07);
        for (std::size_t j = 1; j < count; ++j) {
            const auto ch = static_cast<unsigned char>(text[i + j]);
            if ((ch & 0xc0) != 0x80) throw WordProcessingException("Invalid UTF-8 continuation");
            scalar = (scalar << 6) | (ch & 0x3f);
        }
        if ((count == 2 && scalar < 0x80) || (count == 3 && scalar < 0x800) ||
            (count == 4 && scalar < 0x10000) || scalar > 0x10ffff ||
            (scalar >= 0xd800 && scalar <= 0xdfff) || scalar == 0xfffe || scalar == 0xffff ||
            (scalar < 0x20 && scalar != 9 && scalar != 10 && scalar != 13)) {
            throw WordProcessingException("Text contains an invalid XML Unicode scalar");
        }
        i += count;
        result.push_back(i);
    }
    return result;
}
std::string runChildText(xmlNodePtr node) {
    if (node->type != XML_ELEMENT_NODE || named(node, "rPr") || named(node, "lastRenderedPageBreak")) return {};
    if (named(node, "t")) return content(node);
    if (named(node, "tab")) return "\t";
    if (named(node, "cr")) return "\n";
    if (named(node, "br")) {
        auto value = std::unique_ptr<xmlChar, decltype(xmlFree)>(
            xmlGetNsProp(node, BAD_CAST "type", BAD_CAST wordNamespace), xmlFree);
        if (!value || xmlStrEqual(value.get(), BAD_CAST "textWrapping")) return "\n";
        throw WordProcessingException("Position editing does not support page or column breaks");
    }
    if (named(node, "drawing") || named(node, "pict") || named(node, "object")) return "\xef\xbf\xbc";
    throw WordProcessingException("Position editing encountered unsupported run content");
}
struct RunSpan { xmlNodePtr node; std::size_t start; std::string text; std::size_t length; };
std::vector<RunSpan> runsIn(xmlNodePtr paragraph) {
    std::vector<RunSpan> runs;
    std::size_t position = 0;
    const auto collect = [&](auto&& self, xmlNodePtr parent) -> void {
        for (auto* node = parent->children; node; node = node->next) {
            if (node->type != XML_ELEMENT_NODE) continue;
            if (named(node, "r")) {
                std::string text;
                for (auto* child = node->children; child; child = child->next) text += runChildText(child);
                const auto length = boundaries(text).size() - 1;
                runs.push_back({node, position, std::move(text), length});
                position += length;
            } else if (named(node, "hyperlink")) {
                self(self, node);
            } else if (!named(node, "pPr") && !named(node, "bookmarkStart") &&
                       !named(node, "bookmarkEnd") && !named(node, "proofErr")) {
                // 未建模的域、修订、内容控件等不能当作零长度内容绕过。
                throw WordProcessingException("Position editing encountered unsupported paragraph content");
            }
        }
    };
    collect(collect, paragraph);
    return runs;
}
void checkRange(const std::vector<RunSpan>& runs, std::size_t offset, std::size_t length) {
    const auto total = runs.empty() ? 0 : runs.back().start + runs.back().length;
    if (offset > total || length > total - offset) throw WordProcessingException("Text range is out of bounds");
}
void setText(xmlNodePtr node, std::string_view text) {
    // xmlNodeSetContent 会解释实体；用文本节点写入，确保 & 和 < 保持字面值。
    xmlNodeSetContent(node, nullptr);
    auto* value = xmlNewTextLen(BAD_CAST text.data(), static_cast<int>(text.size()));
    if (!value) throw WordProcessingException("Cannot allocate text node");
    xmlAddChild(node, value);
    auto* ns = xmlSearchNsByHref(node->doc, node, BAD_CAST "http://www.w3.org/XML/1998/namespace");
    xmlSetNsProp(node, ns, BAD_CAST "space", BAD_CAST "preserve");
}
Node copyRunShell(xmlNodePtr run) {
    Node result(xmlDocCopyNode(run, run->doc, 2), xmlFreeNode);
    if (!result) throw WordProcessingException("Cannot copy run properties");
    for (auto* child = run->children; child; child = child->next) {
        if (named(child, "rPr")) {
            auto* copy = xmlDocCopyNode(child, run->doc, 1);
            if (!copy) throw WordProcessingException("Cannot copy run style");
            xmlAddChild(result.get(), copy);
        }
    }
    return result;
}
// 在 Run 内拆分；调用方仅传入严格位于文本内部的位置。
xmlNodePtr splitRun(xmlNodePtr run, std::size_t offset) {
    auto right = copyRunShell(run);
    std::size_t position = 0;
    for (auto* child = run->children; child;) {
        auto* next = child->next;
        if (!named(child, "rPr")) {
            const auto text = runChildText(child);
            const auto offsets = boundaries(text);
            const auto length = offsets.size() - 1;
            if (position >= offset) {
                xmlUnlinkNode(child);
                xmlAddChild(right.get(), child);
            } else if (length > offset - position) {
                Node copy(xmlDocCopyNode(child, run->doc, 1), xmlFreeNode);
                if (!copy) throw WordProcessingException("Cannot split text node");
                const auto byte = offsets[offset - position];
                setText(copy.get(), text.substr(byte));
                setText(child, text.substr(0, byte));
                xmlAddChild(right.get(), copy.release());
            }
            position += length;
        }
        child = next;
    }
    auto* result = right.get();
    xmlAddNextSibling(run, right.release());
    return result;
}
std::vector<xmlNodePtr> selectedRuns(const std::vector<RunSpan>& runs, std::size_t offset, std::size_t length) {
    std::vector<xmlNodePtr> selected;
    if (!length) return selected;
    const auto end = offset + length;
    for (const auto& run : runs) {
        const auto begin = std::max(offset, run.start);
        const auto stop = std::min(end, run.start + run.length);
        if (begin >= stop) continue;
        // 图片等对象占一个位置，但文本编辑不负责删除或格式化对象。
        std::size_t position = run.start;
        for (auto* child = run.node->children; child; child = child->next) {
            const auto count = boundaries(runChildText(child)).size() - 1;
            if (count && position < stop && position + count > begin &&
                (named(child, "drawing") || named(child, "pict") || named(child, "object"))) {
                throw WordProcessingException("Text range includes an embedded object");
            }
            position += count;
        }
        if (stop < run.start + run.length) splitRun(run.node, stop - run.start);
        selected.push_back(begin > run.start ? splitRun(run.node, begin - run.start) : run.node);
    }
    return selected;
}
void appendText(xmlNodePtr run, std::string_view text) {
    std::size_t start = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i < text.size() && text[i] != '\t' && text[i] != '\n' && text[i] != '\r') continue;
        if (i > start) {
            auto* node = xmlNewChild(run, run->ns, BAD_CAST "t", nullptr);
            if (!node) throw WordProcessingException("Cannot create text element");
            setText(node, text.substr(start, i - start));
        }
        if (i < text.size()) {
            if (!xmlNewChild(run, run->ns, BAD_CAST (text[i] == '\t' ? "tab" : "br"), nullptr))
                throw WordProcessingException("Cannot create control element");
            if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
        }
        start = i + 1;
    }
}
}

std::string paragraphEditingText(xmlNodePtr paragraph) {
    std::string text;
    for (const auto& run : runsIn(paragraph)) text += run.text;
    return text;
}
void insertParagraphText(xmlNodePtr paragraph, std::size_t offset, std::string_view text) {
    boundaries(text);
    const auto runs = runsIn(paragraph);
    checkRange(runs, offset, 0);
    if (text.empty()) return;
    // 边界优先继承左侧 Run 的格式和超链接归属；段首继承首个 Run。
    for (const auto& run : runs) {
        if (offset < run.start || offset > run.start + run.length) continue;
        auto inserted = copyRunShell(run.node);
        appendText(inserted.get(), text);
        if (offset == run.start) xmlAddPrevSibling(run.node, inserted.release());
        else if (offset == run.start + run.length) xmlAddNextSibling(run.node, inserted.release());
        else {
            auto* right = splitRun(run.node, offset - run.start);
            xmlAddPrevSibling(right, inserted.release());
        }
        return;
    }
    Node inserted(xmlNewNode(paragraph->ns, BAD_CAST "r"), xmlFreeNode);
    if (!inserted) throw WordProcessingException("Cannot create run");
    xmlSetTreeDoc(inserted.get(), paragraph->doc);
    appendText(inserted.get(), text);
    xmlAddChild(paragraph, inserted.release());
}
void deleteParagraphRange(xmlNodePtr paragraph, std::size_t offset, std::size_t length) {
    const auto runs = runsIn(paragraph);
    checkRange(runs, offset, length);
    for (auto* run : selectedRuns(runs, offset, length)) {
        for (auto* child = run->children; child;) {
            auto* next = child->next;
            if (!runChildText(child).empty()) { xmlUnlinkNode(child); xmlFreeNode(child); }
            child = next;
        }
    }
}
void styleParagraphRange(xmlNodePtr paragraph, std::size_t offset, std::size_t length,
    const TextStyle& style, void (*apply)(xmlNodePtr, const TextStyle&)) {
    const auto runs = runsIn(paragraph);
    checkRange(runs, offset, length);
    for (auto* run : selectedRuns(runs, offset, length)) apply(run, style);
}
}
