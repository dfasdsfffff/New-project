//! @file WordDocument.cpp Word 文档操作实现：段落/文本编辑、表格操作、图像插入、文本替换与模板渲染

#include "cppwordkit/WordDocument.hpp"

#include "Detail.hpp"
#include "OpenXmlPackage.hpp"
#include "cppwordkit/Error.hpp"
#include "cppwordkit/XmlPart.hpp"

#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppwordkit {
namespace {

// 主文档在 OOXML 包中的固定路径
constexpr const char* mainDocumentPath = "word/document.xml";
// 图像关系的固定类型 URI
constexpr const char* imageRelationshipType = "http://schemas.openxmlformats.org/officeDocument/2006/relationships/image";

// XPath 索引从 1 开始，辅助函数将 0-based 转为 1-based
std::string xPathIndex(std::size_t index) {
    return std::to_string(index + 1);
}

std::string bodyParagraphXPath(std::size_t index) {
    return "(/w:document/w:body/w:p)[" + xPathIndex(index) + "]";
}

// 高层 API 仅索引正文直属段落；底层 XmlPart 索引包含表格内的所有段落。
std::size_t xmlParagraphIndex(const XmlPart& part, std::size_t index) {
    if (index >= part.countByXPath("/w:document/w:body/w:p")) {
        throw WordProcessingException("Paragraph index is out of range");
    }
    return part.countByXPath(bodyParagraphXPath(index) + "/preceding::w:p");
}

// 前置声明
std::string makeInlineImageRunXml(std::string_view relationshipId, const ImageOptions& options);

// XPath: 定位第 tableIndex 个表格
std::string tableXPath(std::size_t tableIndex) {
    return "(//w:tbl)[" + xPathIndex(tableIndex) + "]";
}

// XPath: 定位表格中的指定单元格
std::string cellXPath(std::size_t tableIndex, std::size_t rowIndex, std::size_t cellIndex) {
    return tableXPath(tableIndex) +
        "/w:tr[" + xPathIndex(rowIndex) + "]" +
        "/w:tc[" + xPathIndex(cellIndex) + "]";
}

void ensureTableIndex(std::size_t tableIndex, std::size_t tableCount) {
    if (tableIndex >= tableCount) {
        throw WordException("Table index is out of range");
    }
}

// 规范化占位符格式：如果已经是 {{...}} 格式则原样返回，否则补全
std::string normalizePlaceholder(std::string_view placeholder) {
    if (placeholder.empty()) {
        throw WordException("Placeholder must not be empty");
    }

    if (placeholder.size() >= 4 &&
        placeholder.substr(0, 2) == "{{" &&
        placeholder.substr(placeholder.size() - 2) == "}}") {
        return std::string(placeholder);
    }

    return "{{" + std::string(placeholder) + "}}";
}

// XML 文本内容转义（五个预定义实体）
std::string xmlEscape(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        case '"':
            escaped += "&quot;";
            break;
        case '\'':
            escaped += "&apos;";
            break;
        default:
            escaped += ch;
            break;
        }
    }
    return escaped;
}

// XML 属性值转义（复用文本转义逻辑，因五个实体的规则一致）
std::string xmlAttributeEscape(std::string_view value) {
    return xmlEscape(value);
}

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

bool styleDefaultFromXml(std::string_view value) {
    return value == "1" || value == "true" || value == "on";
}

std::string styleTypeToXml(StyleType type) {
    switch (type) {
    case StyleType::Paragraph:
        return "paragraph";
    case StyleType::Character:
        return "character";
    }
    return "paragraph";
}

std::vector<StyleInfo> readStylesByType(const XmlPart& stylesPart, StyleType type) {
    const auto typeName = styleTypeToXml(type);
    const auto styleSetPath = "//w:style[@w:type='" + typeName + "']";
    const auto styleCount = stylesPart.countByXPath(styleSetPath);

    std::vector<StyleInfo> styles;
    styles.reserve(styleCount);

    for (std::size_t styleIndex = 0; styleIndex < styleCount; ++styleIndex) {
        const auto stylePath = "(" + styleSetPath + ")[" + xPathIndex(styleIndex) + "]";
        StyleInfo style;
        style.type = type;

        const auto ids = stylesPart.textsByXPath(stylePath + "/@w:styleId");
        if (!ids.empty()) {
            style.styleId = ids.front();
        }

        const auto names = stylesPart.textsByXPath(stylePath + "/w:name/@w:val");
        if (!names.empty()) {
            style.name = names.front();
        }

        const auto defaults = stylesPart.textsByXPath(stylePath + "/@w:default");
        if (!defaults.empty()) {
            style.isDefault = styleDefaultFromXml(defaults.front());
        }

        styles.push_back(std::move(style));
    }

    return styles;
}

