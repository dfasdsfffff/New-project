#include "cppwordkit/Template.hpp"

#include <algorithm>
#include <cctype>
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

bool startsWith(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
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

std::vector<std::string> splitFilters(std::string_view value) {
    std::vector<std::string> result;
    std::string current;
    char quote = '\0';
    int parens = 0;
    for (const char ch : value) {
        if ((ch == '\'' || ch == '"') && quote == '\0') {
            quote = ch;
        } else if (ch == quote) {
            quote = '\0';
        } else if (quote == '\0' && ch == '(') {
            ++parens;
        } else if (quote == '\0' && ch == ')') {
            --parens;
        }

        if (quote == '\0' && parens == 0 && ch == '|') {
            result.push_back(trim(current));
            current.clear();
        } else {
            current += ch;
        }
    }
    result.push_back(trim(current));
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

std::string capitalizeAscii(std::string value) {
    if (value.empty()) {
        return value;
    }
    value = lowerAscii(std::move(value));
    value.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(value.front())));
    return value;
}

std::string titleAscii(std::string value) {
    bool nextUpper = true;
    for (auto& ch : value) {
        if (std::isspace(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-') {
            nextUpper = true;
            continue;
        }
        ch = static_cast<char>(nextUpper
            ? std::toupper(static_cast<unsigned char>(ch))
            : std::tolower(static_cast<unsigned char>(ch)));
        nextUpper = false;
    }
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

std::string unquote(std::string_view value) {
    const auto trimmed = trim(value);
    if (trimmed.size() >= 2 &&
        ((trimmed.front() == '\'' && trimmed.back() == '\'') ||
            (trimmed.front() == '"' && trimmed.back() == '"'))) {
        return trimmed.substr(1, trimmed.size() - 2);
    }
    return trimmed;
}

std::vector<std::string> parseArguments(std::string_view value) {
    std::vector<std::string> args;
    std::string current;
    char quote = '\0';
    int parens = 0;
    for (const char ch : value) {
        if ((ch == '\'' || ch == '"') && quote == '\0') {
            quote = ch;
        } else if (ch == quote) {
            quote = '\0';
        } else if (quote == '\0' && ch == '(') {
            ++parens;
        } else if (quote == '\0' && ch == ')') {
            --parens;
        }

        if (quote == '\0' && parens == 0 && ch == ',') {
            args.push_back(unquote(current));
            current.clear();
        } else {
            current += ch;
        }
    }
    if (!current.empty() || !args.empty()) {
        args.push_back(unquote(current));
    }
    return args;
}

struct EvalValue {
    const TemplateValue* ref{};
    std::string scalar;
    bool missing = true;

    [[nodiscard]] bool truthy() const {
        if (ref != nullptr) {
            return ref->isTruthy();
        }
        return !scalar.empty();
    }

    [[nodiscard]] std::string toString(bool strict) const {
        if (ref != nullptr) {
            return ref->toString();
        }
        if (missing && strict) {
            throw WordProcessingException("Template variable is not declared");
        }
        return scalar;
    }

    [[nodiscard]] std::size_t size() const {
        if (ref != nullptr) {
            return ref->size();
        }
        return scalar.size();
    }
};

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

EvalValue evaluatePath(std::string_view expression, const TemplateContext& context) {
    const auto path = trim(expression);
    if (path.empty()) {
        throw WordProcessingException("Template expression is empty");
    }
    if ((path.front() == '\'' && path.back() == '\'') || (path.front() == '"' && path.back() == '"')) {
        return {nullptr, unquote(path), false};
    }
    if (path == "true" || path == "True") {
        return {nullptr, "true", false};
    }
    if (path == "false" || path == "False") {
        return {nullptr, "", false};
    }
    const auto* value = findInContext(context, path);
    if (value == nullptr || value->isNull()) {
        return {nullptr, {}, true};
    }
    return {value, {}, false};
}

std::string rootVariable(std::string_view expression) {
    auto root = trim(split(expression, '|').front());
    if (startsWith(root, "not ")) {
        root = trim(std::string_view(root).substr(4));
    }
    const auto space = root.find(' ');
    if (space != std::string::npos) {
        root = root.substr(0, space);
    }
    const auto dot = root.find('.');
    if (dot != std::string::npos) {
        root = root.substr(0, dot);
    }
    return root;
}

std::string filterName(std::string_view filter);

bool hasDefaultFilter(std::string_view expression) {
    const auto filters = splitFilters(expression);
    for (std::size_t i = 1; i < filters.size(); ++i) {
        if (filterName(filters[i]) == "default") {
            return true;
        }
    }
    return false;
}

std::string filterName(std::string_view filter) {
    const auto trimmed = trim(filter);
    const auto open = trimmed.find('(');
    return open == std::string::npos ? trimmed : trim(std::string_view(trimmed).substr(0, open));
}

std::vector<std::string> filterArgs(std::string_view filter) {
    const auto trimmed = trim(filter);
    const auto open = trimmed.find('(');
    const auto close = trimmed.rfind(')');
    if (open == std::string::npos) {
        return {};
    }
    if (close == std::string::npos || close <= open) {
        throw WordProcessingException("Template filter argument list is not closed: " + trimmed);
    }
    return parseArguments(std::string_view(trimmed).substr(open + 1, close - open - 1));
}

std::string applyFilter(
    std::string rendered,
    const EvalValue& original,
    std::string_view filter,
    bool& missing
) {
    const auto name = filterName(filter);
    const auto args = filterArgs(filter);
    if (name == "upper") {
        return upperAscii(std::move(rendered));
    }
    if (name == "lower") {
        return lowerAscii(std::move(rendered));
    }
    if (name == "capitalize") {
        return capitalizeAscii(std::move(rendered));
    }
    if (name == "title") {
        return titleAscii(std::move(rendered));
    }
    if (name == "trim") {
        return trim(rendered);
    }
    if (name == "escape" || name == "e") {
        return xmlEscape(rendered);
    }
    if (name == "length") {
        return std::to_string(original.size());
    }
    if (name == "default") {
        if (args.empty()) {
            throw WordProcessingException("default filter requires an argument");
        }
        if (missing || rendered.empty()) {
            missing = false;
            return args.front();
        }
        return rendered;
    }
    if (name == "replace") {
        if (args.size() < 2) {
            throw WordProcessingException("replace filter requires two arguments");
        }
        std::size_t pos = 0;
        while ((pos = rendered.find(args[0], pos)) != std::string::npos) {
            rendered.replace(pos, args[0].size(), args[1]);
            pos += args[1].size();
        }
        return rendered;
    }
    if (name == "join") {
        if (original.ref == nullptr || !original.ref->isArray()) {
            return rendered;
        }
        const auto delimiter = args.empty() ? std::string{} : args.front();
        std::ostringstream output;
        const auto& values = original.ref->asArray();
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i > 0) {
                output << delimiter;
            }
            output << values[i].toString();
        }
        return output.str();
    }
    throw WordProcessingException("Unsupported template filter: " + name);
}

