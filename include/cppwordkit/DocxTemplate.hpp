//! @file DocxTemplate.hpp docx 模板渲染类
//!
//! DocxTemplate 封装了基于 WordDocument 的模板渲染流程：
//! 打开模板 → 填充变量 → 渲染 → 保存。
//! 模板变量使用 Jinja2/doxctpl 风格语法：{{ variable }}、{% if %}、{% for %} 等。

#pragma once

#include "cppwordkit/Template.hpp"
#include "cppwordkit/Types.hpp"
#include "cppwordkit/WordDocument.hpp"

#include <set>

namespace cppwordkit {

/// @brief docx 模板 — 打开 .docx 模板文件，渲染变量并输出
///
/// 使用示例：
/// @code
///   auto tpl = DocxTemplate::open("template.docx");
///   TemplateContext ctx = {{"name", "张三"}, {"items", TemplateValue::Array{...}}};
///   tpl.render(ctx);
///   tpl.saveAs("output.docx");
/// @endcode
class DocxTemplate {
public:
    DocxTemplate();
    explicit DocxTemplate(WordDocument document);

    /// @brief 打开 .docx 模板文件
    static DocxTemplate open(const Path& path);

    /// @brief 使用模板上下文渲染文档中所有模板变量和结构块
    void render(const TemplateContext& context);
    void save();
    void saveAs(const Path& path, const SaveOptions& options = {});

    /// @brief 获取模板中声明但未在上下文中定义的变量集合
    [[nodiscard]] std::set<std::string> getUndeclaredTemplateVariables(const TemplateContext& context = {}) const;
    /// @brief 验证模板变量完整性，缺失变量将抛出异常
    void validateTemplate(const TemplateContext& context = {}) const;

    /// @brief 获取内部 WordDocument 的可变引用
    [[nodiscard]] WordDocument& document() noexcept;
    /// @brief 获取内部 WordDocument 的只读引用
    [[nodiscard]] const WordDocument& document() const noexcept;

private:
    WordDocument document_;
};

} // namespace cppwordkit