// 构建 w:rPr（run 属性）的 XML 片段，参数不生效时不输出任何内容
void appendRunProperties(std::ostringstream& xml, const TextStyle& style) {
    if (!style.fontFamily &&
        !style.colorHex &&
        !style.backgroundColorHex &&
        !style.highlightColor &&
        !style.borderColorHex &&
        !style.fontSizeHalfPoints &&
        !style.bold &&
        !style.italic &&
        !style.underline &&
        !style.strike &&
        !style.superscript &&
        !style.subscript &&
        !style.enclosedCircle) {
        return;
    }

    xml << "<w:rPr>";
    if (style.fontFamily) {
        const auto font = xmlAttributeEscape(*style.fontFamily);
        xml << "<w:rFonts w:ascii=\"" << font << "\" w:hAnsi=\"" << font << "\" w:eastAsia=\"" << font << "\"/>";
    }
    if (style.colorHex) {
        xml << "<w:color w:val=\"" << xmlAttributeEscape(*style.colorHex) << "\"/>";
    }
    if (style.backgroundColorHex) {
        xml << "<w:shd w:fill=\"" << xmlAttributeEscape(*style.backgroundColorHex) << "\"/>";
    }
    if (style.highlightColor) {
        xml << "<w:highlight w:val=\"" << xmlAttributeEscape(*style.highlightColor) << "\"/>";
    }
    if (style.borderColorHex) {
        xml << "<w:bdr w:val=\"single\" w:color=\"" << xmlAttributeEscape(*style.borderColorHex) << "\"/>";
    }
    if (style.fontSizeHalfPoints) {
        xml << "<w:sz w:val=\"" << *style.fontSizeHalfPoints << "\"/>";
    }
    // 以下粗体/斜体等 toggle 属性：val="0" 表示关闭，不写 val 或 val="1" 表示开启
    if (style.bold) {
        xml << "<w:b";
        if (!*style.bold) {
            xml << " w:val=\"0\"";
        }
        xml << "/>";
    }
    if (style.italic) {
        xml << "<w:i";
        if (!*style.italic) {
            xml << " w:val=\"0\"";
        }
        xml << "/>";
    }
    if (style.underline) {
        xml << "<w:u w:val=\"" << (*style.underline ? "single" : "none") << "\"/>";
    }
    if (style.strike) {
        xml << "<w:strike";
        if (!*style.strike) {
            xml << " w:val=\"0\"";
        }
        xml << "/>";
    }
    if (style.superscript && *style.superscript) {
        xml << "<w:vertAlign w:val=\"superscript\"/>";
    } else if (style.subscript && *style.subscript) {
        xml << "<w:vertAlign w:val=\"subscript\"/>";
    } else if (style.superscript || style.subscript) {
        xml << "<w:vertAlign w:val=\"baseline\"/>";
    }
    if (style.enclosedCircle && *style.enclosedCircle) {
        xml << "<w:em w:val=\"circle\"/>";
    }
    xml << "</w:rPr>";
}

void appendParagraphProperties(std::ostringstream& xml, const ParagraphStyle& style) {
    if (!style.alignment &&
        !style.lineSpacingTwips &&
        !style.lineSpacingRule &&
        !style.spacingBeforeTwips &&
        !style.spacingAfterTwips &&
        !style.firstLineIndentTwips &&
        !style.leftIndentTwips &&
        !style.rightIndentTwips) {
        return;
    }

    xml << "<w:pPr>";
    if (style.alignment) {
        xml << "<w:jc w:val=\"" << alignmentToXml(*style.alignment) << "\"/>";
    }
    if (style.lineSpacingTwips || style.lineSpacingRule || style.spacingBeforeTwips || style.spacingAfterTwips) {
        xml << "<w:spacing";
        if (style.lineSpacingTwips) {
            xml << " w:line=\"" << *style.lineSpacingTwips << "\"";
        }
        if (style.lineSpacingRule) {
            xml << " w:lineRule=\"" << lineRuleToXml(*style.lineSpacingRule) << "\"";
        }
        if (style.spacingBeforeTwips) {
            xml << " w:before=\"" << *style.spacingBeforeTwips << "\"";
        }
        if (style.spacingAfterTwips) {
            xml << " w:after=\"" << *style.spacingAfterTwips << "\"";
        }
        xml << "/>";
    }
    if (style.firstLineIndentTwips || style.leftIndentTwips || style.rightIndentTwips) {
        xml << "<w:ind";
        if (style.firstLineIndentTwips) {
            xml << " w:firstLine=\"" << *style.firstLineIndentTwips << "\"";
        }
        if (style.leftIndentTwips) {
            xml << " w:left=\"" << *style.leftIndentTwips << "\"";
        }
        if (style.rightIndentTwips) {
            xml << " w:right=\"" << *style.rightIndentTwips << "\"";
        }
        xml << "/>";
    }
    xml << "</w:pPr>";
}

