#pragma once

#include "cppwordkit/Error.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace cppwordkit {

class TemplateValue {
public:
    using Array = std::vector<TemplateValue>;
    using Object = std::map<std::string, TemplateValue>;

    TemplateValue();
    TemplateValue(const char* value);
    TemplateValue(std::string value);
    TemplateValue(bool value);
    TemplateValue(std::int64_t value);
    TemplateValue(double value);
    TemplateValue(Array value);
    TemplateValue(Object value);

    [[nodiscard]] bool isNull() const noexcept;
    [[nodiscard]] bool isArray() const noexcept;
    [[nodiscard]] bool isObject() const noexcept;
    [[nodiscard]] bool isTruthy() const;
    [[nodiscard]] std::string toString() const;
    [[nodiscard]] const TemplateValue* find(std::string_view path) const;
    [[nodiscard]] const Array& asArray() const;
    [[nodiscard]] const Object& asObject() const;
    [[nodiscard]] std::size_t size() const;

private:
    enum class Type {
        Null,
        String,
        Bool,
        Integer,
        Double,
        Array,
        Object
    };

    Type type_ = Type::Null;
    std::string string_;
    bool bool_ = false;
    std::int64_t integer_ = 0;
    double double_ = 0.0;
    std::shared_ptr<Array> array_;
    std::shared_ptr<Object> object_;
};

using TemplateContext = std::map<std::string, TemplateValue>;

struct TemplateOptions {
    bool strictUndefined = true;
    bool autoEscape = false;
    bool keepEmptyParagraphs = false;
};

struct TemplateDiagnostic {
    std::string message;
    std::size_t position{};
};

struct TemplateDiagnostics {
    std::vector<TemplateDiagnostic> errors;
    std::set<std::string> undeclaredVariables;

    [[nodiscard]] bool ok() const noexcept { return errors.empty() && undeclaredVariables.empty(); }
};

struct TemplateRenderResult {
    std::string text;
    std::set<std::string> usedVariables;
    TemplateDiagnostics diagnostics;
};

class TemplateEngine {
public:
    static TemplateRenderResult renderText(std::string_view text, const TemplateContext& context);
    static TemplateRenderResult renderText(
        std::string_view text,
        const TemplateContext& context,
        const TemplateOptions& options
    );
    static std::set<std::string> variables(std::string_view text);
    static std::set<std::string> undeclaredVariables(std::string_view text, const TemplateContext& context);
    static void validateText(std::string_view text);
    static TemplateDiagnostics diagnostics(std::string_view text, const TemplateContext& context = {});
};

} // namespace cppwordkit
