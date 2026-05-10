//! @file XmlPart.cpp XML 文档部件的封装实现，基于 libxml2 解析 OOXML 中的 XML 内容
//! 提供文本替换、样式设置、表格行插入、XPath 查询等操作

#include "cppwordkit/XmlPart.hpp"

#include "Detail.hpp"
#include "cppwordkit/Error.hpp"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppwordkit {
namespace {

// 将 libxml2 的 xmlChar* 转为 std::string 并释放原内存
std::string xmlStringToStd(xmlChar* value) {
    if (value == nullptr) {
        return {};
    }
    std::string result(reinterpret_cast<const char*>(value));
    xmlFree(value);
    return result;
}

// 判断 XML 节点是否为指定名称的元素节点（使用 xmlStrEqual 确保命名空间感知比较）
bool isElementNamed(xmlNodePtr node, const char* localName) {
    return node != nullptr &&
        node->type == XML_ELEMENT_NODE &&
        xmlStrEqual(node->name, reinterpret_cast<const xmlChar*>(localName)) == 1;
}

// 验证是否为 6 位十六进制 RGB 颜色值
bool isHexColor(std::string_view value) {
    return value.size() == 6 && std::ranges::all_of(value, [](unsigned char ch) {
        return std::isxdigit(ch) != 0;
    });
}

std::string upperAscii(std::string_view value) {
    std::string result(value.begin(), value.end());
    std::ranges::transform(result, result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return result;
}

// 获取 XML 节点的 w: 命名空间属性值
// OOXML 的 WordprocessingML 命名空间为 http://schemas.openxmlformats.org/wordprocessingml/2006/main
std::string wAttributeValue(xmlNodePtr node, const char* localName) {
    if (node == nullptr) {
        return {};
    }
    return xmlStringToStd(xmlGetNsProp(
        node,
        reinterpret_cast<const xmlChar*>(localName),
        reinterpret_cast<const xmlChar*>("http://schemas.openxmlformats.org/wordprocessingml/2006/main")
    ));
}

// 设置 XML 节点的 w: 命名空间属性，若命名空间未注册则自动创建
void setWAttribute(xmlNodePtr node, const char* localName, std::string_view value) {
    if (node == nullptr) {
        throw WordProcessingException("Cannot set attribute on null XML node");
    }
    auto* ns = xmlSearchNsByHref(
        node->doc,
        node,
        reinterpret_cast<const xmlChar*>("http://schemas.openxmlformats.org/wordprocessingml/2006/main")
    );
    if (ns == nullptr) {
        ns = xmlNewNs(
            xmlDocGetRootElement(node->doc),
            reinterpret_cast<const xmlChar*>("http://schemas.openxmlformats.org/wordprocessingml/2006/main"),
            reinterpret_cast<const xmlChar*>("w")
        );
    }
    xmlSetNsProp(
        node,
        ns,
        reinterpret_cast<const xmlChar*>(localName),
        reinterpret_cast<const xmlChar*>(std::string(value).c_str())
    );
}

// 创建新的 w: 命名空间元素节点
xmlNodePtr newWElement(xmlDocPtr doc, const char* localName) {
    auto* root = xmlDocGetRootElement(doc);
    auto* ns = xmlSearchNsByHref(
        doc,
        root,
        reinterpret_cast<const xmlChar*>("http://schemas.openxmlformats.org/wordprocessingml/2006/main")
    );
    if (ns == nullptr) {
        ns = xmlNewNs(
            root,
            reinterpret_cast<const xmlChar*>("http://schemas.openxmlformats.org/wordprocessingml/2006/main"),
            reinterpret_cast<const xmlChar*>("w")
        );
    }

    auto* node = xmlNewNode(ns, reinterpret_cast<const xmlChar*>(localName));
    if (node == nullptr) {
        throw WordProcessingException("Unable to create WordprocessingML node");
    }
    return node;
}

// 查找父节点下第一个指定名称的子元素
xmlNodePtr firstChildElement(xmlNodePtr parent, const char* localName) {
    if (parent == nullptr) {
        return nullptr;
    }
    for (auto* child = parent->children; child != nullptr; child = child->next) {
        if (isElementNamed(child, localName)) {
            return child;
        }
    }
    return nullptr;
}

// 确保子元素存在：若存在则返回现有元素，否则在第一个子节点前插入新元素
// 用于确保 rPr / pPr 等属性节点在正确位置（作为第一个子元素）
xmlNodePtr ensureFirstChild(xmlNodePtr parent, const char* localName) {
    auto* existing = firstChildElement(parent, localName);
    if (existing != nullptr) {
        return existing;
    }

    auto* node = newWElement(parent->doc, localName);
    xmlNodePtr inserted = nullptr;
    if (parent->children != nullptr) {
        inserted = xmlAddPrevSibling(parent->children, node);
    } else {
        inserted = xmlAddChild(parent, node);
    }
    if (inserted == nullptr) {
        xmlFreeNode(node);
        throw WordProcessingException("Unable to insert WordprocessingML node");
    }
    return inserted;
}

// 确保子元素存在：追加到父节点末尾（用于不需特定顺序的属性节点）
xmlNodePtr ensureChild(xmlNodePtr parent, const char* localName) {
    auto* existing = firstChildElement(parent, localName);
    if (existing != nullptr) {
        return existing;
    }

    auto* node = newWElement(parent->doc, localName);
    if (xmlAddChild(parent, node) == nullptr) {
        xmlFreeNode(node);
        throw WordProcessingException("Unable to append WordprocessingML node");
    }
    return node;
}

// 安全解析整数值，失败时返回 nullopt
std::optional<std::int32_t> parseInt(std::string_view value) {
    if (value.empty()) {
        return std::nullopt;
    }
    try {
        return std::stoi(std::string(value));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

// 将对齐枚举转为 OOXML 字符串（OOXML 中两端对齐使用 "both"）
std::string alignmentToXml(ParagraphAlignment alignment) {
    switch (alignment) {
    case ParagraphAlignment::Left:
        return "left";
    case ParagraphAlignment::Center:
        return "center";
    case ParagraphAlignment::Right:
        return "right";
    case ParagraphAlignment::Justify:
        return "both";
    }
    return "left";
}

std::optional<ParagraphAlignment> alignmentFromXml(std::string_view value) {
    if (value == "left") {
        return ParagraphAlignment::Left;
    }
    if (value == "center") {
        return ParagraphAlignment::Center;
    }
    if (value == "right") {
        return ParagraphAlignment::Right;
    }
    if (value == "both" || value == "justify") {
        return ParagraphAlignment::Justify;
    }
    return std::nullopt;
}

std::string lineRuleToXml(LineSpacingRule rule) {
    switch (rule) {
    case LineSpacingRule::Auto:
        return "auto";
    case LineSpacingRule::Exact:
        return "exact";
    case LineSpacingRule::AtLeast:
        return "atLeast";
    }
    return "auto";
}

std::optional<LineSpacingRule> lineRuleFromXml(std::string_view value) {
    if (value == "auto") {
        return LineSpacingRule::Auto;
    }
    if (value == "exact") {
        return LineSpacingRule::Exact;
    }
    if (value == "atLeast") {
        return LineSpacingRule::AtLeast;
    }
    return std::nullopt;
}

// 解析 OOXML 的 toggle 属性节点：val 为空或 true/1/on 视为启用，false/0/off 视为禁用
std::optional<bool> boolFromToggleNode(xmlNodePtr node) {
    if (node == nullptr) {
        return std::nullopt;
    }
    const auto value = wAttributeValue(node, "val");
    if (value.empty() || value == "true" || value == "1" || value == "on") {
        return true;
    }
    if (value == "false" || value == "0" || value == "off") {
        return false;
    }
    return std::nullopt;
}

// 设置 toggle 属性的子元素：启用时不需要 val="1"，禁用时设置 val="0"
void setToggleChild(xmlNodePtr parent, const char* localName, bool enabled) {
    auto* child = ensureChild(parent, localName);
    if (enabled) {
        return;
    }
    setWAttribute(child, "val", "0");
}

// XPath: 定位第 paragraphIndex 个段落（从 0 开始）
std::string paragraphXPath(std::size_t paragraphIndex) {
    return "(//w:p)[" + std::to_string(paragraphIndex + 1) + "]";
}

// XPath: 定位段落中的特定 run
std::string runXPath(std::size_t paragraphIndex, std::size_t runIndex) {
    return paragraphXPath(paragraphIndex) + "/w:r[" + std::to_string(runIndex + 1) + "]";
}

std::string readTextFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw XmlException("Unable to open XML file: " + path);
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void writeTextFile(const std::string& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw XmlException("Unable to write XML file: " + path);
    }
    output << content;
}

bool nodeIsTextLike(xmlNodePtr node) {
    return node != nullptr && node->type == XML_TEXT_NODE;
}

// 向 XPath 上下文注册命名空间前缀，若注册失败返回 false
bool registerNamespace(xmlXPathContextPtr context, const char* prefix, const char* href) {
    return xmlXPathRegisterNs(
        context,
        reinterpret_cast<const xmlChar*>(prefix),
        reinterpret_cast<const xmlChar*>(href)
    ) == 0;
}

// 表示一个 w:t 文本节点及其在合并文本中的位置范围
struct WordTextNode {
    xmlNodePtr node{};
    std::string text;
    std::size_t start{};  //!< 在 combinedText 中的起始位置
    std::size_t end{};    //!< 在 combinedText 中的结束位置
};

struct TextMatch {
    std::size_t position{};
    std::size_t length{};
};

// 在字符串中查找所有匹配的位置（返回位置列表而非原地替换）
std::vector<TextMatch> findMatches(
    std::string_view text,
    std::string_view search,
    bool matchCase
) {
    std::vector<TextMatch> matches;
    if (search.empty()) {
        return matches;
    }

    const auto haystack = matchCase ? std::string(text) : detail::lowerAscii(text);
    const auto needle = matchCase ? std::string(search) : detail::lowerAscii(search);
    std::size_t position = 0;
    while ((position = haystack.find(needle, position)) != std::string::npos) {
        matches.push_back({position, search.size()});
        position += search.size();
    }
    return matches;
}

// 找到第一个与匹配范围重叠的 WordTextNode 索引
std::size_t firstOverlappingNode(
    const std::vector<WordTextNode>& nodes,
    std::size_t matchStart,
    std::size_t matchEnd
) {
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].end > matchStart && nodes[i].start < matchEnd) {
            return i;
        }
    }
    return nodes.size();
}