std::string runXml(const Run& run) {
    std::ostringstream xml;
    if (!run.text.empty() || run.images.empty()) {
        xml << "<w:r>";
        appendRunProperties(xml, run.style);
        xml << "<w:t xml:space=\"preserve\">" << xmlEscape(run.text) << "</w:t>";
        xml << "</w:r>";
    }
    for (const auto& image : run.images) {
        auto options = image.options;
        if (!image.description.empty()) {
            options.description = image.description;
        }
        xml << makeInlineImageRunXml(image.image.relationshipId, options);
    }
    return xml.str();
}

std::string paragraphXml(const Paragraph& paragraph) {
    std::ostringstream xml;
    xml << "<w:p>";
    appendParagraphProperties(xml, paragraph.style);
    if (paragraph.runs.empty()) {
        xml << "<w:r><w:t></w:t></w:r>";
    } else {
        for (const auto& run : paragraph.runs) {
            xml << runXml(run);
        }
    }
    xml << "</w:p>";
    return xml.str();
}

// 独立 XML 片段必须自行声明其使用的命名空间。
std::string standaloneParagraphXml(const Paragraph& paragraph) {
    auto xml = paragraphXml(paragraph);
    xml.replace(0, 5,
        "<w:p xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
        "xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\" "
        "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
        "xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">");
    return xml;
}

std::string tableXml(const Table& table) {
    std::ostringstream xml;
    xml << "<w:tbl>";
    for (const auto& row : table.rows) {
        xml << "<w:tr>";
        for (const auto& cell : row) {
            xml << "<w:tc><w:p><w:r><w:t xml:space=\"preserve\">"
                << xmlEscape(cell.text)
                << "</w:t></w:r></w:p></w:tc>";
        }
        xml << "</w:tr>";
    }
    xml << "</w:tbl>";
    return xml.str();
}

// 根据 DocumentModel 生成完整的 word/document.xml 字符串
void appendBodyBlockXml(std::ostringstream& xml, const BodyBlock& block) {
    switch (block.type) {
    case BodyBlockType::Paragraph:
        xml << paragraphXml(block.paragraph);
        break;
    case BodyBlockType::Table:
        xml << tableXml(block.table);
        break;
    }
}

BodyBlock* paragraphBlockAt(DocumentModel& model, std::size_t paragraphIndex) {
    std::size_t currentParagraph = 0;
    for (auto& block : model.bodyBlocks) {
        if (block.type != BodyBlockType::Paragraph) {
            continue;
        }
        if (currentParagraph == paragraphIndex) {
            return &block;
        }
        ++currentParagraph;
    }
    return nullptr;
}

std::string documentXmlFromModel(const DocumentModel& model) {
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        << "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
        << "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
        << "xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\" "
        << "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
        << "xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">"
        << "<w:body>";
    if (!model.bodyBlocks.empty()) {
        for (const auto& block : model.bodyBlocks) {
            appendBodyBlockXml(xml, block);
        }
    } else {
        for (const auto& paragraph : model.paragraphs) {
            xml << paragraphXml(paragraph);
        }
        for (const auto& table : model.tables) {
            xml << tableXml(table);
        }
    }
    // 写入节属性（页面尺寸和边距）
    xml << "<w:sectPr>";
    const auto& ps = model.pageSetup;
    if (ps.pageWidthTwips || ps.pageHeightTwips) {
        xml << "<w:pgSz";
        if (ps.pageWidthTwips)  xml << " w:w=\"" << *ps.pageWidthTwips << "\"";
        if (ps.pageHeightTwips) xml << " w:h=\"" << *ps.pageHeightTwips << "\"";
        xml << "/>";
    }
    if (ps.marginTopTwips || ps.marginBottomTwips || ps.marginLeftTwips || ps.marginRightTwips) {
        xml << "<w:pgMar";
        if (ps.marginTopTwips)    xml << " w:top=\"" << *ps.marginTopTwips << "\"";
        if (ps.marginBottomTwips) xml << " w:bottom=\"" << *ps.marginBottomTwips << "\"";
        if (ps.marginLeftTwips)   xml << " w:left=\"" << *ps.marginLeftTwips << "\"";
        if (ps.marginRightTwips)  xml << " w:right=\"" << *ps.marginRightTwips << "\"";
        xml << "/>";
    }
    xml << "</w:sectPr></w:body></w:document>";
    return xml.str();
}

// 将媒体部件的内部路径转为关系文件中的 target（去掉 "word/" 前缀）
std::string mediaRelationshipTarget(const std::string& mediaPartName) {
    constexpr std::string_view prefix = "word/";
    if (mediaPartName.rfind(prefix, 0) == 0) {
        return mediaPartName.substr(prefix.size());
    }
    return mediaPartName;
}

// 将关系文件中的 target 转为媒体部件的内部路径（补上 "word/" 前缀）
std::string mediaPartNameFromRelationshipTarget(std::string target) {
    constexpr std::string_view prefix = "word/";
    if (target.rfind(prefix, 0) == 0) {
        return target;
    }
    return "word/" + target;
}

std::string makeInlineImageRunXml(
    std::string_view relationshipId,
    const ImageOptions& options
) {
    std::ostringstream xml;
    xml << "<w:r xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
        << "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
        << "xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\" "
        << "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
        << "xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">"
        << "<w:drawing>"
        << "<wp:inline distT=\"0\" distB=\"0\" distL=\"0\" distR=\"0\">"
        << "<wp:extent cx=\"" << options.widthEmu << "\" cy=\"" << options.heightEmu << "\"/>"
        << "<wp:effectExtent l=\"0\" t=\"0\" r=\"0\" b=\"0\"/>"
        << "<wp:docPr id=\"1\" name=\"Picture\" descr=\"" << xmlEscape(options.description) << "\"/>"
        << "<wp:cNvGraphicFramePr>"
        << "<a:graphicFrameLocks noChangeAspect=\"1\"/>"
        << "</wp:cNvGraphicFramePr>"
        << "<a:graphic>"
        << "<a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">"
        << "<pic:pic>"
        << "<pic:nvPicPr>"
        << "<pic:cNvPr id=\"0\" name=\"" << xmlEscape(options.description) << "\"/>"
        << "<pic:cNvPicPr/>"
        << "</pic:nvPicPr>"
        << "<pic:blipFill>"
        << "<a:blip r:embed=\"" << std::string(relationshipId) << "\"/>"
        << "<a:stretch><a:fillRect/></a:stretch>"
        << "</pic:blipFill>"
        << "<pic:spPr>"
        << "<a:xfrm>"
        << "<a:off x=\"0\" y=\"0\"/>"
        << "<a:ext cx=\"" << options.widthEmu << "\" cy=\"" << options.heightEmu << "\"/>"
        << "</a:xfrm>"
        << "<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom>"
        << "</pic:spPr>"
        << "</pic:pic>"
        << "</a:graphicData>"
        << "</a:graphic>"
        << "</wp:inline>"
        << "</w:drawing>"
        << "</w:r>";
    return xml.str();
}

} // namespace

