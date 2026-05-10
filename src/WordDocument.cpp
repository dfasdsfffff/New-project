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

namespace cppwordkit {
namespace {

constexpr const char* mainDocumentPath = "word/document.xml";
constexpr const char* imageRelationshipType = "http://schemas.openxmlformats.org/officeDocument/2006/relationships/image";

std::string xPathIndex(std::size_t index) {
    return std::to_string(index + 1);
}

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

std::string mediaRelationshipTarget(const std::string& mediaPartName) {
    constexpr std::string_view prefix = "word/";
    if (mediaPartName.rfind(prefix, 0) == 0) {
        return mediaPartName.substr(prefix.size());
    }
    return mediaPartName;
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

bool WordDocument::insertImageAtPlaceholder(
    std::string_view placeholder,
    const Path& imagePath,
    const ImageOptions& options
) {
    const auto token = normalizePlaceholder(placeholder);
    const auto mediaPartName = impl_->package.addImagePart(imagePath);
    const auto relationshipId = impl_->package.addMainDocumentRelationship(
        imageRelationshipType,
        mediaRelationshipTarget(mediaPartName)
    );
    return mainDocumentPart().replaceWordTextWithXml(token, makeInlineImageRunXml(relationshipId, options));
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