// 找到最后一个与匹配范围重叠的 WordTextNode 索引
std::size_t lastOverlappingNode(
    const std::vector<WordTextNode>& nodes,
    std::size_t matchStart,
    std::size_t matchEnd
) {
    for (std::size_t i = nodes.size(); i > 0; --i) {
        const auto index = i - 1;
        if (nodes[index].end > matchStart && nodes[index].start < matchEnd) {
            return index;
        }
    }
    return nodes.size();
}

// 规范化表格书签名：如果已经是 ${...} 格式则原样返回，否则补全 ${...}
std::string normalizeTableBookmark(std::string_view bookmark) {
    if (bookmark.empty()) {
        throw WordProcessingException("Table row bookmark must not be empty");
    }

    if (bookmark.size() >= 3 &&
        bookmark.substr(0, 2) == "${" &&
        bookmark.back() == '}') {
        return std::string(bookmark);
    }

    return "${" + std::string(bookmark) + "}";
}

// 向上查找指定名称的祖先元素
xmlNodePtr findAncestor(xmlNodePtr node, const char* localName) {
    for (auto* current = node; current != nullptr; current = current->parent) {
        if (isElementNamed(current, localName)) {
            return current;
        }
    }
    return nullptr;
}

// 收集父节点下所有指定名称的直接子元素
void collectChildElements(xmlNodePtr parent, const char* localName, std::vector<xmlNodePtr>& out) {
    if (parent == nullptr) {
        return;
    }

    for (auto* child = parent->children; child != nullptr; child = child->next) {
        if (isElementNamed(child, localName)) {
            out.push_back(child);
        }
    }
}

