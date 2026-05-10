#include "OpenXmlPackage.hpp"

#include "cppwordkit/Error.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
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

constexpr const char* documentRelsXml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"/>
)";

std::vector<std::uint8_t> readBinaryFile(const Path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw PackageException("Unable to open binary file: " + path.string());
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string lowerAscii(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string imageContentType(std::string_view extension) {
    if (extension == "jpg" || extension == "jpeg") {
        return "image/jpeg";
    }
    if (extension == "png") {
        return "image/png";
    }
    if (extension == "gif") {
        return "image/gif";
    }
    if (extension == "bmp") {
        return "image/bmp";
    }
    throw PackageException("Unsupported image extension: " + std::string(extension));
}

std::string stripLeadingDot(std::string extension) {
    if (!extension.empty() && extension.front() == '.') {
        extension.erase(extension.begin());
    }
    return lowerAscii(std::move(extension));
}

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
    package.archive_.setText("word/_rels/document.xml.rels", documentRelsXml);
    package.loadXmlPart("[Content_Types].xml");
    package.loadXmlPart("_rels/.rels");
    package.loadXmlPart("word/document.xml");
    package.loadXmlPart("word/_rels/document.xml.rels");
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

std::optional<std::vector<std::uint8_t>> OpenXmlPackage::rawPart(std::string_view name) const {
    const auto key = std::string(name);
    if (!archive_.contains(key)) {
        return std::nullopt;
    }
    return archive_.bytes(key);
}

std::string OpenXmlPackage::addImagePart(const Path& imagePath) {
    const auto extension = stripLeadingDot(imagePath.extension().string());
    const auto contentType = imageContentType(extension);
    ensureContentTypeDefault(extension, contentType);

    std::size_t index = 1;
    std::string partName;
    do {
        partName = "word/media/image" + std::to_string(index++) + "." + extension;
    } while (archive_.contains(partName));

    archive_.setBytes(partName, readBinaryFile(imagePath));
    return partName;
}

std::string OpenXmlPackage::addMainDocumentRelationship(std::string_view type, std::string_view target) {
    constexpr const char* relsPart = "word/_rels/document.xml.rels";
    if (!archive_.contains(relsPart)) {
        archive_.setText(relsPart, documentRelsXml);
        loadXmlPart(relsPart);
    } else if (!xmlParts_.contains(relsPart)) {
        loadXmlPart(relsPart);
    }

    auto& rels = xmlPart(relsPart);
    const auto count = rels.countByXPath("//rel:Relationship");
    auto nextId = count + 1;
    std::string relationshipId;
    do {
        relationshipId = "rId" + std::to_string(nextId++);
    } while (rels.hasXPath("//rel:Relationship[@Id='" + relationshipId + "']"));

    std::ostringstream xml;
    xml << "<Relationship xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\" "
        << "Id=\"" << relationshipId << "\" "
        << "Type=\"" << std::string(type) << "\" "
        << "Target=\"" << std::string(target) << "\"/>";
    rels.appendChildXml("/*[local-name()='Relationships']", xml.str());
    return relationshipId;
}

void OpenXmlPackage::ensureContentTypeDefault(std::string_view extension, std::string_view contentType) {
    auto& contentTypes = xmlPart("[Content_Types].xml");
    const auto xpath = "/*[local-name()='Types']/*[local-name()='Default'][@Extension='" + std::string(extension) + "']";
    if (contentTypes.hasXPath(xpath)) {
        return;
    }

    std::ostringstream xml;
    xml << "<Default xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\" "
        << "Extension=\"" << std::string(extension) << "\" "
        << "ContentType=\"" << std::string(contentType) << "\"/>";
    contentTypes.appendChildXml("/*[local-name()='Types']", xml.str());
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
