//! @file DocxTemplate.cpp 基于模板引擎的 Word 文档渲染，支持段落/行级结构标签（{%p if %}、{%tr for %}）和行内变量插值

#include "cppwordkit/DocxTemplate.hpp"

#include "cppwordkit/Error.hpp"
#include "cppwordkit/XmlPart.hpp"

#include <algorithm>
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

// 判断部件是否支持模板渲染：仅文档正文（document.xml）和页眉/页脚参与模板处理
bool isTemplateRenderablePart(std::string_view partName) {
    return partName == "word/document.xml" ||
        (startsWith(partName, "word/header") && endsWith(partName, ".xml")) ||
        (startsWith(partName, "word/footer") && endsWith(partName, ".xml"));
}

// 提取部件中所有 w:t 文本节点的拼接结果（不含 XML 标签）
std::string wordText(const XmlPart& part) {
    const auto texts = part.textsByXPath("//w:t");
    std::ostringstream output;
    for (const auto& text : texts) {
        output << text;
    }
    return output.str();
}

// 在 context 中按路径查找变量（简化版，不含过滤器支持）
const TemplateValue* findInContext(const TemplateContext& context, std::string_view path) {
    std::size_t start = 0;
    const TemplateValue* current = nullptr;
    while (start <= path.size()) {
        const auto end = path.find('.', start);
        const auto name = std::string(path.substr(start, end == std::string_view::npos ? path.size() - start : end - start));
        if (name.empty()) {
            return nullptr;
        }
        if (current == nullptr) {
            const auto it = context.find(name);
            if (it == context.end()) {
                return nullptr;
            }
            current = &it->second;
        } else {
            current = current->find(name);
            if (current == nullptr) {
                return nullptr;
            }
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return current;
}

// 对模板条件表达式求值（仅支持 not 前缀和简单的真值判断）
bool conditionTruthy(std::string_view expression, const TemplateContext& context) {
    auto expr = trim(expression);
    if (startsWith(expr, "not ")) {
        return !conditionTruthy(std::string_view(expr).substr(4), context);
    }
    const auto* value = findInContext(context, expr);
    return value != nullptr && value->isTruthy();
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

// 在 w:t 文本节点内执行模板变量渲染（找到 <w:t> 内容并用渲染结果替换）
std::string renderTextNodes(std::string xml, const TemplateContext& context) {
    std::size_t cursor = 0;
    while ((cursor = xml.find("<w:t", cursor)) != std::string::npos) {
        const auto afterName = cursor + 4;
        if (afterName >= xml.size() || (xml[afterName] != '>' && !std::isspace(static_cast<unsigned char>(xml[afterName])))) {
            cursor = afterName;
            continue;
        }
        const auto openEnd = xml.find('>', cursor);
        if (openEnd == std::string::npos) {
            break;
        }
        const auto close = xml.find("</w:t>", openEnd);
        if (close == std::string::npos) {
            break;
        }
        const auto textStart = openEnd + 1;
        const auto text = xml.substr(textStart, close - textStart);
        const auto rendered = TemplateEngine::renderText(text, context).text;
        xml.replace(textStart, close - textStart, xmlEscape(rendered));
        cursor = textStart + rendered.size();
    }
    return xml;
}

struct ElementSpan {
    std::size_t start{};
    std::size_t end{};
    std::string xml;
};

// 收集 XML 中所有指定 w:tagName 元素的开始-结束位置区间
std::vector<ElementSpan> collectElements(const std::string& xml, std::string_view tagName) {
    std::vector<ElementSpan> elements;
    const auto openNeedle = "<w:" + std::string(tagName);
    const auto closeNeedle = "</w:" + std::string(tagName) + ">";
    std::size_t cursor = 0;
    while ((cursor = xml.find(openNeedle, cursor)) != std::string::npos) {
        const auto close = xml.find(closeNeedle, cursor);
        if (close == std::string::npos) {
            break;
        }
        const auto end = close + closeNeedle.size();
        elements.push_back({cursor, end, xml.substr(cursor, end - cursor)});
        cursor = end;
    }
    return elements;
}

// 从元素的 XML 中提取结构标签语句（如 {%tr for item in items %} 中的 "for item in items"）
std::string extractStructureStatement(std::string_view xml, std::string_view marker) {
    const auto openNeedle = "{%" + std::string(marker);
    const auto open = xml.find(openNeedle);
    if (open == std::string_view::npos) {
        return {};
    }
    const auto close = xml.find("%}", open);
    if (close == std::string_view::npos) {
        throw WordProcessingException("Unclosed docxtpl structure tag");
    }
    return trim(xml.substr(open + openNeedle.size(), close - open - openNeedle.size()));
}

std::string joinElementXml(const std::vector<ElementSpan>& elements, std::size_t first, std::size_t lastExclusive) {
    std::string result;
    for (std::size_t i = first; i < lastExclusive; ++i) {
        result += elements[i].xml;
    }
    return result;
}

std::size_t findMatchingEnd(
    const std::vector<ElementSpan>& elements,
    std::size_t start,
    std::string_view marker,
    std::string_view beginKeyword,
    std::string_view endKeyword
) {
    int depth = 0;
    for (std::size_t i = start; i < elements.size(); ++i) {
        const auto statement = extractStructureStatement(elements[i].xml, marker);
        if (startsWith(statement, beginKeyword)) {
            ++depth;
        } else if (statement == endKeyword) {
            --depth;
            if (depth == 0) {
                return i;
            }
        }
    }
    throw WordProcessingException("Unclosed docxtpl structure block: " + std::string(beginKeyword));
}

// 渲染 for 块：对集合中的每个元素创建子作用域并渲染循环体 XML
std::string renderForBlock(
    std::string_view statement,
    std::string_view bodyXml,
    const TemplateContext& context
) {
    const auto expression = trim(std::string_view(statement).substr(4));
    const auto inPos = expression.find(" in ");
    if (inPos == std::string::npos) {
        throw WordProcessingException("for tag must use: for item in items");
    }
    const auto loopName = trim(std::string_view(expression).substr(0, inPos));
    const auto collectionName = trim(std::string_view(expression).substr(inPos + 4));
    const auto* collection = findInContext(context, collectionName);
    if (collection == nullptr || !collection->isArray()) {
        throw WordProcessingException("Template for expression is not an array: " + collectionName);
    }

    std::string result;
    const auto& values = collection->asArray();
    for (std::size_t i = 0; i < values.size(); ++i) {
        auto scoped = context;
        scoped[loopName] = values[i];
        scoped["loop"] = TemplateValue::Object{
            {"index", static_cast<std::int64_t>(i + 1)},
            {"index0", static_cast<std::int64_t>(i)},
            {"first", i == 0},
            {"last", i + 1 == values.size()}
        };
        result += renderTextNodes(std::string(bodyXml), scoped);
    }
    return result;
}

// 处理指定标签级别（tr/p）的结构块：查找 {%for%} 和 {%if%} 标签，渲染后替换对应的 XML
std::string processStructuredBlocks(
    const std::string& xml,
    std::string_view tagName,
    std::string_view marker,
    const TemplateContext& context
) {
    const auto elements = collectElements(xml, tagName);
    if (elements.empty()) {
        return xml;
    }

    std::string result;
    std::size_t xmlCursor = 0;
    for (std::size_t i = 0; i < elements.size(); ++i) {
        result += xml.substr(xmlCursor, elements[i].start - xmlCursor);
        const auto statement = extractStructureStatement(elements[i].xml, marker);
        if (startsWith(statement, "for ")) {
            const auto endIndex = findMatchingEnd(elements, i, marker, "for ", "endfor");
            const auto body = joinElementXml(elements, i + 1, endIndex);
            result += renderForBlock(statement, body, context);
            xmlCursor = elements[endIndex].end;
            i = endIndex;
            continue;
        }
        if (startsWith(statement, "if ")) {
            const auto endIndex = findMatchingEnd(elements, i, marker, "if ", "endif");
            const auto body = joinElementXml(elements, i + 1, endIndex);
            if (conditionTruthy(std::string_view(statement).substr(3), context)) {
                result += renderTextNodes(body, context);
            }
            xmlCursor = elements[endIndex].end;
            i = endIndex;
            continue;
        }
        if (!statement.empty() && (statement == "endif" || statement == "endfor")) {
            throw WordProcessingException("Unexpected docxtpl structure terminator: " + statement);
        }
        result += elements[i].xml;
        xmlCursor = elements[i].end;
    }
    result += xml.substr(xmlCursor);
    return result;
}

// 移除行内结构标签的标记文本（{%p %}、{%tr %}、{%tc %}、{%r %}），这些标记在渲染后不再需要
std::string removeInlineStructureTags(std::string xml) {
    for (const std::string marker : {"p", "tr", "tc", "r"}) {
        std::size_t cursor = 0;
        const auto openNeedle = "{%" + marker;
        while ((cursor = xml.find(openNeedle, cursor)) != std::string::npos) {
            const auto close = xml.find("%}", cursor);
            if (close == std::string::npos) {
                throw WordProcessingException("Unclosed docxtpl structure tag");
            }
            xml.erase(cursor, close + 2 - cursor);
        }
    }
    return xml;
}

// 模板渲染主流程：先处理行级（tr）块，再处理段落级（p）块，最后清除行内标签标记
std::string renderPartXml(const std::string& xml, const TemplateContext& context) {
    auto rendered = processStructuredBlocks(xml, "tr", "tr", context);
    rendered = processStructuredBlocks(rendered, "p", "p", context);
    rendered = removeInlineStructureTags(std::move(rendered));
    return rendered;
}

std::vector<std::string> inlineTemplateExpressions(std::string_view text) {
    std::vector<std::string> result;
    std::size_t cursor = 0;
    while (cursor < text.size()) {
        const auto variable = text.find("{{", cursor);
        const auto comment = text.find("{#", cursor);
        const auto open = std::min(variable, comment);
        if (open == std::string_view::npos) {
            break;
        }
        const auto closeNeedle = open == variable ? "}}" : "#}";
        const auto close = text.find(closeNeedle, open + 2);
        if (close == std::string_view::npos) {
            throw WordProcessingException("Unclosed template variable");
        }
        result.push_back(std::string(text.substr(open, close - open + 2)));
        cursor = close + 2;
    }
    return result;
}

void renderInlineExpressions(XmlPart& part, const TemplateContext& context) {
    const auto text = wordText(part);
    for (const auto& expression : inlineTemplateExpressions(text)) {
        const auto rendered = startsWith(expression, "{#")
            ? std::string{}
            : TemplateEngine::renderText(expression, context).text;
        part.replaceWordText(expression, rendered);
    }
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

        **partRef = XmlPart::fromString(renderPartXml((*partRef)->toString(), context));
        renderInlineExpressions(**partRef, context);
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

//! 从文件路径打开文档并创建模板对象
DocxTemplate DocxTemplate::open(const Path& path) {
    return DocxTemplate(WordDocument::open(path));
}

//! 执行模板渲染：先校验变量完整性，再对文档的所有可渲染部件（正文/页眉/页脚）执行渲染
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

//! 校验模板：确保所有用到的变量在 context 中都有声明，且模板语法正确
void DocxTemplate::validateTemplate(const TemplateContext& context) const {
    validateDocument(const_cast<WordDocument&>(document_), context);
}

WordDocument& DocxTemplate::document() noexcept {
    return document_;
}

const WordDocument& DocxTemplate::document() const noexcept {
    return document_;
}

//! 在 WordDocument 上直接执行模板渲染（便捷方法）
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