// 递归收集所有指定名称的后代元素
void collectDescendants(xmlNodePtr parent, const char* localName, std::vector<xmlNodePtr>& out) {
    if (parent == nullptr) {
        return;
    }

    for (auto* child = parent->children; child != nullptr; child = child->next) {
        if (isElementNamed(child, localName)) {
            out.push_back(child);
        }
        collectDescendants(child, localName, out);
    }
}

// 用数据填充表格行的每个单元格：第一个 w:t 写入文本，其余清空
void fillTableRow(xmlNodePtr row, const TableRow& data) {
    std::vector<xmlNodePtr> cells;
    collectChildElements(row, "tc", cells);

    const auto cellCount = std::min(cells.size(), data.size());
    for (std::size_t cellIndex = 0; cellIndex < cellCount; ++cellIndex) {
        std::vector<xmlNodePtr> textNodes;
        collectDescendants(cells[cellIndex], "t", textNodes);
        if (textNodes.empty()) {
            continue;
        }

        xmlNodeSetContent(textNodes.front(), reinterpret_cast<const xmlChar*>(data[cellIndex].text.c_str()));
        for (std::size_t textIndex = 1; textIndex < textNodes.size(); ++textIndex) {
            xmlNodeSetContent(textNodes[textIndex], reinterpret_cast<const xmlChar*>(""));
        }
    }
}

// 从表格行中删除书签标记文本
void removeBookmarkText(xmlNodePtr row, std::string_view bookmark) {
    std::vector<xmlNodePtr> textNodes;
    collectDescendants(row, "t", textNodes);
    for (auto* textNode : textNodes) {
        auto text = xmlStringToStd(xmlNodeGetContent(textNode));
        if (detail::replaceAllInString(text, bookmark, "", true, false) > 0) {
            xmlNodeSetContent(textNode, reinterpret_cast<const xmlChar*>(text.c_str()));
        }
    }
}

// 将 XML 片段解析为独立文档（用于后续复制节点到目标文档）
std::unique_ptr<xmlDoc, decltype(&xmlFreeDoc)> parseXmlFragmentDoc(std::string_view xml) {
    auto* doc = xmlReadMemory(
        xml.data(),
        static_cast<int>(xml.size()),
        nullptr,
        nullptr,
        XML_PARSE_NONET
    );
    if (doc == nullptr) {
        throw WordProcessingException("Unable to parse XML fragment");
    }
    return std::unique_ptr<xmlDoc, decltype(&xmlFreeDoc)>(doc, xmlFreeDoc);
}

// 解析 XML 片段并深拷贝其根节点到目标文档（用于从外部 XML 插入内容到 OOXML 文档）
xmlNodePtr copyXmlFragmentRoot(std::string_view xml, xmlDocPtr targetDoc) {
    auto fragmentDoc = parseXmlFragmentDoc(xml);
    auto* root = xmlDocGetRootElement(fragmentDoc.get());
    if (root == nullptr) {
        throw WordProcessingException("XML fragment has no root node");
    }

    auto* copied = xmlDocCopyNode(root, targetDoc, 1);
    if (copied == nullptr) {
        throw WordProcessingException("Unable to copy XML fragment into document");
    }
    return copied;
}

