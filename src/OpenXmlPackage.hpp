#pragma once
//! @file OpenXmlPackage.hpp OOXML 包的封装，管理 ZIP 归档中的 XML 部件和图像资源

#include "ZipArchive.hpp"
#include "cppwordkit/XmlPart.hpp"

#include <optional>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace cppwordkit {

//! 表示一个 OOXML（Office Open XML）包，本质上是包含特定文件的 ZIP 归档
//! 负责管理 ZIP 中的 XML 部件读取/写入、Content Types 和图像资源添加
class OpenXmlPackage {
public:
    //! 从文件路径打开现有 OOXML 包
    static OpenXmlPackage open(const Path& path);
    //! 创建一个空白 OOXML 文档包（含最小必需的 XML 骨架）
    static OpenXmlPackage createBlankDocument();

    void saveAs(const Path& path) const;

    [[nodiscard]] bool hasPart(std::string_view name) const;
    [[nodiscard]] XmlPart& xmlPart(std::string_view name);
    [[nodiscard]] const XmlPart& xmlPart(std::string_view name) const;
    [[nodiscard]] std::optional<XmlPart*> findXmlPart(std::string_view name);
    [[nodiscard]] std::vector<std::string> partNames() const;
    //! 获取 ZIP 中原始二进制部件（非 XML，如图片）
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> rawPart(std::string_view name) const;
    //! 向包中添加图像部件，返回生成的部件路径（如 word/media/image1.png）
    [[nodiscard]] std::string addImagePart(const Path& imagePath);
    //! 向文档关系文件（word/_rels/document.xml.rels）添加新关系，返回生成的 rId
    [[nodiscard]] std::string addMainDocumentRelationship(std::string_view type, std::string_view target);
    //! 确保 [Content_Types].xml 中包含指定扩展名的默认内容类型
    void ensureContentTypeDefault(std::string_view extension, std::string_view contentType);

private:
    ZipArchive archive_;                                //!< 底层的 ZIP 归档
    std::unordered_map<std::string, XmlPart> xmlParts_; //!< 已解析的 XML 部件缓存

    void loadXmlPart(const std::string& name);
    void flushXmlParts();
};

} // namespace cppwordkit
