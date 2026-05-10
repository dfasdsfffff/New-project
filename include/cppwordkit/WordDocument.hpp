#pragma once

#include "cppwordkit/Types.hpp"
#include "cppwordkit/Template.hpp"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace cppwordkit {

class XmlPart;

class WordDocument {
public:
    WordDocument();
    ~WordDocument();

    WordDocument(WordDocument&&) noexcept;
    WordDocument& operator=(WordDocument&&) noexcept;

    WordDocument(const WordDocument&) = delete;
    WordDocument& operator=(const WordDocument&) = delete;

    static WordDocument open(const Path& path);
    static WordDocument create();

    void save();
    void saveAs(const Path& path, const SaveOptions& options = {});

    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] Path path() const;

    [[nodiscard]] std::string text() const;
    [[nodiscard]] std::vector<TextRun> textRuns() const;
    [[nodiscard]] DocumentModel model() const;
    void replaceBody(const DocumentModel& model);
    [[nodiscard]] std::size_t paragraphCount() const;
    [[nodiscard]] Paragraph paragraph(std::size_t index) const;
    void setParagraph(std::size_t index, const Paragraph& paragraph);
    void appendParagraph(const Paragraph& paragraph);
    void insertParagraph(std::size_t index, const Paragraph& paragraph);
    void removeParagraph(std::size_t index);
    void setRunStyle(std::size_t paragraphIndex, std::size_t runIndex, const TextStyle& style);
    [[nodiscard]] TextStyle runStyle(std::size_t paragraphIndex, std::size_t runIndex) const;
    void setParagraphStyle(std::size_t paragraphIndex, const ParagraphStyle& style);
    [[nodiscard]] ParagraphStyle paragraphStyle(std::size_t paragraphIndex) const;

    bool ReplaceText(const std::string& placeholder, const std::string& value);
    std::size_t ReplaceText(const std::map<std::string, std::string>& replacements);

    bool replaceText(
        std::string_view search,
        std::string_view replacement,
        const ReplaceOptions& options = {}
    );

    std::size_t replaceAll(
        const std::unordered_map<std::string, std::string>& replacements,
        const ReplaceOptions& options = {}
    );

    [[nodiscard]] std::size_t tableCount() const;
    [[nodiscard]] TableData table(std::size_t tableIndex) const;

    void fillTable(std::size_t tableIndex, const TableData& data);
    void fillFirstTable(const TableData& data);
    std::size_t insertTableRowsAtBookmark(std::string_view bookmark, const TableData& rows);
    [[nodiscard]] ImageId addImage(const Path& imagePath);
    void insertImage(
        std::size_t paragraphIndex,
        std::size_t runIndex,
        const ImageId& image,
        const ImageOptions& options = {}
    );
    bool insertImageAtPlaceholder(
        std::string_view placeholder,
        const Path& imagePath,
        const ImageOptions& options = {}
    );
    void render(const TemplateContext& context);
    [[nodiscard]] std::set<std::string> getUndeclaredTemplateVariables(const TemplateContext& context = {}) const;
    void validateTemplate(const TemplateContext& context = {}) const;

    [[nodiscard]] XmlPart& mainDocumentPart();
    [[nodiscard]] const XmlPart& mainDocumentPart() const;

    [[nodiscard]] std::optional<XmlPart*> part(std::string_view packagePath);
    [[nodiscard]] std::vector<std::string> partNames() const;
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> rawPart(std::string_view packagePath) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cppwordkit
