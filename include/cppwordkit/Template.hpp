//! @file Template.hpp Jinja2/docxctl 风格模板引擎
//!
//! 提供纯文本模板渲染引擎 TemplateEngine 和模板值容器 TemplateValue。
//! 支持变量插值 {{ var }}、过滤器 {{ var|upper }}、条件 {% if %}、
//! 循环 {% for %}、注释 {# ... #}，以及段落/表格/单元格结构块。
//! 设计为与 docx 解耦，可独立用于任何文本模板场景。

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

/// @brief 模板值容器，支持多种基础类型和嵌套结构
///
/// 对应模板引擎中的变量值，支持 Null、String、Bool、Integer、Double、
/// Array、Object 七种类型。提供自动类型推断构造和运行时类型查询。
/// Array/Object 内部使用 shared_ptr 管理以实现高效拷贝。
class TemplateValue {
public:
    using Array = std::vector<TemplateValue>;
    using Object = std::map<std::string, TemplateValue>;

    TemplateValue();
    TemplateValue(const char* value);      ///< C 字符串 → String 类型
    TemplateValue(std::string value);       ///< std::string → String 类型
    TemplateValue(bool value);             ///< bool → Bool 类型
    TemplateValue(std::int64_t value);     ///< int64_t → Integer 类型
    TemplateValue(double value);           ///< double → Double 类型
    TemplateValue(Array value);            ///< 数组 → Array 类型
    TemplateValue(Object value);           ///< 对象 → Object 类型

    [[nodiscard]] bool isNull() const noexcept;
    [[nodiscard]] bool isArray() const noexcept;
    [[nodiscard]] bool isObject() const noexcept;
    /// @brief 判断值在布尔上下文中的真值（Null/空/0/false → false，其他 → true）
    [[nodiscard]] bool isTruthy() const;
    /// @brief 将值转换为字符串表示
    [[nodiscard]] std::string toString() const;
    /// @brief 按路径 "a.b.c" 或 "arr.0.name" 在嵌套结构中查找子值
    [[nodiscard]] const TemplateValue* find(std::string_view path) const;
    [[nodiscard]] const Array& asArray() const;
    [[nodiscard]] const Object& asObject() const;
    /// @brief 获取数组/Object 的元素数量（其他类型返回 0）
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

/// @brief 模板上下文：变量名 → 值的映射表
using TemplateContext = std::map<std::string, TemplateValue>;

/// @brief 模板渲染选项
struct TemplateOptions {
    bool strictUndefined = true;   ///< 是否存在未定义变量时报错
    bool autoEscape = false;       ///< 是否自动转义 HTML
    bool keepEmptyParagraphs = false; ///< 是否保留空段落
};

/// @brief 模板诊断信息（单条）
struct TemplateDiagnostic {
    std::string message;
    std::size_t position{};
};

/// @brief 模板诊断结果集合
struct TemplateDiagnostics {
    std::vector<TemplateDiagnostic> errors;
    std::set<std::string> undeclaredVariables;  ///< 模板中出现但上下文未提供的变量

    [[nodiscard]] bool ok() const noexcept { return errors.empty() && undeclaredVariables.empty(); }
};

/// @brief 模板渲染结果
struct TemplateRenderResult {
    std::string text;                     ///< 渲染后的文本
    std::set<std::string> usedVariables;  ///< 渲染过程中使用的变量
    TemplateDiagnostics diagnostics;      ///< 诊断信息
};

/// @brief 模板引擎（纯文本级，与 docx 格式解耦）
///
/// 支持 Jinja2 兼容语法：变量插值、过滤器链、条件、循环、注释。
/// 所有方法为静态方法，无内部状态，线程安全。
class TemplateEngine {
public:
    /// @brief 渲染模板文本
    static TemplateRenderResult renderText(std::string_view text, const TemplateContext& context);
    /// @brief 带选项的渲染
    static TemplateRenderResult renderText(
        std::string_view text,
        const TemplateContext& context,
        const TemplateOptions& options
    );
    /// @brief 提取模板中所有变量名
    static std::set<std::string> variables(std::string_view text);
    /// @brief 提取模板中声明但未在上下文中定义的变量
    static std::set<std::string> undeclaredVariables(std::string_view text, const TemplateContext& context);
    /// @brief 验证模板语法正确性
    static void validateText(std::string_view text);
    /// @brief 完整诊断（语法错误 + 未定义变量）
    static TemplateDiagnostics diagnostics(std::string_view text, const TemplateContext& context = {});
};

} // namespace cppwordkit
