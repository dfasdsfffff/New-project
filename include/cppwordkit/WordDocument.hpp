//! @file WordDocument.hpp Word 文档操作高层 API
//!
//! WordDocument 是库的主要公共接口，封装 OOXML 包的读写、文档模型操作、
//! 段落/文本编辑、图像插入、表格操作和模板渲染等完整功能。
//! 使用 PIMPL 模式，支持移动语义，禁止拷贝。

#pragma once

#include "cppwordkit/Types.hpp"
#include "cppwordkit/Template.hpp"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace cppwordkit {

class XmlPart;

/// @brief Word 文档操作核心类
///
/// 提供对 .docx 文档的完整操作能力：
/// - 生命周期：open()/create()/save()/saveAs()
/// - 内容读写：model()/replaceBody()
/// - 段落操作：paragraphCount()/paragraph()/setParagraph()/appendParagraph()/insertParagraph()/removeParagraph()
/// - 样式操作：setRunStyle()/runStyle()/setParagraphStyle()/paragraphStyle()
/// - 文本替换：replaceText()/replaceAll()（含旧版 API）
/// - 表格操作：tableCount()/table()/fillTable()/insertTableRowsAtBookmark()
/// - 图像操作：addImage()/insertImage()/insertImageAtPlaceholder()
/// - 模板渲染：render()/getUndeclaredTemplateVariables()/validateTemplate()
/// - 包访问：part()/partNames()/rawPart()/mainDocumentPart()
///
/// 仅支持移动构造/赋值，禁止拷贝以保护内部资源唯一所有权。
class WordDocument {
public:
    WordDocument();
    ~WordDocument();

    WordDocument(WordDocument&&) noexcept;
    WordDocument& operator=(WordDocument&&) noexcept;

    WordDocument(const WordDocument&) = delete;
    WordDocument& operator=(const WordDocument&) = delete;

    /// @brief 打开已有 .docx 文件
    /// @throws PackageException 文件不存在或不是有效的 OOXML 包
    static WordDocument open(const Path& path);
    /// @brief 创建空白文档（含一个空段落和节属性）
    static WordDocument create();

