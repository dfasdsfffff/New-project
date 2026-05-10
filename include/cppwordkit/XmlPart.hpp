//! @file XmlPart.hpp OOXML XML 部件操作
//!
//! XmlPart 是对单个 OOXML XML 部件（如 word/document.xml、word/_rels/document.xml.rels）
//! 的封装，提供 XPath 查询、文本替换、样式读写等底层能力。
//! 使用 PIMPL 模式隐藏 libxml2 依赖，确保 ABI 稳定。

#pragma once

#include "cppwordkit/Types.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cppwordkit {

/// @brief OOXML XML 部件，封装 libxml2 的 XML 文档节点树操作
///
/// 支持 XPath 查询/修改、文本替换、跨 w:t 节点替换（处理 OOXML 文本分片）、
/// 样式读写及表格行插入。所有操作基于注册的 OOXML 命名空间前缀。
///
/// 仅支持移动构造/赋值，禁止拷贝（libxml2 文档指针唯一所有权）。
class XmlPart {
public:
    XmlPart();
    explicit XmlPart(std::string xmlContent);
    ~XmlPart();

    XmlPart(XmlPart&&) noexcept;
    XmlPart& operator=(XmlPart&&) noexcept;

    XmlPart(const XmlPart&) = delete;
    XmlPart& operator=(const XmlPart&) = delete;

    /// @brief 从 XML 字符串创建部件
    static XmlPart fromString(std::string xmlContent);
    /// @brief 从文件路径读取 XML 并创建部件
    static XmlPart fromFile(const std::string& path);

    /// @brief 序列化为 XML 字符串
    [[nodiscard]] std::string toString() const;
    /// @brief 保存 XML 到文件
    void saveToFile(const std::string& path) const;

    [[nodiscard]] bool empty() const noexcept;

    /// @brief 获取文档中所有 w:t 节点的文本内容列表
    [[nodiscard]] std::vector<std::string> textNodes() const;
    /// @brief XPath 查询第一个匹配节点的文本
    [[nodiscard]] std::optional<std::string> firstTextByXPath(std::string_view xpath) const;
    /// @brief XPath 查询所有匹配节点的文本列表
    [[nodiscard]] std::vector<std::string> textsByXPath(std::string_view xpath) const;

    /// @brief 在 XML 文本节点中进行简单文本替换（不处理跨 w:t 分片）
    bool replaceText(std::string_view search, std::string_view replacement, bool matchCase = true);
    /// @brief Word 文本替换：处理跨多个 w:t 节点的占位符
    bool replaceWordText(std::string_view search, std::string_view replacement, bool matchCase = true);
    /// @brief 在书签位置插入表格行
    std::size_t insertTableRowsAtBookmark(std::string_view bookmark, const TableData& rows);
    /// @brief Word 文本替换为 XML 片段（用于图像占位符替换为 w:drawing 元素）
    bool replaceWordTextWithXml(std::string_view search, std::string_view replacementXml);
    /// @brief 设置指定段落中指定 run 的字符样式（创建或修改 w:rPr 子元素）
    void setRunStyle(std::size_t paragraphIndex, std::size_t runIndex, const TextStyle& style);
    /// @brief 读取指定段落中指定 run 的字符样式
    [[nodiscard]] TextStyle runStyle(std::size_t paragraphIndex, std::size_t runIndex) const;
    /// @brief 设置指定段落的段落样式（创建或修改 w:pPr 子元素）
    void setParagraphStyle(std::size_t paragraphIndex, const ParagraphStyle& style);
    /// @brief 读取指定段落的段落样式
    [[nodiscard]] ParagraphStyle paragraphStyle(std::size_t paragraphIndex) const;

    /// @brief 通过 XPath 设置单个节点的文本内容
    void setTextByXPath(std::string_view xpath, std::string_view value);
    /// @brief 向指定 XPath 节点的子节点列表末尾追加 XML 子元素
    void appendChildXml(std::string_view parentXPath, std::string_view childXml);
    /// @brief XPath 节点计数
    [[nodiscard]] std::size_t countByXPath(std::string_view xpath) const;
    /// @brief 通过 XPath 批量设置多个节点的文本内容
    void setTextsByXPath(std::string_view xpath, const std::vector<std::string>& values);

    /// @brief 检查是否存在匹配 XPath 的节点
    [[nodiscard]] bool hasXPath(std::string_view xpath) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cppwordkit