std::string evaluateExpression(
    std::string_view expression,
    const TemplateContext& context,
    const TemplateOptions& options,
    std::set<std::string>& usedVariables
) {
    const auto filters = splitFilters(expression);
    if (filters.empty() || filters.front().empty()) {
        throw WordProcessingException("Template expression is empty");
    }

    const auto path = trim(filters.front());
    usedVariables.insert(rootVariable(path));
    auto value = evaluatePath(path, context);
    bool missing = value.missing;
    std::string rendered;
    if (!missing) {
        if (value.ref == nullptr || (!value.ref->isArray() && !value.ref->isObject())) {
            rendered = value.toString(options.strictUndefined);
        } else if (filters.size() == 1) {
            rendered = value.toString(options.strictUndefined);
        }
    }

    for (std::size_t i = 1; i < filters.size(); ++i) {
        rendered = applyFilter(std::move(rendered), value, filters[i], missing);
    }

    if (missing) {
        if (options.strictUndefined) {
            throw WordProcessingException("Template variable is not declared: " + std::string(path));
        }
        return {};
    }
    return options.autoEscape ? xmlEscape(rendered) : rendered;
}

bool evaluateCondition(std::string_view expression, const TemplateContext& context) {
    auto expr = trim(expression);
    if (startsWith(expr, "not ")) {
        return !evaluateCondition(std::string_view(expr).substr(4), context);
    }
    const auto neq = expr.find("!=");
    const auto eq = expr.find("==");
    if (neq != std::string::npos || eq != std::string::npos) {
        const auto pos = neq != std::string::npos ? neq : eq;
        const auto opSize = neq != std::string::npos ? 2 : 2;
        const auto left = evaluatePath(std::string_view(expr).substr(0, pos), context);
        const auto right = evaluatePath(std::string_view(expr).substr(pos + opSize), context);
        const auto same = left.toString(false) == right.toString(false);
        return neq != std::string::npos ? !same : same;
    }
    return evaluatePath(expr, context).truthy();
}

