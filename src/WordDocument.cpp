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

constexpr const char* mainDocumentPath = "word/document.xml";
constexpr const char* imageRelationshipType = "http://schemas.openxmlformats.org/officeDocument/2006/relationships/image";

std::string xPathIndex(std::size_t index) {
    return std::to_string(index + 1);
}

std::string makeInlineImageRunXml(std::string_view relationshipId, const ImageOptions& options);

std::string tableXPath(std::size_t tableIndex) {
    return "(//w:tbl)[" + xPathIndex(tableIndex) + "]";
}

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

void appendRunProperties(std::ostringstream& xml, const TextStyle& style) {
    if (!style.fontFamily &&
        !style.colorHex &&
        !style.backgroundColorHex &&
        !style.fontSizeHalfPoints &&
        !style.bold &&
        !style.italic &&
        !style.underline &&
        !style.strike &&
        !style.superscript &&
        !style.subscript) {
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
    if (style.fontSizeHalfPoints) {
        xml << "<w:sz w:val=\"" << *style.fontSizeHalfPoints << "\"/>";
    }
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

std::string documentXmlFromModel(const DocumentModel& model) {
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        << "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
        << "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
        << "xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\" "
        << "xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
        << "xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">"
        << "<w:body>";
    for (const auto& paragraph : model.paragraphs) {
        xml << paragraphXml(paragraph);
    }
    for (const auto& table : model.tables) {
        xml << tableXml(table);
    }
    xml << "<w:sectPr/></w:body></w:document>";
    return xml.str();
}

std::string mediaRelationshipTarget(const std::string& mediaPartName) {
    constexpr std::string_view prefix = "word/";
    if (mediaPartName.rfind(prefix, 0) == 0) {
        return mediaPartName.substr(prefix.size());
    }
    return mediaPartName;
}

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

struct WordDocument::Impl {
    Path sourcePath;
    OpenXmlPackage package;
    bool open = false;
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
    impl_->package.saveAs(path);
    impl_->sourcePath = path;
    impl_->open = true;
}

bool WordDocument::isOpen() const noexcept {
    return impl_ != nullptr && impl_->open;
}

Path WordDocument::path() const {
    return impl_->sourcePath;
}

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

    const auto tables = tableCount();
    documentModel.tables.reserve(tables);
    for (std::size_t tableIndex = 0; tableIndex < tables; ++tableIndex) {
        documentModel.tables.push_back({table(tableIndex)});
    }
    return documentModel;
}

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

void WordDocument::setParagraph(std::size_t index, const Paragraph& paragraph) {
    auto documentModel = model();
    if (index >= documentModel.paragraphs.size()) {
        throw WordProcessingException("Paragraph index is out of range: " + std::to_string(index));
    }
    documentModel.paragraphs[index] = paragraph;
    replaceBody(documentModel);
}

void WordDocument::appendParagraph(const Paragraph& paragraph) {
    auto documentModel = model();
    documentModel.paragraphs.push_back(paragraph);
    replaceBody(documentModel);
}

void WordDocument::insertParagraph(std::size_t index, const Paragraph& paragraph) {
    auto documentModel = model();
    if (index > documentModel.paragraphs.size()) {
        throw WordProcessingException("Paragraph index is out of range: " + std::to_string(index));
    }
    documentModel.paragraphs.insert(documentModel.paragraphs.begin() + static_cast<std::ptrdiff_t>(index), paragraph);
    replaceBody(documentModel);
}

void WordDocument::removeParagraph(std::size_t index) {
    auto documentModel = model();
    if (index >= documentModel.paragraphs.size()) {
        throw WordProcessingException("Paragraph index is out of range: " + std::to_string(index));
    }
    documentModel.paragraphs.erase(documentModel.paragraphs.begin() + static_cast<std::ptrdiff_t>(index));
    replaceBody(documentModel);
}

void WordDocument::setRunStyle(std::size_t paragraphIndex, std::size_t runIndex, const TextStyle& style) {
    mainDocumentPart().setRunStyle(paragraphIndex, runIndex, style);
}

TextStyle WordDocument::runStyle(std::size_t paragraphIndex, std::size_t runIndex) const {
    return mainDocumentPart().runStyle(paragraphIndex, runIndex);
}

void WordDocument::setParagraphStyle(std::size_t paragraphIndex, const ParagraphStyle& style) {
    mainDocumentPart().setParagraphStyle(paragraphIndex, style);
}

ParagraphStyle WordDocument::paragraphStyle(std::size_t paragraphIndex) const {
    return mainDocumentPart().paragraphStyle(paragraphIndex);
}

bool WordDocument::ReplaceText(const std::string& placeholder, const std::string& value) {
    return mainDocumentPart().replaceWordText(normalizePlaceholder(placeholder), value);
}

std::size_t WordDocument::ReplaceText(const std::map<std::string, std::string>& replacements) {
    std::size_t changedPatterns = 0;
    for (const auto& [placeholder, value] : replacements) {
        if (ReplaceText(placeholder, value)) {
            ++changedPatterns;
        }
    }
    return changedPatterns;
}

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

std::size_t WordDocument::insertTableRowsAtBookmark(std::string_view bookmark, const TableData& rows) {
    return mainDocumentPart().insertTableRowsAtBookmark(bookmark, rows);
}

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
    replaceBody(documentModel);
}

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

} // namespace cppwordkit
