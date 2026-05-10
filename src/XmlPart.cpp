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
#include <utility>

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

void XmlPart::setTextByXPath(std::string_view xpath, std::string_view value) {
    auto* result = impl_->eval(xpath);
    const auto cleanup = std::unique_ptr<xmlXPathObject, decltype(&xmlXPathFreeObject)>(result, xmlXPathFreeObject);
    if (result->nodesetval == nullptr || result->nodesetval->nodeNr == 0) {
        throw XmlException("XPath did not match any XML nodes");
    }
    xmlNodeSetContent(result->nodesetval->nodeTab[0], reinterpret_cast<const xmlChar*>(std::string(value).c_str()));
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
