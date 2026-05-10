//! @file QtDocumentMapper.hpp 在 QTextDocument 与 cppwordkit::DocumentModel 之间双向映射

#pragma once

#include <cppwordkit/CppWordKit.hpp>

#include <QTextDocument>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QTextTable>
#include <QTextFrame>
#include <QTextImageFormat>
#include <QImage>
#include <QBuffer>
#include <QByteArray>
#include <QUrl>
#include <QTemporaryFile>
#include <QDir>

#include <unordered_map>
#include <vector>
#include <string>

class ImageStore;

/**
 * @brief QTextDocument 与 cppwordkit 模型双向映射器
 *
 * 负责在 Qt 富文本文档（QTextDocument）和底层 Word 文档模型（cppwordkit::DocumentModel）之间
 * 进行双向格式转换，包含：字体样式、段落样式、对齐方式、图片、表格等。
 * 是 MVC 架构中的桥梁层，将 Qt 的格式体系映射为 OpenXML 兼容的格式体系。
 */
class QtDocumentMapper {
public:
    explicit QtDocumentMapper(ImageStore& imageStore);

    /**
     * @brief 从 cppwordkit 模型导入到 QTextDocument
     * @param doc 目标 QTextDocument
     * @param model 源 cppwordkit 文档模型
     * @param wordDoc 用于提取图片原始数据的 WordDocument
     */
    void importModel(QTextDocument& doc, const cppwordkit::DocumentModel& model, const cppwordkit::WordDocument& wordDoc);

    /**
     * @brief 从 QTextDocument 导出为 cppwordkit 模型
     * @param doc 源 QTextDocument
     * @param wordDoc 用于写入图片数据的 WordDocument
     * @return 导出的文档模型
     */
    cppwordkit::DocumentModel exportModel(const QTextDocument& doc, cppwordkit::WordDocument& wordDoc);

private:
    // cppwordkit -> Qt 格式映射
    QTextCharFormat mapTextStyle(const cppwordkit::TextStyle& style) const;
    QTextBlockFormat mapParagraphStyle(const cppwordkit::ParagraphStyle& style) const;
    Qt::Alignment mapAlignment(cppwordkit::ParagraphAlignment alignment) const;

    // Qt -> cppwordkit 格式提取
    cppwordkit::TextStyle extractTextStyle(const QTextCharFormat& fmt) const;
    cppwordkit::ParagraphStyle extractParagraphStyle(const QTextBlockFormat& fmt) const;
    cppwordkit::ParagraphAlignment extractAlignment(Qt::Alignment alignment) const;

    // 字号在 Qt（point）和 OpenXML（half-points）之间转换
    int fontSizeToHalfPoints(qreal pointSize) const;
    qreal halfPointsToFontSize(std::int32_t halfPoints) const;

    ImageStore& imageStore_;  ///< 图片资源存储引用，用于导出时查询图片数据
};
