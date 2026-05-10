#pragma once

#include "cppwordkit/Template.hpp"
#include "cppwordkit/Types.hpp"
#include "cppwordkit/WordDocument.hpp"

#include <set>

namespace cppwordkit {

class DocxTemplate {
public:
    DocxTemplate();
    explicit DocxTemplate(WordDocument document);

    static DocxTemplate open(const Path& path);

    void render(const TemplateContext& context);
    void save();
    void saveAs(const Path& path, const SaveOptions& options = {});

    [[nodiscard]] std::set<std::string> getUndeclaredTemplateVariables(const TemplateContext& context = {}) const;
    void validateTemplate(const TemplateContext& context = {}) const;

    [[nodiscard]] WordDocument& document() noexcept;
    [[nodiscard]] const WordDocument& document() const noexcept;

private:
    WordDocument document_;
};

} // namespace cppwordkit
