#include "cppwordkit/XmlPart.hpp"

#include "Detail.hpp"
#include "cppwordkit/Error.hpp"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppwordkit {
namespace {

std::string xmlStringToStd(xmlChar* value) {
    if (value == nullptr) {
        return {};
    }
    std::string result(reinterpret_cast<const char*>(value));
    xmlFree(value);
    return result;
}

bool isElementNamed(xmlNodePtr node, const char* localName) {
    return node != nullptr &&
        node->type == XML_ELEMENT_NODE &&
        xmlStrEqual(node->name, reinterpret_cast<const xmlChar*>(localName)) == 1;
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

bool registerNamespace(xmlXPathContextPtr context, const char* prefix, const char* href) {
    return xmlXPathRegisterNs(
        context,
        reinterpret_cast<const xmlChar*>(prefix),
        reinterpret_cast<const xmlChar*>(href)
    ) == 0;
}

struct WordTextNode {
    xmlNodePtr node{};
    std::string text;
    std::size_t start{};
    std::size_t end{};
};

struct TextMatch {
    std::size_t position{};
    std::size_t length{};
};

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

xmlNodePtr findAncestor(xmlNodePtr node, const char* localName) {
    for (auto* current = node; current != nullptr; current = current->parent) {
        if (isElementNamed(current, localName)) {
            return current;
        }
    }
    return nullptr;
}

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

xmlNodePtr findWordRunForTextNode(xmlNodePtr textNode) {
    return findAncestor(textNode, "r");
}

} // namespace

struct XmlPart::Impl {
    xmlDocPtr doc{};

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

    [[nodiscard]] xmlXPathContextPtr makeXPathContext() const {
        if (doc == nullptr) {
            throw XmlException("XML document is empty");
        }

        auto* context = xmlXPathNewContext(doc);
        if (context == nullptr) {
            throw XmlException("Unable to create XPath context");
        }

        registerNamespace(context, "w", "http://schemas.openxmlformats.org/wordprocessingml/2006/main");
        registerNamespace(context, "r", "http://schemas.openxmlformats.org/officeDocument/2006/relationships");
        registerNamespace(context, "wp", "http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing");
        registerNamespace(context, "a", "http://schemas.openxmlformats.org/drawingml/2006/main");
        registerNamespace(context, "pic", "http://schemas.openxmlformats.org/drawingml/2006/picture");
        registerNamespace(context, "rel", "http://schemas.openxmlformats.org/package/2006/relationships");
        return context;
    }

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