// WordDocument 的 PIMPL 实现
struct WordDocument::Impl {
    Path sourcePath;          //!< 文档的原始文件路径
    OpenXmlPackage package;   //!< 底层的 OOXML 包
    bool open = false;        //!< 文档是否已成功打开
};

WordDocument::WordDocument() : impl_(std::make_unique<Impl>()) {}

WordDocument::~WordDocument() = default;

WordDocument::WordDocument(WordDocument&&) noexcept = default;

WordDocument& WordDocument::operator=(WordDocument&&) noexcept = default;

WordDocument WordDocument::open(const Path& path) {
    WordDocument document;
    document.impl_->sourcePath = path;
    document.impl_->package = OpenXmlPackage::open(path);
    document.impl_->open = true;
    return document;
}

WordDocument WordDocument::create() {
    WordDocument document;
    document.impl_->package = OpenXmlPackage::createBlankDocument();
    document.impl_->open = true;
    return document;
}

void WordDocument::save() {
    if (impl_->sourcePath.empty()) {
        throw WordException("Cannot save a document without a source path; call saveAs instead");
    }
    saveAs(impl_->sourcePath);
}

void WordDocument::saveAs(const Path& path, const SaveOptions& options) {
    if (std::filesystem::exists(path) && !options.overwrite) {
        throw WordException("Target file already exists: " + path.string());
    }
    impl_->package.saveAs(path, options.overwrite);
    impl_->sourcePath = path;
    impl_->open = true;
}

bool WordDocument::isOpen() const noexcept {
    return impl_ != nullptr && impl_->open;
}