enum class TokenKind {
    Text,
    Variable,
    Statement,
    Comment
};

struct Token {
    TokenKind kind{};
    std::string value;
    std::size_t position{};
};

std::string normalizeStatement(std::string value) {
    value = trim(value);
    if (value.size() >= 2 &&
        (value[0] == 'p' || value[0] == 'r') &&
        std::isspace(static_cast<unsigned char>(value[1]))) {
        value.erase(value.begin());
        value = trim(value);
    } else if (value.size() >= 3 && value[0] == 't' && value[1] == 'r' &&
        std::isspace(static_cast<unsigned char>(value[2]))) {
        value.erase(value.begin(), value.begin() + 2);
        value = trim(value);
    } else if (value.size() >= 3 && value[0] == 't' && value[1] == 'c' &&
        std::isspace(static_cast<unsigned char>(value[2]))) {
        value.erase(value.begin(), value.begin() + 2);
        value = trim(value);
    }
    return value;
}

std::vector<Token> lex(std::string_view text) {
    std::vector<Token> tokens;
    std::size_t cursor = 0;
    while (cursor < text.size()) {
        const auto variable = text.find("{{", cursor);
        const auto statement = text.find("{%", cursor);
        const auto comment = text.find("{#", cursor);
        auto next = std::min({variable, statement, comment});
        if (next == std::string_view::npos) {
            tokens.push_back({TokenKind::Text, std::string(text.substr(cursor)), cursor});
            break;
        }
        if (next > cursor) {
            tokens.push_back({TokenKind::Text, std::string(text.substr(cursor, next - cursor)), cursor});
        }

        const auto kind = next == variable ? TokenKind::Variable : (next == statement ? TokenKind::Statement : TokenKind::Comment);
        const auto closeText = kind == TokenKind::Variable ? "}}" : (kind == TokenKind::Statement ? "%}" : "#}");
        const auto close = text.find(closeText, next + 2);
        if (close == std::string_view::npos) {
            throw WordProcessingException("Unclosed template tag");
        }
        auto value = std::string(text.substr(next + 2, close - next - 2));
        if (kind == TokenKind::Statement) {
            value = normalizeStatement(std::move(value));
        } else {
            value = trim(value);
        }
        tokens.push_back({kind, std::move(value), next});
        cursor = close + 2;
    }
    return tokens;
}

struct Node {
    enum class Kind {
        Text,
        Variable,
        If,
        For
    };

    Kind kind{};
    std::string value;
    std::string loopVariable;
    std::vector<Node> children;
    std::vector<std::pair<std::string, std::vector<Node>>> elifBranches;
    std::vector<Node> elseChildren;
};

std::vector<Node> parseNodes(const std::vector<Token>& tokens, std::size_t& index, const std::set<std::string>& terminators);

Node parseIf(const std::vector<Token>& tokens, std::size_t& index) {
    Node node;
    node.kind = Node::Kind::If;
    node.value = trim(std::string_view(tokens[index].value).substr(3));
    ++index;
    node.children = parseNodes(tokens, index, {"elif", "else", "endif"});
    while (index < tokens.size() && tokens[index].kind == TokenKind::Statement && startsWith(tokens[index].value, "elif ")) {
        const auto condition = trim(std::string_view(tokens[index].value).substr(5));
        ++index;
        node.elifBranches.push_back({condition, parseNodes(tokens, index, {"elif", "else", "endif"})});
    }
    if (index < tokens.size() && tokens[index].kind == TokenKind::Statement && tokens[index].value == "else") {
        ++index;
        node.elseChildren = parseNodes(tokens, index, {"endif"});
    }
    if (index >= tokens.size() || tokens[index].value != "endif") {
        throw WordProcessingException("Unclosed if block");
    }
    ++index;
    return node;
}

