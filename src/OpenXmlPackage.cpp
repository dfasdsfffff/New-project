#include "OpenXmlPackage.hpp"

#include "cppwordkit/Error.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace cppwordkit {
namespace {

bool endsWith(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}

constexpr const char* contentTypesXml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Default Extension="xml" ContentType="application/xml"/>
  <Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>
</Types>
)";

constexpr const char* rootRelsXml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>
</Relationships>
)";

constexpr const char* documentXml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">
  <w:body>
    <w:p>
      <w:r>
        <w:t></w:t>
      </w:r>
    </w:p>
    <w:sectPr/>
  </w:body>
</w:document>
)";

} // namespace

OpenXmlPackage OpenXmlPackage::open(const Path& path) {
    OpenXmlPackage package;
    package.archive_ = ZipArchive::read(path);
    if (!package.archive_.contains("word/document.xml")) {
        throw PackageException("Package does not contain word/document.xml");
    }
    for (const auto& name : package.archive_.names()) {
        if (endsWith(name, ".xml") || endsWith(name, ".rels")) {
            package.loadXmlPart(name);
        }
    }
    return package;
}

OpenXmlPackage OpenXmlPackage::createBlankDocument() {
    OpenXmlPackage package;
    package.archive_.setText("[Content_Types].xml", contentTypesXml);
    package.archive_.setText("_rels/.rels", rootRelsXml);
    package.archive_.setText("word/document.xml", documentXml);
    package.loadXmlPart("[Content_Types].xml");
    package.loadXmlPart("_rels/.rels");
    package.loadXmlPart("word/document.xml");
    return package;
}

void OpenXmlPackage::saveAs(const Path& path) const {
    auto archive = archive_;
    for (const auto& [name, part] : xmlParts_) {
        archive.setText(name, part.toString());
    }
    archive.write(path);
}

bool OpenXmlPackage::hasPart(std::string_view name) const {
    return xmlParts_.contains(std::string(name)) || archive_.contains(std::string(name));
}

XmlPart& OpenXmlPackage::xmlPart(std::string_view name) {
    const auto key = std::string(name);
    const auto it = xmlParts_.find(key);
    if (it == xmlParts_.end()) {
        throw PackageException("XML part not found: " + key);
    }
    return it->second;
}

const XmlPart& OpenXmlPackage::xmlPart(std::string_view name) const {
    const auto key = std::string(name);
    const auto it = xmlParts_.find(key);
    if (it == xmlParts_.end()) {
        throw PackageException("XML part not found: " + key);
    }
    return it->second;
}

std::optional<XmlPart*> OpenXmlPackage::findXmlPart(std::string_view name) {
    const auto it = xmlParts_.find(std::string(name));
    if (it == xmlParts_.end()) {
        return std::nullopt;
    }
    return &it->second;
}

std::vector<std::string> OpenXmlPackage::partNames() const {
    auto names = archive_.names();
    std::ranges::sort(names);
    return names;
}

void OpenXmlPackage::loadXmlPart(const std::string& name) {
    xmlParts_.insert_or_assign(name, XmlPart::fromString(archive_.text(name)));
}

void OpenXmlPackage::flushXmlParts() {
    for (const auto& [name, part] : xmlParts_) {
        archive_.setText(name, part.toString());
    }
}

} // namespace cppwordkit