// 从任意 XML 节点向上查找所属的 w:r（Word run）元素
xmlNodePtr findWordRunForTextNode(xmlNodePtr textNode) {
    return findAncestor(textNode, "r");
}

} // namespace

// XmlPart 的 PIMPL 实现，封装 libxml2 的 XML 文档和 XPath 求值
struct XmlPart::Impl {
    xmlDocPtr doc{};  //!< libxml2 XML 文档指针

    Impl() = default;

    explicit Impl(std::string xmlContent) {
        parse(std::move(xmlContent));
    }

    ~Impl() {
        if (doc != nullptr) {
            xmlFreeDoc(doc);
        }
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    // 解析 XML 字符串，替换之前的文档内容
    void parse(std::string xmlContent) {
        if (doc != nullptr) {
            xmlFreeDoc(doc);
            doc = nullptr;
        }
        doc = xmlReadMemory(xmlContent.data(), static_cast<int>(xmlContent.size()), nullptr, nullptr, XML_PARSE_NONET);
        if (doc == nullptr) {
            throw XmlException("Unable to parse XML content");
        }
    }

    // 创建 XPath 上下文并注册 OOXML 常用的命名空间前缀
    [[nodiscard]] xmlXPathContextPtr makeXPathContext() const {
        if (doc == nullptr) {
            throw XmlException("XML document is empty");
        }

        auto* context = xmlXPathNewContext(doc);
        if (context == nullptr) {
            throw XmlException("Unable to create XPath context");
        }

        // 注册 OOXML 核心命名空间
        registerNamespace(context, "w", "http://schemas.openxmlformats.org/wordprocessingml/2006/main");
        registerNamespace(context, "r", "http://schemas.openxmlformats.org/officeDocument/2006/relationships");
        registerNamespace(context, "wp", "http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing");
        registerNamespace(context, "a", "http://schemas.openxmlformats.org/drawingml/2006/main");
        registerNamespace(context, "pic", "http://schemas.openxmlformats.org/drawingml/2006/picture");
        registerNamespace(context, "rel", "http://schemas.openxmlformats.org/package/2006/relationships");
        return context;
    }

    // 执行 XPath 表达式并返回结果（调用者负责释放）
    [[nodiscard]] xmlXPathObjectPtr eval(std::string_view xpath) const {
        auto* context = makeXPathContext();
        auto* result = xmlXPathEvalExpression(
            reinterpret_cast<const xmlChar*>(std::string(xpath).c_str()),
            context
        );
        xmlXPathFreeContext(context);
        if (result == nullptr) {
            throw XmlException("Invalid XPath expression");
        }
        return result;
    }
};

XmlPart::XmlPart() : impl_(std::make_unique<Impl>()) {}

XmlPart::XmlPart(std::string xmlContent) : impl_(std::make_unique<Impl>(std::move(xmlContent))) {}

XmlPart::~XmlPart() = default;

XmlPart::XmlPart(XmlPart&&) noexcept = default;

XmlPart& XmlPart::operator=(XmlPart&&) noexcept = default;

XmlPart XmlPart::fromString(std::string xmlContent) {
    return XmlPart(std::move(xmlContent));
}

XmlPart XmlPart::fromFile(const std::string& path) {
    return XmlPart(readTextFile(path));
}

std::string XmlPart::toString() const {
    if (empty()) {
        return {};
    }
    xmlChar* buffer = nullptr;
    int size = 0;
    xmlDocDumpMemoryEnc(impl_->doc, &buffer, &size, "UTF-8");
    if (buffer == nullptr) {
        throw XmlException("Unable to serialize XML document");
    }
    std::string result(reinterpret_cast<const char*>(buffer), static_cast<std::size_t>(size));
    xmlFree(buffer);
    return result;
}

void XmlPart::saveToFile(const std::string& path) const {
    writeTextFile(path, toString());
}

bool XmlPart::empty() const noexcept {
    return impl_ == nullptr || impl_->doc == nullptr;
}

std::vector<std::string> XmlPart::textNodes() const {
    return textsByXPath("//text()");
}

std::optional<std::string> XmlPart::firstTextByXPath(std::string_view xpath) const {
    auto texts = textsByXPath(xpath);
    if (texts.empty()) {
        return std::nullopt;
    }
    return texts.front();
}

std::vector<std::string> XmlPart::textsByXPath(std::string_view xpath) const {
    std::vector<std::string> texts;
    auto* result = impl_->eval(xpath);
    const auto cleanup = std::unique_ptr<xmlXPathObject, decltype(&xmlXPathFreeObject)>(result, xmlXPathFreeObject);
    if (result->nodesetval == nullptr) {
        return texts;
    }

    const auto count = result->nodesetval->nodeNr;
    texts.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        auto* node = result->nodesetval->nodeTab[i];
        texts.push_back(xmlStringToStd(xmlNodeGetContent(node)));
    }
    return texts;
}