Node parseFor(const std::vector<Token>& tokens, std::size_t& index) {
    Node node;
    node.kind = Node::Kind::For;
    const auto expression = trim(std::string_view(tokens[index].value).substr(4));
    const auto inPos = expression.find(" in ");
    if (inPos == std::string::npos) {
        throw WordProcessingException("for tag must use: for item in items");
    }
    node.loopVariable = trim(std::string_view(expression).substr(0, inPos));
    node.value = trim(std::string_view(expression).substr(inPos + 4));
    ++index;
    node.children = parseNodes(tokens, index, {"endfor"});
    if (index >= tokens.size() || tokens[index].value != "endfor") {
        throw WordProcessingException("Unclosed for block");
    }
    ++index;
    return node;
}

std::vector<Node> parseNodes(const std::vector<Token>& tokens, std::size_t& index, const std::set<std::string>& terminators) {
    std::vector<Node> nodes;
    while (index < tokens.size()) {
        const auto& token = tokens[index];
        if (token.kind == TokenKind::Statement) {
            const auto head = split(token.value, ' ').front();
            if (terminators.contains(head) || terminators.contains(token.value)) {
                break;
            }
            if (startsWith(token.value, "if ")) {
                nodes.push_back(parseIf(tokens, index));
                continue;
            }
            if (startsWith(token.value, "for ")) {
                nodes.push_back(parseFor(tokens, index));
                continue;
            }
            throw WordProcessingException("Unexpected template tag: " + token.value);
        }
        if (token.kind == TokenKind::Text) {
            nodes.push_back({Node::Kind::Text, token.value});
        } else if (token.kind == TokenKind::Variable) {
            nodes.push_back({Node::Kind::Variable, token.value});
        }
        ++index;
    }
    return nodes;
}

std::vector<Node> parseTemplate(std::string_view text) {
    const auto tokens = lex(text);
    std::size_t index = 0;
    auto nodes = parseNodes(tokens, index, {});
    if (index != tokens.size()) {
        throw WordProcessingException("Unexpected template block terminator: " + tokens[index].value);
    }
    return nodes;
}

void collectVariables(const std::vector<Node>& nodes, std::set<std::string>& out, const std::set<std::string>& locals = {}) {
    for (const auto& node : nodes) {
        if (node.kind == Node::Kind::Variable) {
            const auto root = rootVariable(node.value);
            if (!locals.contains(root) && !hasDefaultFilter(node.value)) {
                out.insert(root);
            }
        } else if (node.kind == Node::Kind::If) {
            const auto root = rootVariable(node.value);
            if (!locals.contains(root)) {
                out.insert(root);
            }
            collectVariables(node.children, out, locals);
            for (const auto& [condition, children] : node.elifBranches) {
                const auto branchRoot = rootVariable(condition);
                if (!locals.contains(branchRoot)) {
                    out.insert(branchRoot);
                }
                collectVariables(children, out, locals);
            }
            collectVariables(node.elseChildren, out, locals);
        } else if (node.kind == Node::Kind::For) {
            out.insert(rootVariable(node.value));
            auto nestedLocals = locals;
            nestedLocals.insert(node.loopVariable);
            nestedLocals.insert("loop");
            collectVariables(node.children, out, nestedLocals);
        }
    }
}

