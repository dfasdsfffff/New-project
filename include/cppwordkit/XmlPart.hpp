#pragma once

#include "cppwordkit/Types.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cppwordkit {

class XmlPart {
public:
    XmlPart();
    explicit XmlPart(std::string xmlContent);
    ~XmlPart();

    XmlPart(XmlPart&&) noexcept;
    XmlPart& operator=(XmlPart&&) noexcept;

    XmlPart(const XmlPart&) = delete;
    XmlPart& operator=(const XmlPart&) = delete;

    static XmlPart fromString(std::string xmlContent);
    static XmlPart fromFile(const std::string& path);

    [[nodiscard]] std::string toString() const;
    void saveToFile(const std::string& path) const;

    [[nodiscard]] bool empty() const noexcept;

    [[nodiscard]] std::vector<std::string> textNodes() const;
    [[nodiscard]] std::optional<std::string> firstTextByXPath(std::string_view xpath) const;
    [[nodiscard]] std::vector<std::string> textsByXPath(std::string_view xpath) const;

    bool replaceText(std::string_view search, std::string_view replacement, bool matchCase = true);
    bool replaceWordText(std::string_view search, std::string_view replacement, bool matchCase = true);
    std::size_t insertTableRowsAtBookmark(std::string_view bookmark, const TableData& rows);
    bool replaceWordTextWithXml(std::string_view search, std::string_view replacementXml);

    void setTextByXPath(std::string_view xpath, std::string_view value);
    void appendChildXml(std::string_view parentXPath, std::string_view childXml);
    [[nodiscard]] std::size_t countByXPath(std::string_view xpath) const;
    void setTextsByXPath(std::string_view xpath, const std::vector<std::string>& values);

    [[nodiscard]] bool hasXPath(std::string_view xpath) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cppwordkit
