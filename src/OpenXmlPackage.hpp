#pragma once

#include "ZipArchive.hpp"
#include "cppwordkit/XmlPart.hpp"

#include <optional>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace cppwordkit {

class OpenXmlPackage {
public:
    static OpenXmlPackage open(const Path& path);
    static OpenXmlPackage createBlankDocument();

    void saveAs(const Path& path) const;

    [[nodiscard]] bool hasPart(std::string_view name) const;
    [[nodiscard]] XmlPart& xmlPart(std::string_view name);
    [[nodiscard]] const XmlPart& xmlPart(std::string_view name) const;
    [[nodiscard]] std::optional<XmlPart*> findXmlPart(std::string_view name);
    [[nodiscard]] std::vector<std::string> partNames() const;
    [[nodiscard]] std::string addImagePart(const Path& imagePath);
    [[nodiscard]] std::string addMainDocumentRelationship(std::string_view type, std::string_view target);
    void ensureContentTypeDefault(std::string_view extension, std::string_view contentType);

private:
    ZipArchive archive_;
    std::unordered_map<std::string, XmlPart> xmlParts_;

    void loadXmlPart(const std::string& name);
    void flushXmlParts();
};

} // namespace cppwordkit