std::string renderNodes(
    const std::vector<Node>& nodes,
    const TemplateContext& context,
    const TemplateOptions& options,
    std::set<std::string>& usedVariables
) {
    std::string output;
    for (const auto& node : nodes) {
        if (node.kind == Node::Kind::Text) {
            output += node.value;
        } else if (node.kind == Node::Kind::Variable) {
            output += evaluateExpression(node.value, context, options, usedVariables);
        } else if (node.kind == Node::Kind::If) {
            usedVariables.insert(rootVariable(node.value));
            if (evaluateCondition(node.value, context)) {
                output += renderNodes(node.children, context, options, usedVariables);
            } else {
                bool matched = false;
                for (const auto& [condition, children] : node.elifBranches) {
                    usedVariables.insert(rootVariable(condition));
                    if (evaluateCondition(condition, context)) {
                        output += renderNodes(children, context, options, usedVariables);
                        matched = true;
                        break;
                    }
                }
                if (!matched) {
                    output += renderNodes(node.elseChildren, context, options, usedVariables);
                }
            }
        } else if (node.kind == Node::Kind::For) {
            usedVariables.insert(rootVariable(node.value));
            const auto collection = evaluatePath(node.value, context);
            if (collection.ref == nullptr || !collection.ref->isArray()) {
                if (options.strictUndefined) {
                    throw WordProcessingException("Template for expression is not an array: " + node.value);
                }
                continue;
            }
            const auto& values = collection.ref->asArray();
            for (std::size_t i = 0; i < values.size(); ++i) {
                auto scoped = context;
                scoped[node.loopVariable] = values[i];
                scoped["loop"] = TemplateValue::Object{
                    {"index", static_cast<std::int64_t>(i + 1)},
                    {"index0", static_cast<std::int64_t>(i)},
                    {"first", i == 0},
                    {"last", i + 1 == values.size()}
                };
                output += renderNodes(node.children, scoped, options, usedVariables);
            }
        }
    }
    return output;
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

bool TemplateValue::isArray() const noexcept {
    return type_ == Type::Array;
}

bool TemplateValue::isObject() const noexcept {
    return type_ == Type::Object;
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

const TemplateValue::Array& TemplateValue::asArray() const {
    if (type_ != Type::Array || array_ == nullptr) {
        throw WordProcessingException("Template value is not an array");
    }
    return *array_;
}

const TemplateValue::Object& TemplateValue::asObject() const {
    if (type_ != Type::Object || object_ == nullptr) {
        throw WordProcessingException("Template value is not an object");
    }
    return *object_;
}

std::size_t TemplateValue::size() const {
    switch (type_) {
    case Type::Null:
        return 0;
    case Type::String:
        return string_.size();
    case Type::Bool:
    case Type::Integer:
    case Type::Double:
        return 1;
    case Type::Array:
        return array_ == nullptr ? 0 : array_->size();
    case Type::Object:
        return object_ == nullptr ? 0 : object_->size();
    }
    return 0;
}

TemplateRenderResult TemplateEngine::renderText(std::string_view text, const TemplateContext& context) {
    return renderText(text, context, {});
}

TemplateRenderResult TemplateEngine::renderText(
    std::string_view text,
    const TemplateContext& context,
    const TemplateOptions& options
) {
    TemplateRenderResult result;
    const auto nodes = parseTemplate(text);
    result.text = renderNodes(nodes, context, options, result.usedVariables);
    result.diagnostics.undeclaredVariables = undeclaredVariables(text, context);
    return result;
}

std::set<std::string> TemplateEngine::variables(std::string_view text) {
    std::set<std::string> result;
    collectVariables(parseTemplate(text), result);
    return result;
}

std::set<std::string> TemplateEngine::undeclaredVariables(std::string_view text, const TemplateContext& context) {
    std::set<std::string> result;
    for (const auto& variable : variables(text)) {
        if (!variable.empty() && !context.contains(variable) && variable != "loop") {
            result.insert(variable);
        }
    }
    return result;
}

void TemplateEngine::validateText(std::string_view text) {
    (void)parseTemplate(text);
}

TemplateDiagnostics TemplateEngine::diagnostics(std::string_view text, const TemplateContext& context) {
    TemplateDiagnostics result;
    try {
        result.undeclaredVariables = undeclaredVariables(text, context);
    } catch (const WordProcessingException& ex) {
        result.errors.push_back({ex.what(), 0});
    }
    return result;
}

} // namespace cppwordkit