Path WordDocument::path() const {
    return impl_->sourcePath;
}

//! 获取主文档正文所有文本的拼接结果（不含 XML 标签）
std::string WordDocument::text() const {
    const auto runs = mainDocumentPart().textsByXPath("//w:t");
    std::ostringstream output;
    for (const auto& run : runs) {
        output << run;
    }
    return output.str();
}

std::vector<TextRun> WordDocument::textRuns() const {
    std::vector<TextRun> runs;
    const auto paragraphTexts = mainDocumentPart().textsByXPath("//w:p");
    for (std::size_t paragraphIndex = 0; paragraphIndex < paragraphTexts.size(); ++paragraphIndex) {
        const auto xpath = "(//w:p)[" + xPathIndex(paragraphIndex) + "]/w:r";
        const auto runTexts = mainDocumentPart().textsByXPath(xpath);
        for (std::size_t runIndex = 0; runIndex < runTexts.size(); ++runIndex) {
            runs.push_back({runTexts[runIndex], paragraphIndex, runIndex});
        }
    }
    return runs;
}

//! 读取文档的完整结构模型：段落（含样式、文本、内联图像）和表格
//! 通过 XPath 逐一解析每个段落/run 的文本、样式和嵌入图片信息
DocumentModel WordDocument::model() const {
    DocumentModel documentModel;
    const auto topLevelParagraphCount = paragraphCount();
    documentModel.paragraphs.reserve(topLevelParagraphCount);

    auto relsPart = const_cast<WordDocument*>(this)->part("word/_rels/document.xml.rels");

    for (std::size_t paragraphIndex = 0; paragraphIndex < topLevelParagraphCount; ++paragraphIndex) {
        Paragraph paragraph;
        try {
            paragraph.style = paragraphStyle(paragraphIndex);
        } catch (const WordException&) {
        }

        const auto paragraphPath = "(/w:document/w:body/w:p)[" + xPathIndex(paragraphIndex) + "]";
        const auto runCount = mainDocumentPart().countByXPath(paragraphPath + "/w:r");
        paragraph.runs.reserve(runCount);
        for (std::size_t runIndex = 0; runIndex < runCount; ++runIndex) {
            Run run;
            const auto runPath = paragraphPath + "/w:r[" + xPathIndex(runIndex) + "]";
            const auto textNodes = mainDocumentPart().textsByXPath(runPath + "//w:t");
            for (const auto& text : textNodes) {
                run.text += text;
            }
            try {
                run.style = runStyle(paragraphIndex, runIndex);
            } catch (const WordException&) {
            }

            const auto relationshipIds = mainDocumentPart().textsByXPath(runPath + "//a:blip/@r:embed");
            for (const auto& relationshipId : relationshipIds) {
                InlineImage image;
                image.image.relationshipId = relationshipId;
                if (relsPart) {
                    const auto target = (*relsPart)->textsByXPath(
                        "//rel:Relationship[@Id='" + relationshipId + "']/@Target"
                    );
                    if (!target.empty()) {
                        image.image.partName = mediaPartNameFromRelationshipTarget(target.front());
                    }
                }

                const auto descriptions = mainDocumentPart().textsByXPath(runPath + "//wp:docPr/@descr");
                if (!descriptions.empty()) {
                    image.description = descriptions.front();
                    image.options.description = descriptions.front();
                }

                const auto widths = mainDocumentPart().textsByXPath(runPath + "//wp:extent/@cx");
                const auto heights = mainDocumentPart().textsByXPath(runPath + "//wp:extent/@cy");
                if (!widths.empty()) {
                    try {
                        image.options.widthEmu = std::stoll(widths.front());
                    } catch (const std::exception&) {
                    }
                }
                if (!heights.empty()) {
                    try {
                        image.options.heightEmu = std::stoll(heights.front());
                    } catch (const std::exception&) {
                    }
                }

                run.images.push_back(std::move(image));
            }
            paragraph.runs.push_back(std::move(run));
        }
        documentModel.paragraphs.push_back(std::move(paragraph));
    }

    // 模型的有序块只包含顶层表格，不能让嵌套表格挤占后续顶层表格的索引。
    const auto tables = mainDocumentPart().countByXPath("/w:document/w:body/w:tbl");
    documentModel.tables.reserve(tables);
    for (std::size_t tableIndex = 0; tableIndex < tables; ++tableIndex) {
        const auto path = "(/w:document/w:body/w:tbl)[" + xPathIndex(tableIndex) + "]";
        const auto globalIndex = mainDocumentPart().countByXPath(path + "/preceding::w:tbl");
        documentModel.tables.push_back({table(globalIndex)});
    }

    // 读取页面尺寸（w:sectPr/w:pgSz），单位为 twips
    const auto orderedBlockCount = mainDocumentPart().countByXPath("/w:document/w:body/*[self::w:p or self::w:tbl]");
    documentModel.bodyBlocks.reserve(orderedBlockCount);
    std::size_t paragraphBlockIndex = 0;
    std::size_t tableBlockIndex = 0;
    for (std::size_t blockIndex = 0; blockIndex < orderedBlockCount; ++blockIndex) {
        const auto blockPath = "(/w:document/w:body/*[self::w:p or self::w:tbl])[" + xPathIndex(blockIndex) + "]";
        if (mainDocumentPart().hasXPath(blockPath + "[self::w:p]")) {
            if (paragraphBlockIndex < documentModel.paragraphs.size()) {
                BodyBlock block;
                block.type = BodyBlockType::Paragraph;
                block.paragraph = documentModel.paragraphs[paragraphBlockIndex];
                documentModel.bodyBlocks.push_back(std::move(block));
            }
            ++paragraphBlockIndex;
        } else if (mainDocumentPart().hasXPath(blockPath + "[self::w:tbl]")) {
            if (tableBlockIndex < documentModel.tables.size()) {
                BodyBlock block;
                block.type = BodyBlockType::Table;
                block.table = documentModel.tables[tableBlockIndex];
                documentModel.bodyBlocks.push_back(std::move(block));
            }
            ++tableBlockIndex;
        }
    }

    const auto pgWidths = mainDocumentPart().textsByXPath("//w:sectPr/w:pgSz/@w:w");
    const auto pgHeights = mainDocumentPart().textsByXPath("//w:sectPr/w:pgSz/@w:h");
    if (!pgWidths.empty()) {
        try { documentModel.pageSetup.pageWidthTwips = std::stoi(pgWidths.front()); }
        catch (const std::exception&) {}
    }
    if (!pgHeights.empty()) {
        try { documentModel.pageSetup.pageHeightTwips = std::stoi(pgHeights.front()); }
        catch (const std::exception&) {}
    }

    // 读取页边距（w:sectPr/w:pgMar）
    const auto margins = mainDocumentPart().textsByXPath("//w:sectPr/w:pgMar/@w:top");
    if (!margins.empty()) {
        try { documentModel.pageSetup.marginTopTwips = std::stoi(margins.front()); }
        catch (const std::exception&) {}
    }
    const auto marginBottoms = mainDocumentPart().textsByXPath("//w:sectPr/w:pgMar/@w:bottom");
    if (!marginBottoms.empty()) {
        try { documentModel.pageSetup.marginBottomTwips = std::stoi(marginBottoms.front()); }
        catch (const std::exception&) {}
    }
    const auto marginLefts = mainDocumentPart().textsByXPath("//w:sectPr/w:pgMar/@w:left");
    if (!marginLefts.empty()) {
        try { documentModel.pageSetup.marginLeftTwips = std::stoi(marginLefts.front()); }
        catch (const std::exception&) {}
    }
    const auto marginRights = mainDocumentPart().textsByXPath("//w:sectPr/w:pgMar/@w:right");
    if (!marginRights.empty()) {
        try { documentModel.pageSetup.marginRightTwips = std::stoi(marginRights.front()); }
        catch (const std::exception&) {}
    }

    return documentModel;
}