    /// @brief 原位保存（必须先通过 open() 或 saveAs() 设置路径）
    void save();
    /// @brief 另存为指定路径
    /// @details 完整写入同目录暂存文件后提交替换；提交失败保留原文件和当前路径。
    /// 暂存需要额外磁盘空间；不保证断电持久性或保留原文件的所有文件系统元数据。
    void saveAs(const Path& path, const SaveOptions& options = {});

    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] Path path() const;

    /// @brief 提取文档全部纯文本
    [[nodiscard]] std::string text() const;
    /// @brief 提取文档全部文本运行（含位置索引，无样式）
    [[nodiscard]] std::vector<TextRun> textRuns() const;
    /// @brief 读取完整结构化文档模型
    [[nodiscard]] DocumentModel model() const;
    /// @brief 用新模型替换文档主体内容（段落、表格、图像）
    void replaceBody(const DocumentModel& model);
    /// @brief 段落及样式 API 的索引仅包含正文直属段落，不含表格内段落
    [[nodiscard]] std::size_t paragraphCount() const;
    [[nodiscard]] Paragraph paragraph(std::size_t index) const;
    /// @brief 替换指定段落；其他正文节点和节属性保持原样，目标段落按模型重建
    void setParagraph(std::size_t index, const Paragraph& paragraph);
    void appendParagraph(const Paragraph& paragraph);
    void insertParagraph(std::size_t index, const Paragraph& paragraph);
    void removeParagraph(std::size_t index);
    /// @brief 读取位置编辑使用的段落文本：tab 为 \t，软换行为 \n，对象为 U+FFFC
    /// @details 段落索引为正文直属段落；域、修订等未支持结构会抛异常。
    [[nodiscard]] std::string paragraphText(std::size_t paragraphIndex) const;
    /// @brief 按 Unicode 标量偏移插入 UTF-8 文本，边界继承左侧 Run 格式和超链接归属
    /// @details 偏移不是字节、UTF-16 或字素簇；CRLF/CR 归一为软换行。失败不修改文档。
    void insertText(std::size_t paragraphIndex, std::size_t offset, std::string_view text);
    /// @brief 删除单段内 [offset, offset + length) 文本，可跨 Run；不能包含嵌入对象
    void deleteRange(std::size_t paragraphIndex, std::size_t offset, std::size_t length);
    /// @brief 对单段内 Unicode 标量范围应用样式补丁，未设置的属性保留；失败不修改文档
    void applyStyle(std::size_t paragraphIndex, std::size_t offset, std::size_t length, const TextStyle& style);
    void setRunStyle(std::size_t paragraphIndex, std::size_t runIndex, const TextStyle& style);
    [[nodiscard]] TextStyle runStyle(std::size_t paragraphIndex, std::size_t runIndex) const;
    void setParagraphStyle(std::size_t paragraphIndex, const ParagraphStyle& style);
    [[nodiscard]] ParagraphStyle paragraphStyle(std::size_t paragraphIndex) const;

    /// @brief 枚举 word/styles.xml 中的段落样式
    [[nodiscard]] std::vector<StyleInfo> paragraphStyles() const;
    /// @brief 枚举 word/styles.xml 中的字符样式
    [[nodiscard]] std::vector<StyleInfo> characterStyles() const;

    // 旧版 API，向后兼容
    bool ReplaceText(const std::string& placeholder, const std::string& value);
    std::size_t ReplaceText(const std::map<std::string, std::string>& replacements);

    /// @brief 替换文档中的指定文本（单次替换）
    bool replaceText(
        std::string_view search,
        std::string_view replacement,
        const ReplaceOptions& options = {}
    );

    /// @brief 批量替换（使用 unordered_map 映射）
    std::size_t replaceAll(
        const std::unordered_map<std::string, std::string>& replacements,
        const ReplaceOptions& options = {}
    );

    [[nodiscard]] std::size_t tableCount() const;
    [[nodiscard]] TableData table(std::size_t tableIndex) const;

    /// @brief 填充指定表格的单元格内容
    void fillTable(std::size_t tableIndex, const TableData& data);
    /// @brief 填充第一个表格（fillTable(0, data) 的便捷方法）
    void fillFirstTable(const TableData& data);
    /// @brief 在书签位置插入表格行并填充数据
    /// @return 插入的行数
    std::size_t insertTableRowsAtBookmark(std::string_view bookmark, const TableData& rows);
    /// @brief 从文件添加图像到包中
    /// @return 图像在包中的标识（关系 ID + 部件路径）
    [[nodiscard]] ImageId addImage(const Path& imagePath);
    /// @brief 在指定段落/run 中插入图像
    void insertImage(
        std::size_t paragraphIndex,
        std::size_t runIndex,
        const ImageId& image,
        const ImageOptions& options = {}
    );
    /// @brief 在指定占位符位置插入图像
    bool insertImageAtPlaceholder(
        std::string_view placeholder,
        const Path& imagePath,
        const ImageOptions& options = {}
    );

    /// @brief 使用模板上下文渲染文档（委托给 DocxTemplate 实现）
    void render(const TemplateContext& context);
    /// @brief 获取模板中未声明的变量
    [[nodiscard]] std::set<std::string> getUndeclaredTemplateVariables(const TemplateContext& context = {}) const;
    /// @brief 验证模板变量完整性
    void validateTemplate(const TemplateContext& context = {}) const;

    /// @brief 获取主文档部件（word/document.xml）的引用，用于高级底层操作
    [[nodiscard]] XmlPart& mainDocumentPart();
    [[nodiscard]] const XmlPart& mainDocumentPart() const;

    /// @brief 按包路径获取 XML 部件
    [[nodiscard]] std::optional<XmlPart*> part(std::string_view packagePath);
    /// @brief 获取包中所有部件名称列表
    [[nodiscard]] std::vector<std::string> partNames() const;
    /// @brief 按包路径获取部件字节；XML 返回当前编辑状态的序列化，二进制保持原样
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> rawPart(std::string_view packagePath) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cppwordkit
