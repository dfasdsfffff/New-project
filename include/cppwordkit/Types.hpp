//! @file Types.hpp CppWordKit 数据模型类型定义
//!
//! 定义 Word 文档的完整结构化数据模型，包含：
//! - 基础类型：TextRun（纯文本提取）、TableCell、ImageId、各种 Options
//! - 样式枚举：ParagraphAlignment、LineSpacingRule
//! - 格式结构：TextStyle（字符级样式）、ParagraphStyle（段落级样式）
//! - 文档组件：InlineImage、Run、Paragraph、Table、DocumentModel
//!
//! 设计原则：所有样式字段使用 std::optional，仅设置过的字段才会生成对应 XML 元素，
//! 未设置的字段保持文档原有格式不变。

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace cppwordkit {

using Path = std::filesystem::path;

/// @brief 纯文本运行结果，仅包含文本和位置索引，无样式信息
struct TextRun {
    std::string text;
    std::size_t paragraphIndex{};  ///< 所在段落索引（0-based）
    std::size_t runIndex{};        ///< 段落内 run 索引（0-based）
};

/// @brief 表格单元格，第一版仅支持纯文本
struct TableCell {
    std::string text;
};

using TableRow = std::vector<TableCell>;
using TableData = std::vector<TableRow>;

/// @brief 图像在 OPC 包中的标识
struct ImageId {
    std::string relationshipId;  ///< OOXML 关系 ID，如 "rId10"
    std::string partName;         ///< 包内部件路径，如 "word/media/image1.png"
};

/// @brief 文本替换选项
struct ReplaceOptions {
    bool matchCase = true;    ///< 是否区分大小写
    bool wholeWord = false;   ///< 是否全词匹配
};

/// @brief 文档保存选项
struct SaveOptions {
    bool overwrite = true;            ///< 是否覆盖已存在的文件
    bool preserveUnknownParts = true; ///< 是否保留包中未识别的部件
    bool prettyPrintXml = false;      ///< 是否格式化 XML 输出
};

/// @brief 图像插入选项
struct ImageOptions {
    std::int64_t widthEmu = 914400;               ///< 图像宽度（EMU 单位，1 inch = 914400 EMU）
    std::int64_t heightEmu = 914400;              ///< 图像高度（EMU 单位）
    std::string description = "CppWordKit image"; ///< 图像描述文字
};

/// @brief 段落对齐方式
enum class ParagraphAlignment {
    Left,     ///< 左对齐（对应 OOXML w:jc val="left"）
    Center,   ///< 居中对齐
    Right,    ///< 右对齐
    Justify   ///< 两端对齐（对应 OOXML w:jc val="both"）
};

/// @brief 行间距规则（对应 OOXML w:spacing 的 w:lineRule 属性）
enum class LineSpacingRule {
    Auto,     ///< 自动行距
    Exact,    ///< 固定值
    AtLeast   ///< 最小值
};

/// @brief 字符级（Run）样式
///
/// 每个字段对应 OOXML w:rPr 下的一个子元素。
/// 所有字段为 optional：仅非空字段才会在写入时生成对应 XML，
/// 读取时未出现的 XML 元素对应字段保持 std::nullopt。
struct TextStyle {
    std::optional<std::string> fontFamily;          ///< 字体族名称
    std::optional<std::string> colorHex;            ///< 文字颜色（十六进制，不含 #）
    std::optional<std::string> backgroundColorHex;  ///< 文字背景色（w:shd w:fill）
    std::optional<std::int32_t> fontSizeHalfPoints; ///< 字号（半点，24 = 12pt）
    std::optional<bool> bold;                       ///< 粗体（w:b）
    std::optional<bool> italic;                     ///< 斜体（w:i）
    std::optional<bool> underline;                  ///< 下划线（w:u）
    std::optional<bool> strike;                     ///< 删除线（w:strike）
    std::optional<bool> superscript;                ///< 上标（w:vertAlign val="superscript"）
    std::optional<bool> subscript;                  ///< 下标（w:vertAlign val="subscript"）
};

/// @brief 段落级样式（对应 OOXML w:pPr 下的元素）
struct ParagraphStyle {
    std::optional<ParagraphAlignment> alignment;       ///< 段落对齐
    std::optional<std::int32_t> lineSpacingTwips;     ///< 行间距（twips，1/20 磅）
    std::optional<LineSpacingRule> lineSpacingRule;    ///< 行间距规则
    std::optional<std::int32_t> spacingBeforeTwips;   ///< 段前间距（twips）
    std::optional<std::int32_t> spacingAfterTwips;    ///< 段后间距（twips）
    std::optional<std::int32_t> firstLineIndentTwips; ///< 首行缩进（twips）
    std::optional<std::int32_t> leftIndentTwips;      ///< 左缩进（twips）
    std::optional<std::int32_t> rightIndentTwips;     ///< 右缩进（twips）
};

/// @brief 内联图像（对应 OOXML w:drawing/wp:inline/pic:pic）
struct InlineImage {
    ImageId image;          ///< 图像在包中的标识
    ImageOptions options;   ///< 图像显示选项（尺寸、描述）
    std::string description; ///< 图像描述
};

/// @brief 文本运行（Run），包含文本内容和可选的内联图像
///
/// 对应 OOXML w:r 元素。一个 Run 可以包含文本、多张内联图像。
/// 同一 Run 内所有文本共享同一个 TextStyle。
struct Run {
    std::string text;
    TextStyle style;
    std::vector<InlineImage> images; ///< 本 Run 中嵌入的内联图像列表
};

/// @brief 段落，包含若干 Run 列表和段落级样式
/// 对应 OOXML w:p 元素
struct Paragraph {
    std::vector<Run> runs;
    ParagraphStyle style;
};

/// @brief 表格（第一版仅支持纯文本单元格）
/// 对应 OOXML w:tbl 元素
struct Table {
    TableData rows;
};

/// @brief 文档模型 —— 编辑器和模板模块使用的核心数据结构
///
/// 包含段落实体和表格实体。注意：paragraphs 和 tables 是分离存储的，
/// 顺序信息在读/写往返时可能丢失。（v1 已知限制）
struct DocumentModel {
    std::vector<Paragraph> paragraphs;
    std::vector<Table> tables;
};

} // namespace cppwordkit
