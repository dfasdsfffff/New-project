//! @file OpenXmlPackage.cpp OOXML 包管理实现，包括空白文档创建、部件读取/写入和图像添加

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

// 空白文档的 [Content_Types].xml 模板
constexpr const char* contentTypesXml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Default Extension="xml" ContentType="application/xml"/>
  <Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>
</Types>
)";

// 空白文档的根关系文件（_rels/.rels）模板
constexpr const char* rootRelsXml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>
</Relationships>
)";

// 空白文档的主文档（word/document.xml）模板，含一个空段落和节属性
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

// 空白文档的文档关系文件模板
constexpr const char* documentRelsXml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"/>
)";

// 读取二进制文件全部内容
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

// 根据文件扩展名返回对应的 MIME 类型
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

// 去除扩展名前导点号并统一为小写
std::string stripLeadingDot(std::string extension) {
    if (!extension.empty() && extension.front() == '.') {
        extension.erase(extension.begin());
    }
    return lowerAscii(std::move(extension));
}

} // namespace

//! 打开现有 OOXML 包：读取 ZIP，校验包含主文档，自动加载所有 XML 和关系部件
OpenXmlPackage OpenXmlPackage::open(const Path& path) {
    OpenXmlPackage package;
    package.archive_ = ZipArchive::read(path);
    if (!package.archive_.contains("word/document.xml")) {
        throw PackageException("Package does not contain word/document.xml");
    }
    // 预加载所有 XML 部件到内存，便于后续频繁读取
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

//! 保存 OOXML 包：先将内存中修改过的 XML 部件刷回 ZIP 归档，再写入文件
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

//! 添加图像部件到包中：复制图像文件到 word/media/，同时更新 [Content_Types].xml
//! @return 生成的部件路径，如 "word/media/image1.png"
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
