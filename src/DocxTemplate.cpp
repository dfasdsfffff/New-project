#include "cppwordkit/DocxTemplate.hpp"

#include "cppwordkit/Error.hpp"
#include "cppwordkit/XmlPart.hpp"

#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppwordkit {
namespace {

bool startsWith(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

bool endsWith(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}

bool isTemplateRenderablePart(std::string_view partName) {
    return partName == "word/document.xml" ||
        (startsWith(partName, "word/header") && endsWith(partName, ".xml")) ||
        (startsWith(partName, "word/footer") && endsWith(partName, ".xml"));
}

std::string wordText(const XmlPart& part) {
    const auto texts = part.textsByXPath("//w:t");
    std::ostringstream output;
    for (const auto& text : texts) {
        output << text;
    }
    return output.str();
}

std::vector<std::string> templateExpressions(std::string_view text) {
    std::vector<std::string> result;
    std::size_t cursor = 0;
    while (cursor < text.size()) {
        const auto open = text.find("{{", cursor);
        if (open == std::string_view::npos) {
            break;
        }
        const auto close = text.find("}}", open + 2);
        if (close == std::string_view::npos) {
            throw WordProcessingException("Unclosed template variable");
        }
        result.push_back(std::string(text.substr(open, close - open + 2)));
        cursor = close + 2;
    }
    return result;
}

void renderDocument(WordDocument& document, const TemplateContext& context) {
    for (const auto& partName : document.partNames()) {
        if (!isTemplateRenderablePart(partName)) {
            continue;
        }

        auto partRef = document.part(partName);
        if (!partRef) {
            continue;
        }

        const auto text = wordText(**partRef);
        for (const auto& expression : templateExpressions(text)) {
            const auto expressionRendered = TemplateEngine::renderText(expression, context);
            (*partRef)->replaceWordText(expression, expressionRendered.text);
        }
    }
}

std::set<std::string> undeclaredVariables(WordDocument& document, const TemplateContext& context) {
    std::set<std::string> result;
    for (const auto& partName : document.partNames()) {
        if (!isTemplateRenderablePart(partName)) {
            continue;
        }

        auto partRef = document.part(partName);
        if (!partRef) {
            continue;
        }

        const auto undeclared = TemplateEngine::undeclaredVariables(wordText(**partRef), context);
        result.insert(undeclared.begin(), undeclared.end());
    }
    return result;
}

void validateDocument(WordDocument& document, const TemplateContext& context) {
    const auto undeclared = undeclaredVariables(document, context);
    if (!undeclared.empty()) {
        throw WordProcessingException("Template variable is not declared: " + *undeclared.begin());
    }

    for (const auto& partName : document.partNames()) {
        if (!isTemplateRenderablePart(partName)) {
            continue;
        }
        auto partRef = document.part(partName);
        if (partRef) {
            TemplateEngine::validateText(wordText(**partRef));
        }
    }
}

} // namespace

DocxTemplate::DocxTemplate() = default;

DocxTemplate::DocxTemplate(WordDocument document)
    : document_(std::move(document)) {}

DocxTemplate DocxTemplate::open(const Path& path) {
    return DocxTemplate(WordDocument::open(path));
}

void DocxTemplate::render(const TemplateContext& context) {
    validateTemplate(context);
    renderDocument(document_, context);
}

void DocxTemplate::save() {
    document_.save();
}

void DocxTemplate::saveAs(const Path& path, const SaveOptions& options) {
    document_.saveAs(path, options);
}

std::set<std::string> DocxTemplate::getUndeclaredTemplateVariables(const TemplateContext& context) const {
    return undeclaredVariables(const_cast<WordDocument&>(document_), context);
}

void DocxTemplate::validateTemplate(const TemplateContext& context) const {
    validateDocument(const_cast<WordDocument&>(document_), context);
}

WordDocument& DocxTemplate::document() noexcept {
    return document_;
}

const WordDocument& DocxTemplate::document() const noexcept {
    return document_;
}

void WordDocument::render(const TemplateContext& context) {
    validateDocument(*this, context);
    renderDocument(*this, context);
}

std::set<std::string> WordDocument::getUndeclaredTemplateVariables(const TemplateContext& context) const {
    return undeclaredVariables(const_cast<WordDocument&>(*this), context);
}

void WordDocument::validateTemplate(const TemplateContext& context) const {
    validateDocument(const_cast<WordDocument&>(*this), context);
}

} // namespace cppwordkit