//! 用新模型替换文档正文内容（整体替换，段落/表格/样式全部重新生成）
void WordDocument::replaceBody(const DocumentModel& documentModel) {
    mainDocumentPart() = XmlPart::fromString(documentXmlFromModel(documentModel));
}

std::size_t WordDocument::paragraphCount() const {
    return mainDocumentPart().countByXPath("/w:document/w:body/w:p");
}

Paragraph WordDocument::paragraph(std::size_t index) const {
    if (index >= paragraphCount()) {
        throw WordProcessingException("Paragraph index is out of range: " + std::to_string(index));
    }
    return model().paragraphs[index];
}

// 段落局部编辑直接修改 XML 树，保留其他段落、表格及节属性的原始结构。
void WordDocument::setParagraph(std::size_t index, const Paragraph& paragraph) {
    if (index >= paragraphCount()) {
        throw WordProcessingException("Paragraph index is out of range");
    }
    mainDocumentPart().replaceElementXml(bodyParagraphXPath(index), standaloneParagraphXml(paragraph));
}

void WordDocument::appendParagraph(const Paragraph& paragraph) {
    insertParagraph(paragraphCount(), paragraph);
}

void WordDocument::insertParagraph(std::size_t index, const Paragraph& paragraph) {
    const auto count = paragraphCount();
    if (index > count) {
        throw WordProcessingException("Paragraph index is out of range");
    }
    const auto xml = standaloneParagraphXml(paragraph);
    if (index < count) {
        mainDocumentPart().insertElementXmlBefore(bodyParagraphXPath(index), xml);
    } else if (mainDocumentPart().hasXPath("/w:document/w:body/w:sectPr")) {
        mainDocumentPart().insertElementXmlBefore("/w:document/w:body/w:sectPr", xml);
    } else {
        mainDocumentPart().appendChildXml("/w:document/w:body", xml);
    }
}