// 通用的 XML 文本节点替换（替换所有 //text() 节点中的匹配文本）
bool XmlPart::replaceText(std::string_view search, std::string_view replacement, bool matchCase) {
    bool changed = false;
    auto* result = impl_->eval("//text()");
    const auto cleanup = std::unique_ptr<xmlXPathObject, decltype(&xmlXPathFreeObject)>(result, xmlXPathFreeObject);
    if (result->nodesetval == nullptr) {
        return false;
    }

    for (int i = 0; i < result->nodesetval->nodeNr; ++i) {
        auto* node = result->nodesetval->nodeTab[i];
        if (!nodeIsTextLike(node)) {
            continue;
        }

        auto text = xmlStringToStd(xmlNodeGetContent(node));
        if (detail::replaceAllInString(text, search, replacement, matchCase, false) == 0) {
            continue;
        }
        xmlNodeSetContent(node, reinterpret_cast<const xmlChar*>(text.c_str()));
        changed = true;
    }

    return changed;
}

// 替换 WordprocessingML 中跨 w:t 节点的文本（占位符可能被分割到多个 w:t 中）
// 做法：将所有 w:t 的文本合并为连续字符串，定位匹配后再回写到各自的节点
bool XmlPart::replaceWordText(std::string_view search, std::string_view replacement, bool matchCase) {
    auto* result = impl_->eval("//w:t");
    const auto cleanup = std::unique_ptr<xmlXPathObject, decltype(&xmlXPathFreeObject)>(result, xmlXPathFreeObject);
    if (result->nodesetval == nullptr) {
        return false;
    }

    std::vector<WordTextNode> nodes;
    std::string combinedText;
    nodes.reserve(static_cast<std::size_t>(result->nodesetval->nodeNr));

    for (int i = 0; i < result->nodesetval->nodeNr; ++i) {
        auto* node = result->nodesetval->nodeTab[i];
        if (node == nullptr || node->type != XML_ELEMENT_NODE) {
            continue;
        }

        auto text = xmlStringToStd(xmlNodeGetContent(node));
        const auto start = combinedText.size();
        combinedText += text;
        nodes.push_back({node, std::move(text), start, combinedText.size()});
    }

    const auto matches = findMatches(combinedText, search, matchCase);
    if (matches.empty()) {
        return false;
    }

    // 逆序处理匹配项，避免前面的替换破坏后面匹配的偏移位置
    for (auto matchIt = matches.rbegin(); matchIt != matches.rend(); ++matchIt) {
        const auto matchStart = matchIt->position;
        const auto matchEnd = matchIt->position + matchIt->length;
        const auto firstNode = firstOverlappingNode(nodes, matchStart, matchEnd);
        const auto lastNode = lastOverlappingNode(nodes, matchStart, matchEnd);
        if (firstNode == nodes.size() || lastNode == nodes.size()) {
            continue;
        }

        const auto prefixLength = matchStart - nodes[firstNode].start;
        const auto suffixOffset = matchEnd - nodes[lastNode].start;
        const auto prefix = nodes[firstNode].text.substr(0, prefixLength);
        const auto suffix = suffixOffset < nodes[lastNode].text.size()
            ? nodes[lastNode].text.substr(suffixOffset)
            : std::string{};
        nodes[firstNode].text = prefix + std::string(replacement) + suffix;
        for (std::size_t nodeIndex = firstNode + 1; nodeIndex <= lastNode; ++nodeIndex) {
            nodes[nodeIndex].text.clear();
        }
    }

    for (const auto& node : nodes) {
        xmlNodeSetContent(node.node, reinterpret_cast<const xmlChar*>(node.text.c_str()));
    }

    return true;
}

// 在指定书签所在的表格行之后插入新行
// 找到包含书签的 w:tr，对其深拷贝并填充数据后依次插入为兄弟节点
std::size_t XmlPart::insertTableRowsAtBookmark(std::string_view bookmark, const TableData& rows) {
    const auto normalizedBookmark = normalizeTableBookmark(bookmark);
    auto* result = impl_->eval("//w:t");
    const auto cleanup = std::unique_ptr<xmlXPathObject, decltype(&xmlXPathFreeObject)>(result, xmlXPathFreeObject);
    if (result->nodesetval == nullptr) {
        return 0;
    }

    xmlNodePtr templateRow = nullptr;
    for (int i = 0; i < result->nodesetval->nodeNr; ++i) {
        auto* textNode = result->nodesetval->nodeTab[i];
        auto text = xmlStringToStd(xmlNodeGetContent(textNode));
        if (text.find(normalizedBookmark) == std::string::npos) {
            continue;
        }

        templateRow = findAncestor(textNode, "tr");
        break;
    }

    if (templateRow == nullptr) {
        throw WordProcessingException("Table row bookmark not found: " + normalizedBookmark);
    }

    xmlNodePtr insertionPoint = templateRow;
    std::size_t inserted = 0;
    for (const auto& rowData : rows) {
        std::unique_ptr<xmlNode, decltype(&xmlFreeNode)> clonedRow(xmlCopyNode(templateRow, 1), xmlFreeNode);
        if (!clonedRow) {
            throw WordProcessingException("Unable to deep copy table row XML node");
        }

        removeBookmarkText(clonedRow.get(), normalizedBookmark);
        fillTableRow(clonedRow.get(), rowData);
        auto* insertedNode = xmlAddNextSibling(insertionPoint, clonedRow.get());
        if (insertedNode == nullptr) {
            throw WordProcessingException("Unable to insert copied table row XML node");
        }

        clonedRow.release();
        insertionPoint = insertedNode;
        ++inserted;
    }

    removeBookmarkText(templateRow, normalizedBookmark);
    return inserted;
}

