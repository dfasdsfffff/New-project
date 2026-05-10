#include "cppwordkit/Template.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <sstream>
#include <string>
#include <utility>

namespace cppwordkit {
namespace {

std::string trim(std::string_view value) {
    auto begin = value.begin();
    auto end = value.end();
    while (begin != end && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    while (begin != end && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return {begin, end};
}

std::vector<std::string> split(std::string_view value, char delimiter) {
    std::vector<std::string> result;
    std::size_t start = 0;
    while (start <= value.size()) {
        const auto end = value.find(delimiter, start);
        if (end == std::string_view::npos) {
            result.push_back(trim(value.substr(start)));
            break;
        }
        result.push_back(trim(value.substr(start, end - start)));
        start = end + 1;
    }
    return result;
}

std::string lowerAscii(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string upperAscii(std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
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

bool startsWith(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

std::string unquote(std::string_view value) {
    const auto trimmed = trim(value);
    if (trimmed.size() >= 2 &&
        ((trimmed.front() == '\'' && trimmed.back() == '\'') ||
            (trimmed.front() == '"' && trimmed.back() == '"'))) {
        return trimmed.substr(1, trimmed.size() - 2);
    }
    return trimmed;
}

std::string extractDefaultArgument(std::string_view filter) {
    const auto open = filter.find('(');
    const auto close = filter.rfind(')');
    if (open == std::string_view::npos || close == std::string_view::npos || close <= open) {
        throw WordProcessingException("default filter requires an argument");
    }
    return unquote(filter.substr(open + 1, close - open - 1));
}

std::string rootVariable(std::string_view expression) {
    auto root = trim(split(expression, '|').front());
    const auto dot = root.find('.');
    if (dot != std::string::npos) {
        root = root.substr(0, dot);
    }
    return root;
}

bool hasDefaultFilter(std::string_view expression) {
    const auto filters = split(expression, '|');
    for (std::size_t i = 1; i < filters.size(); ++i) {
        if (startsWith(trim(filters[i]), "default")) {
            return true;
        }
    }
    return false;
}

const TemplateValue* findInContext(const TemplateContext& context, std::string_view path) {
    const auto parts = split(path, '.');
    if (parts.empty() || parts.front().empty()) {
        return nullptr;
    }

    const auto first = context.find(parts.front());
    if (first == context.end()) {
        return nullptr;
    }

    const TemplateValue* current = &first->second;
    for (std::size_t i = 1; i < parts.size(); ++i) {
        current = current->find(parts[i]);
        if (current == nullptr) {
            return nullptr;
        }
    }
    return current;
}

std::string evaluateExpression(
    std::string_view expression,
    const TemplateContext& context,
    std::set<std::string>& usedVariables
) {
    const auto filters = split(expression, '|');
    if (filters.empty() || filters.front().empty()) {
        throw WordProcessingException("Template expression is empty");
    }

    const auto path = trim(filters.front());
    usedVariables.insert(rootVariable(path));
    const auto* value = findInContext(context, path);
    bool missing = value == nullptr || value->isNull();
    std::string rendered = missing ? std::string{} : value->toString();

    for (std::size_t i = 1; i < filters.size(); ++i) {
        const auto filter = trim(filters[i]);
        if (filter == "upper") {
            rendered = upperAscii(rendered);
        } else if (filter == "lower") {
            rendered = lowerAscii(rendered);
        } else if (filter == "escape" || filter == "e") {
            rendered = xmlEscape(rendered);
        } else if (startsWith(filter, "default")) {
            if (missing || rendered.empty()) {
                rendered = extractDefaultArgument(filter);
                missing = false;
            }
        } else {
            throw WordProcessingException("Unsupported template filter: " + filter);
        }
    }

    if (missing) {
        throw WordProcessingException("Template variable is not declared: " + path);
    }
    return rendered;
}

} // namespace

TemplateValue::TemplateValue() = default;

TemplateValue::TemplateValue(const char* value) : TemplateValue(std::string(value)) {}

TemplateValue::TemplateValue(std::string value) : type_(Type::String), string_(std::move(value)) {}

TemplateValue::TemplateValue(bool value) : type_(Type::Bool), bool_(value) {}

TemplateValue::TemplateValue(std::int64_t value) : type_(Type::Integer), integer_(value) {}

TemplateValue::TemplateValue(double value) : type_(Type::Double), double_(value) {}

TemplateValue::TemplateValue(Array value)
    : type_(Type::Array), array_(std::make_shared<Array>(std::move(value))) {}

TemplateValue::TemplateValue(Object value)
    : type_(Type::Object), object_(std::make_shared<Object>(std::move(value))) {}

bool TemplateValue::isNull() const noexcept {
    return type_ == Type::Null;
}

bool TemplateValue::isTruthy() const {
    switch (type_) {
    case Type::Null:
        return false;
    case Type::String:
        return !string_.empty();
    case Type::Bool:
        return bool_;
    case Type::Integer:
        return integer_ != 0;
    case Type::Double:
        return std::abs(double_) > 0.0000001;
    case Type::Array:
        return array_ != nullptr && !array_->empty();
    case Type::Object:
        return object_ != nullptr && !object_->empty();
    }
    return false;
}

std::string TemplateValue::toString() const {
    switch (type_) {
    case Type::Null:
        return {};
    case Type::String:
        return string_;
    case Type::Bool:
        return bool_ ? "true" : "false";
    case Type::Integer:
        return std::to_string(integer_);
    case Type::Double: {
        std::ostringstream output;
        output << double_;
        return output.str();
    }
    case Type::Array:
        throw WordProcessingException("Cannot render array as scalar text");
    case Type::Object:
        throw WordProcessingException("Cannot render object as scalar text");
    }
    return {};
}

const TemplateValue* TemplateValue::find(std::string_view path) const {
    if (type_ != Type::Object || object_ == nullptr) {
        return nullptr;
    }
    const auto it = object_->find(std::string(path));
    if (it == object_->end()) {
        return nullptr;
    }
    return &it->second;
}

TemplateRenderResult TemplateEngine::renderText(std::string_view text, const TemplateContext& context) {
    validateText(text);

    TemplateRenderResult result;
    std::size_t cursor = 0;
    while (cursor < text.size()) {
        const auto open = text.find("{{", cursor);
        if (open == std::string_view::npos) {
            result.text.append(text.substr(cursor));
            break;
        }

        const auto close = text.find("}}", open + 2);
        if (close == std::string_view::npos) {
            throw WordProcessingException("Unclosed template variable");
        }

        result.text.append(text.substr(cursor, open - cursor));
        result.text += evaluateExpression(text.substr(open + 2, close - open - 2), context, result.usedVariables);
        cursor = close + 2;
    }
    return result;
}

std::set<std::string> TemplateEngine::variables(std::string_view text) {
    std::set<std::string> result;
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
        result.insert(rootVariable(text.substr(open + 2, close - open - 2)));
        cursor = close + 2;
    }
    return result;
}

std::set<std::string> TemplateEngine::undeclaredVariables(std::string_view text, const TemplateContext& context) {
    std::set<std::string> result;
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
        const auto expression = text.substr(open + 2, close - open - 2);
        const auto root = rootVariable(expression);
        if (!context.contains(root) && !hasDefaultFilter(expression)) {
            result.insert(root);
        }
        cursor = close + 2;
    }
    return result;
}

void TemplateEngine::validateText(std::string_view text) {
    if (text.find("{%") != std::string_view::npos) {
        throw WordProcessingException("Control-flow tags are not implemented in this renderer yet");
    }
    std::size_t cursor = 0;
    while (cursor < text.size()) {
        const auto open = text.find("{{", cursor);
        if (open == std::string_view::npos) {
            break;
        }
        const auto expressionStart = open + 2;
        auto expression = text.substr(expressionStart);
        while (!expression.empty() && std::isspace(static_cast<unsigned char>(expression.front()))) {
            expression.remove_prefix(1);
        }
        if (startsWith(expression, "r ") || startsWith(expression, "r\t")) {
            throw WordProcessingException("RichText tags are not implemented in this renderer yet");
        }
        cursor = expressionStart;
    }
}

} // namespace cppwordkit
