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

class QtDocumentMapper {
public:
    explicit QtDocumentMapper(ImageStore& imageStore);

    void importModel(QTextDocument& doc, const cppwordkit::DocumentModel& model, const cppwordkit::WordDocument& wordDoc);
    cppwordkit::DocumentModel exportModel(const QTextDocument& doc, cppwordkit::WordDocument& wordDoc);

private:
    QTextCharFormat mapTextStyle(const cppwordkit::TextStyle& style) const;
    QTextBlockFormat mapParagraphStyle(const cppwordkit::ParagraphStyle& style) const;
    Qt::Alignment mapAlignment(cppwordkit::ParagraphAlignment alignment) const;

    cppwordkit::TextStyle extractTextStyle(const QTextCharFormat& fmt) const;
    cppwordkit::ParagraphStyle extractParagraphStyle(const QTextBlockFormat& fmt) const;
    cppwordkit::ParagraphAlignment extractAlignment(Qt::Alignment alignment) const;

    int fontSizeToHalfPoints(qreal pointSize) const;
    qreal halfPointsToFontSize(std::int32_t halfPoints) const;

    ImageStore& imageStore_;
};