// 用 XML 片段替换占位符文本（用于插入图片等富内容）
// 保留替换位置前后的文本内容，并在 run 节点后插入新的 XML 节点
bool XmlPart::replaceWordTextWithXml(std::string_view search, std::string_view replacementXml) {
    auto* result = impl_->eval("//w:t");
    const auto cleanup = std::unique_ptr<xmlXPathObject, decltype(&xmlXPathFreeObject)>(result, xmlXPathFreeObject);
    if (result->nodesetval == nullptr) {
        return false;
    }

    std::vector<WordTextNode> nodes;
    std::string combinedText;
    nodes.reserve(static_cast<std::size_t>(result->nodesetval->nodeNr));

    for (int i = 0; i < result->nodesetval->nodeNr; ++i) {
        auto* node = result->nodesetval->nodeTab[i];
        if (node == nullptr || node->type != XML_ELEMENT_NODE) {
            continue;
        }

        auto text = xmlStringToStd(xmlNodeGetContent(node));
        const auto start = combinedText.size();
        combinedText += text;
        nodes.push_back({node, std::move(text), start, combinedText.size()});
    }

    const auto matches = findMatches(combinedText, search, true);
    if (matches.empty()) {
        return false;
    }

    const auto& match = matches.front();
    const auto matchStart = match.position;
    const auto matchEnd = match.position + match.length;
    const auto firstNode = firstOverlappingNode(nodes, matchStart, matchEnd);
    const auto lastNode = lastOverlappingNode(nodes, matchStart, matchEnd);
    if (firstNode == nodes.size() || lastNode == nodes.size()) {
        return false;
    }

    auto* runNode = findWordRunForTextNode(nodes[firstNode].node);
    if (runNode == nullptr || runNode->parent == nullptr) {
        throw WordProcessingException("Unable to locate Word run for image placeholder");
    }

    const auto prefixLength = matchStart - nodes[firstNode].start;
    const auto suffixOffset = matchEnd - nodes[lastNode].start;
    const auto prefix = nodes[firstNode].text.substr(0, prefixLength);
    const auto suffix = suffixOffset < nodes[lastNode].text.size()
        ? nodes[lastNode].text.substr(suffixOffset)
        : std::string{};
    nodes[firstNode].text = prefix;
    for (std::size_t nodeIndex = firstNode + 1; nodeIndex < lastNode; ++nodeIndex) {
        nodes[nodeIndex].text.clear();
    }
    nodes[lastNode].text = firstNode == lastNode ? prefix + suffix : suffix;

    for (const auto& node : nodes) {
        xmlNodeSetContent(node.node, reinterpret_cast<const xmlChar*>(node.text.c_str()));
    }

    std::unique_ptr<xmlNode, decltype(&xmlFreeNode)> replacementNode(
        copyXmlFragmentRoot(replacementXml, impl_->doc),
        xmlFreeNode
    );
    auto* inserted = xmlAddNextSibling(runNode, replacementNode.get());
    if (inserted == nullptr) {
        throw WordProcessingException("Unable to insert image drawing XML");
    }
    replacementNode.release();

    return true;
}

// 设置指定 run 的文本样式（字体、颜色、大小、粗体/斜体等）
// 每个属性对应 w:rPr 下的子元素，不存在则自动创建
void XmlPart::setRunStyle(std::size_t paragraphIndex, std::size_t runIndex, const TextStyle& style) {
    auto* result = impl_->eval(runXPath(paragraphIndex, runIndex));
    const auto cleanup = std::unique_ptr<xmlXPathObject, decltype(&xmlXPathFreeObject)>(result, xmlXPathFreeObject);
    if (result->nodesetval == nullptr || result->nodesetval->nodeNr == 0) {
        throw WordProcessingException("Run index is out of range");
    }

    auto* run = result->nodesetval->nodeTab[0];
    auto* runProperties = ensureFirstChild(run, "rPr");

    if (style.fontFamily) {
        auto* fonts = ensureChild(runProperties, "rFonts");
        setWAttribute(fonts, "ascii", *style.fontFamily);
        setWAttribute(fonts, "hAnsi", *style.fontFamily);
        setWAttribute(fonts, "eastAsia", *style.fontFamily);
    }

    if (style.colorHex) {
        const auto color = upperAscii(*style.colorHex);
        if (!isHexColor(color)) {
            throw WordProcessingException("Text color must be a 6-digit RGB hex value");
        }
        auto* colorNode = ensureChild(runProperties, "color");
        setWAttribute(colorNode, "val", color);
    }

    if (style.backgroundColorHex) {
        const auto color = upperAscii(*style.backgroundColorHex);
        if (!isHexColor(color)) {
            throw WordProcessingException("Text background color must be a 6-digit RGB hex value");
        }
        auto* shading = ensureChild(runProperties, "shd");
        setWAttribute(shading, "fill", color);
    }

    if (style.fontSizeHalfPoints) {
        auto* size = ensureChild(runProperties, "sz");
        setWAttribute(size, "val", std::to_string(*style.fontSizeHalfPoints));
    }

    if (style.bold) {
        setToggleChild(runProperties, "b", *style.bold);
    }
    if (style.italic) {
        setToggleChild(runProperties, "i", *style.italic);
    }
    if (style.underline) {
        auto* underline = ensureChild(runProperties, "u");
        setWAttribute(underline, "val", *style.underline ? "single" : "none");
    }
    if (style.strike) {
        setToggleChild(runProperties, "strike", *style.strike);
    }
    if (style.superscript || style.subscript) {
        auto* verticalAlign = ensureChild(runProperties, "vertAlign");
        if (style.superscript && *style.superscript) {
            setWAttribute(verticalAlign, "val", "superscript");
        } else if (style.subscript && *style.subscript) {
            setWAttribute(verticalAlign, "val", "subscript");
        } else {
            setWAttribute(verticalAlign, "val", "baseline");
        }
    }
}