void WordDocument::removeParagraph(std::size_t index) {
    if (index >= paragraphCount()) {
        throw WordProcessingException("Paragraph index is out of range");
    }
    mainDocumentPart().removeElement(bodyParagraphXPath(index));
}

void WordDocument::setRunStyle(std::size_t paragraphIndex, std::size_t runIndex, const TextStyle& style) {
    mainDocumentPart().setRunStyle(xmlParagraphIndex(mainDocumentPart(), paragraphIndex), runIndex, style);
}

TextStyle WordDocument::runStyle(std::size_t paragraphIndex, std::size_t runIndex) const {
    return mainDocumentPart().runStyle(xmlParagraphIndex(mainDocumentPart(), paragraphIndex), runIndex);
}

void WordDocument::setParagraphStyle(std::size_t paragraphIndex, const ParagraphStyle& style) {
    mainDocumentPart().setParagraphStyle(xmlParagraphIndex(mainDocumentPart(), paragraphIndex), style);
}

ParagraphStyle WordDocument::paragraphStyle(std::size_t paragraphIndex) const {
    return mainDocumentPart().paragraphStyle(xmlParagraphIndex(mainDocumentPart(), paragraphIndex));
}

std::vector<StyleInfo> WordDocument::paragraphStyles() const {
    auto stylesPart = const_cast<WordDocument*>(this)->part("word/styles.xml");
    if (!stylesPart) {
        return {};
    }
    return readStylesByType(**stylesPart, StyleType::Paragraph);
}

std::vector<StyleInfo> WordDocument::characterStyles() const {
    auto stylesPart = const_cast<WordDocument*>(this)->part("word/styles.xml");
    if (!stylesPart) {
        return {};
    }
    return readStylesByType(**stylesPart, StyleType::Character);
}

//! 用值替换文档中的占位符（自动补全 {{}} 格式），支持跨 w:t 节点的占位符
bool WordDocument::ReplaceText(const std::string& placeholder, const std::string& value) {
    return mainDocumentPart().replaceWordText(normalizePlaceholder(placeholder), value);
}

//! 批量执行占位符替换（映射表版本）
std::size_t WordDocument::ReplaceText(const std::map<std::string, std::string>& replacements) {
    std::size_t changedPatterns = 0;
    for (const auto& [placeholder, value] : replacements) {
        if (ReplaceText(placeholder, value)) {
            ++changedPatterns;
        }
    }
    return changedPatterns;
}

//! 通用的文本替换方法，支持区分大小写和全词匹配
//! 非全词模式使用跨 w:t 节点的 replaceWordText，全词模式在单个 w:t 节点内操作
bool WordDocument::replaceText(
    std::string_view search,
    std::string_view replacement,
    const ReplaceOptions& options
) {
    if (!options.wholeWord) {
        return mainDocumentPart().replaceWordText(search, replacement, options.matchCase);
    }

    bool changed = false;
    const auto textNodes = mainDocumentPart().textsByXPath("//w:t");
    for (std::size_t i = 0; i < textNodes.size(); ++i) {
        auto text = textNodes[i];
        const auto count = detail::replaceAllInString(text, search, replacement, options.matchCase, true);
        if (count == 0) {
            continue;
        }
        mainDocumentPart().setTextByXPath("(//w:t)[" + xPathIndex(i) + "]", text);
        changed = true;
    }
    return changed;
}

std::size_t WordDocument::replaceAll(
    const std::unordered_map<std::string, std::string>& replacements,
    const ReplaceOptions& options
) {
    std::size_t changedPatterns = 0;
    for (const auto& [search, replacement] : replacements) {
        if (replaceText(search, replacement, options)) {
            ++changedPatterns;
        }
    }
    return changedPatterns;
}

std::size_t WordDocument::tableCount() const {
    return mainDocumentPart().textsByXPath("//w:tbl").size();
}

