#pragma once

#include "cppwordkit/Types.hpp"

#include <map>
#include <memory>
#include <optional>
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
    bool insertImageAtPlaceholder(
        std::string_view placeholder,
        const Path& imagePath,
        const ImageOptions& options = {}
    );

    [[nodiscard]] XmlPart& mainDocumentPart();
    [[nodiscard]] const XmlPart& mainDocumentPart() const;

    [[nodiscard]] std::optional<XmlPart*> part(std::string_view packagePath);
    [[nodiscard]] std::vector<std::string> partNames() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cppwordkit