TextStyle XmlPart::runStyle(std::size_t paragraphIndex, std::size_t runIndex) const {
    auto* result = impl_->eval(runXPath(paragraphIndex, runIndex));
    const auto cleanup = std::unique_ptr<xmlXPathObject, decltype(&xmlXPathFreeObject)>(result, xmlXPathFreeObject);
    if (result->nodesetval == nullptr || result->nodesetval->nodeNr == 0) {
        throw WordProcessingException("Run index is out of range");
    }

    TextStyle style;
    auto* runProperties = firstChildElement(result->nodesetval->nodeTab[0], "rPr");
    if (runProperties == nullptr) {
        return style;
    }

    auto* fonts = firstChildElement(runProperties, "rFonts");
    if (fonts != nullptr) {
        auto font = wAttributeValue(fonts, "ascii");
        if (font.empty()) {
            font = wAttributeValue(fonts, "hAnsi");
        }
        if (font.empty()) {
            font = wAttributeValue(fonts, "eastAsia");
        }
        if (!font.empty()) {
            style.fontFamily = font;
        }
    }

    auto* color = firstChildElement(runProperties, "color");
    if (color != nullptr) {
        const auto value = wAttributeValue(color, "val");
        if (!value.empty()) {
            style.colorHex = value;
        }
    }

    auto* shading = firstChildElement(runProperties, "shd");
    if (shading != nullptr) {
        const auto value = wAttributeValue(shading, "fill");
        if (!value.empty() && value != "auto") {
            style.backgroundColorHex = value;
        }
    }

    auto* size = firstChildElement(runProperties, "sz");
    if (size != nullptr) {
        style.fontSizeHalfPoints = parseInt(wAttributeValue(size, "val"));
    }

    style.bold = boolFromToggleNode(firstChildElement(runProperties, "b"));
    style.italic = boolFromToggleNode(firstChildElement(runProperties, "i"));

    auto* underline = firstChildElement(runProperties, "u");
    if (underline != nullptr) {
        const auto value = wAttributeValue(underline, "val");
        style.underline = value != "none" && value != "0" && value != "false";
    }

    style.strike = boolFromToggleNode(firstChildElement(runProperties, "strike"));

    auto* verticalAlign = firstChildElement(runProperties, "vertAlign");
    if (verticalAlign != nullptr) {
        const auto value = wAttributeValue(verticalAlign, "val");
        style.superscript = value == "superscript";
        style.subscript = value == "subscript";
    }

    return style;
}

// 设置段落的样式（对齐、缩进、行距、段前段后间距等）
void XmlPart::setParagraphStyle(std::size_t paragraphIndex, const ParagraphStyle& style) {
    auto* result = impl_->eval(paragraphXPath(paragraphIndex));
    const auto cleanup = std::unique_ptr<xmlXPathObject, decltype(&xmlXPathFreeObject)>(result, xmlXPathFreeObject);
    if (result->nodesetval == nullptr || result->nodesetval->nodeNr == 0) {
        throw WordProcessingException("Paragraph index is out of range");
    }

    auto* paragraph = result->nodesetval->nodeTab[0];
    auto* paragraphProperties = ensureFirstChild(paragraph, "pPr");

    if (style.alignment) {
        auto* alignment = ensureChild(paragraphProperties, "jc");
        setWAttribute(alignment, "val", alignmentToXml(*style.alignment));
    }

    if (style.lineSpacingTwips || style.lineSpacingRule) {
        auto* spacing = ensureChild(paragraphProperties, "spacing");
        if (style.lineSpacingTwips) {
            setWAttribute(spacing, "line", std::to_string(*style.lineSpacingTwips));
        }
        if (style.lineSpacingRule) {
            setWAttribute(spacing, "lineRule", lineRuleToXml(*style.lineSpacingRule));
        }
    }

    if (style.spacingBeforeTwips || style.spacingAfterTwips) {
        auto* spacing = ensureChild(paragraphProperties, "spacing");
        if (style.spacingBeforeTwips) {
            setWAttribute(spacing, "before", std::to_string(*style.spacingBeforeTwips));
        }
        if (style.spacingAfterTwips) {
            setWAttribute(spacing, "after", std::to_string(*style.spacingAfterTwips));
        }
    }

    if (style.firstLineIndentTwips || style.leftIndentTwips || style.rightIndentTwips) {
        auto* indent = ensureChild(paragraphProperties, "ind");
        if (style.firstLineIndentTwips) {
            setWAttribute(indent, "firstLine", std::to_string(*style.firstLineIndentTwips));
        }
        if (style.leftIndentTwips) {
            setWAttribute(indent, "left", std::to_string(*style.leftIndentTwips));
        }
        if (style.rightIndentTwips) {
            setWAttribute(indent, "right", std::to_string(*style.rightIndentTwips));
        }
    }
}