TableData WordDocument::table(std::size_t tableIndex) const {
    ensureTableIndex(tableIndex, tableCount());
    TableData data;
    const auto rows = mainDocumentPart().textsByXPath(tableXPath(tableIndex) + "/w:tr");
    data.reserve(rows.size());

    for (std::size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        TableRow row;
        const auto cells = mainDocumentPart().textsByXPath(tableXPath(tableIndex) + "/w:tr[" + xPathIndex(rowIndex) + "]/w:tc");
        row.reserve(cells.size());
        for (const auto& cellText : cells) {
            row.push_back({cellText});
        }
        data.push_back(std::move(row));
    }

    return data;
}

void WordDocument::fillTable(std::size_t tableIndex, const TableData& data) {
    ensureTableIndex(tableIndex, tableCount());
    for (std::size_t rowIndex = 0; rowIndex < data.size(); ++rowIndex) {
        for (std::size_t cellIndex = 0; cellIndex < data[rowIndex].size(); ++cellIndex) {
            const auto textsPath = cellXPath(tableIndex, rowIndex, cellIndex) + "//w:t";
            if (!mainDocumentPart().hasXPath(textsPath)) {
                continue;
            }
            auto values = mainDocumentPart().textsByXPath(textsPath);
            if (values.empty()) {
                continue;
            }
            values.assign(values.size(), std::string{});
            values.front() = data[rowIndex][cellIndex].text;
            mainDocumentPart().setTextsByXPath(textsPath, values);
        }
    }
}

void WordDocument::fillFirstTable(const TableData& data) {
    fillTable(0, data);
}

//! 在书签处插入表格行：深拷贝包含书签的行模板，填充数据后依次插入
std::size_t WordDocument::insertTableRowsAtBookmark(std::string_view bookmark, const TableData& rows) {
    return mainDocumentPart().insertTableRowsAtBookmark(bookmark, rows);
}

//! 向文档添加图像：将图像文件复制到 OOXML 包中，同时添加文档关系
//! @return ImageId 包含 relationshipId 和部件路径
ImageId WordDocument::addImage(const Path& imagePath) {
    const auto mediaPartName = impl_->package.addImagePart(imagePath);
    const auto relationshipId = impl_->package.addMainDocumentRelationship(
        imageRelationshipType,
        mediaRelationshipTarget(mediaPartName)
    );
    return {relationshipId, mediaPartName};
}

void WordDocument::insertImage(
    std::size_t paragraphIndex,
    std::size_t runIndex,
    const ImageId& image,
    const ImageOptions& options
) {
    if (image.relationshipId.empty()) {
        throw WordProcessingException("Image relationship id must not be empty");
    }

    auto documentModel = model();
    if (paragraphIndex >= documentModel.paragraphs.size()) {
        throw WordProcessingException("Paragraph index is out of range: " + std::to_string(paragraphIndex));
    }

    auto& runs = documentModel.paragraphs[paragraphIndex].runs;
    if (runIndex > runs.size()) {
        throw WordProcessingException("Run index is out of range: " + std::to_string(runIndex));
    }
    if (runIndex == runs.size()) {
        runs.push_back({});
    }

    InlineImage inlineImage;
    inlineImage.image = image;
    inlineImage.options = options;
    inlineImage.description = options.description;
    runs[runIndex].images.push_back(std::move(inlineImage));
    if (auto* block = paragraphBlockAt(documentModel, paragraphIndex)) {
        block->paragraph = documentModel.paragraphs[paragraphIndex];
    }
    replaceBody(documentModel);
}

//! 在占位符位置插入图像：自动添加图像到包中，构建内联图像 XML 替换占位符文本
bool WordDocument::insertImageAtPlaceholder(
    std::string_view placeholder,
    const Path& imagePath,
    const ImageOptions& options
) {
    const auto token = normalizePlaceholder(placeholder);
    const auto image = addImage(imagePath);
    return mainDocumentPart().replaceWordTextWithXml(token, makeInlineImageRunXml(image.relationshipId, options));
}

XmlPart& WordDocument::mainDocumentPart() {
    return impl_->package.xmlPart(mainDocumentPath);
}

const XmlPart& WordDocument::mainDocumentPart() const {
    return impl_->package.xmlPart(mainDocumentPath);
}

std::optional<XmlPart*> WordDocument::part(std::string_view packagePath) {
    return impl_->package.findXmlPart(packagePath);
}

std::vector<std::string> WordDocument::partNames() const {
    return impl_->package.partNames();
}

std::optional<std::vector<std::uint8_t>> WordDocument::rawPart(std::string_view packagePath) const {
    return impl_->package.rawPart(packagePath);
}

} // namespace cppwordkit