ParagraphStyle XmlPart::paragraphStyle(std::size_t paragraphIndex) const {
    auto* result = impl_->eval(paragraphXPath(paragraphIndex));
    const auto cleanup = std::unique_ptr<xmlXPathObject, decltype(&xmlXPathFreeObject)>(result, xmlXPathFreeObject);
    if (result->nodesetval == nullptr || result->nodesetval->nodeNr == 0) {
        throw WordProcessingException("Paragraph index is out of range");
    }

    ParagraphStyle style;
    auto* paragraphProperties = firstChildElement(result->nodesetval->nodeTab[0], "pPr");
    if (paragraphProperties == nullptr) {
        return style;
    }

    auto* alignment = firstChildElement(paragraphProperties, "jc");
    if (alignment != nullptr) {
        style.alignment = alignmentFromXml(wAttributeValue(alignment, "val"));
    }

    auto* spacing = firstChildElement(paragraphProperties, "spacing");
    if (spacing != nullptr) {
        style.lineSpacingTwips = parseInt(wAttributeValue(spacing, "line"));
        style.lineSpacingRule = lineRuleFromXml(wAttributeValue(spacing, "lineRule"));
        style.spacingBeforeTwips = parseInt(wAttributeValue(spacing, "before"));
        style.spacingAfterTwips = parseInt(wAttributeValue(spacing, "after"));
    }

    auto* indent = firstChildElement(paragraphProperties, "ind");
    if (indent != nullptr) {
        style.firstLineIndentTwips = parseInt(wAttributeValue(indent, "firstLine"));
        style.leftIndentTwips = parseInt(wAttributeValue(indent, "left"));
        style.rightIndentTwips = parseInt(wAttributeValue(indent, "right"));
    }

    return style;
}

void XmlPart::setTextByXPath(std::string_view xpath, std::string_view value) {
    auto* result = impl_->eval(xpath);
    const auto cleanup = std::unique_ptr<xmlXPathObject, decltype(&xmlXPathFreeObject)>(result, xmlXPathFreeObject);
    if (result->nodesetval == nullptr || result->nodesetval->nodeNr == 0) {
        throw XmlException("XPath did not match any XML nodes");
    }
    xmlNodeSetContent(result->nodesetval->nodeTab[0], reinterpret_cast<const xmlChar*>(std::string(value).c_str()));
}

void XmlPart::appendChildXml(std::string_view parentXPath, std::string_view childXml) {
    auto* result = impl_->eval(parentXPath);
    const auto cleanup = std::unique_ptr<xmlXPathObject, decltype(&xmlXPathFreeObject)>(result, xmlXPathFreeObject);
    if (result->nodesetval == nullptr || result->nodesetval->nodeNr == 0) {
        throw WordProcessingException("Parent XPath did not match any XML nodes");
    }

    auto* parent = result->nodesetval->nodeTab[0];
    std::unique_ptr<xmlNode, decltype(&xmlFreeNode)> child(
        copyXmlFragmentRoot(childXml, impl_->doc),
        xmlFreeNode
    );
    auto* inserted = xmlAddChild(parent, child.get());
    if (inserted == nullptr) {
        throw WordProcessingException("Unable to append XML child");
    }
    child.release();
}

std::size_t XmlPart::countByXPath(std::string_view xpath) const {
    auto* result = impl_->eval(xpath);
    const auto cleanup = std::unique_ptr<xmlXPathObject, decltype(&xmlXPathFreeObject)>(result, xmlXPathFreeObject);
    if (result->nodesetval == nullptr) {
        return 0;
    }
    return static_cast<std::size_t>(result->nodesetval->nodeNr);
}

void XmlPart::setTextsByXPath(std::string_view xpath, const std::vector<std::string>& values) {
    auto* result = impl_->eval(xpath);
    const auto cleanup = std::unique_ptr<xmlXPathObject, decltype(&xmlXPathFreeObject)>(result, xmlXPathFreeObject);
    if (result->nodesetval == nullptr) {
        return;
    }

    const auto count = std::min<int>(result->nodesetval->nodeNr, static_cast<int>(values.size()));
    for (int i = 0; i < count; ++i) {
        xmlNodeSetContent(result->nodesetval->nodeTab[i], reinterpret_cast<const xmlChar*>(values[static_cast<std::size_t>(i)].c_str()));
    }
}

bool XmlPart::hasXPath(std::string_view xpath) const {
    auto* result = impl_->eval(xpath);
    const auto cleanup = std::unique_ptr<xmlXPathObject, decltype(&xmlXPathFreeObject)>(result, xmlXPathFreeObject);
    return result->nodesetval != nullptr && result->nodesetval->nodeNr > 0;
}

} // namespace cppwordkit
