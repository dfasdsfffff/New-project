//! @file QtDocumentMapper.cpp 模型双向映射器实现

#include "QtDocumentMapper.hpp"
#include "ImageStore.hpp"

#include <QTextList>
#include <QTextObject>
#include <QColor>
#include <QFile>
#include <QTemporaryFile>
#include <QDir>

QtDocumentMapper::QtDocumentMapper(ImageStore& imageStore)
    : imageStore_(imageStore)
{
}

/**
 * 将 cppwordkit 字体样式映射为 QTextCharFormat
 * 覆盖字体、字号（half-points -> point）、粗体、斜体、下划线、删除线、上下标、前景/背景色
 */
QTextCharFormat QtDocumentMapper::mapTextStyle(const cppwordkit::TextStyle& style) const
{
    QTextCharFormat fmt;

    if (style.fontFamily) {
        fmt.setFontFamilies({QString::fromStdString(*style.fontFamily)});
    }
    if (style.fontSizeHalfPoints) {
        fmt.setFontPointSize(halfPointsToFontSize(*style.fontSizeHalfPoints));
    }
    if (style.bold) {
        fmt.setFontWeight(*style.bold ? QFont::Bold : QFont::Normal);
    }
    if (style.italic) {
        fmt.setFontItalic(*style.italic);
    }
    if (style.underline) {
        fmt.setUnderlineStyle(*style.underline ? QTextCharFormat::SingleUnderline : QTextCharFormat::NoUnderline);
    }
    if (style.strike) {
        fmt.setFontStrikeOut(*style.strike);
    }
    if (style.superscript && *style.superscript) {
        fmt.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
    }
    if (style.subscript && *style.subscript) {
        fmt.setVerticalAlignment(QTextCharFormat::AlignSubScript);
    }
    if (style.colorHex) {
        fmt.setForeground(QColor(QString::fromStdString("#" + *style.colorHex)));
    }
    if (style.backgroundColorHex) {
        fmt.setBackground(QColor(QString::fromStdString("#" + *style.backgroundColorHex)));
    }

    return fmt;
}

/**
 * 将 cppwordkit 段落样式映射为 QTextBlockFormat
 * 对齐方式、段前/段后间距、左/右缩进、首行缩进（twips -> points 换算）
 */
QTextBlockFormat QtDocumentMapper::mapParagraphStyle(const cppwordkit::ParagraphStyle& style) const
{
    QTextBlockFormat fmt;

    if (style.alignment) {
        fmt.setAlignment(mapAlignment(*style.alignment));
    }
    if (style.spacingBeforeTwips) {
        fmt.setTopMargin(*style.spacingBeforeTwips / 20.0);
    }
    if (style.spacingAfterTwips) {
        fmt.setBottomMargin(*style.spacingAfterTwips / 20.0);
    }
    if (style.leftIndentTwips) {
        fmt.setLeftMargin(*style.leftIndentTwips / 20.0);
    }
    if (style.rightIndentTwips) {
        fmt.setRightMargin(*style.rightIndentTwips / 20.0);
    }
    if (style.firstLineIndentTwips) {
        fmt.setTextIndent(*style.firstLineIndentTwips / 20.0);
    }

    return fmt;
}

Qt::Alignment QtDocumentMapper::mapAlignment(cppwordkit::ParagraphAlignment alignment) const
{
    switch (alignment) {
        case cppwordkit::ParagraphAlignment::Left:    return Qt::AlignLeft;
        case cppwordkit::ParagraphAlignment::Center:  return Qt::AlignHCenter;
        case cppwordkit::ParagraphAlignment::Right:   return Qt::AlignRight;
        case cppwordkit::ParagraphAlignment::Justify: return Qt::AlignJustify;
    }
    return Qt::AlignLeft;
}

int QtDocumentMapper::fontSizeToHalfPoints(qreal pointSize) const
{
    return static_cast<std::int32_t>(pointSize * 2.0);
}

qreal QtDocumentMapper::halfPointsToFontSize(std::int32_t halfPoints) const
{
    return halfPoints / 2.0;
}

/**
 * 从 cppwordkit::DocumentModel 导入到 QTextDocument
 * 处理段落（含文本和图片 run）、表格的完整导入流程
 */
void QtDocumentMapper::importModel(QTextDocument& doc, const cppwordkit::DocumentModel& model, const cppwordkit::WordDocument& wordDoc)
{
    doc.clear();
    imageStore_.clear();
    QTextCursor cursor(&doc);

    bool first = true;

    // 逐段落导入
    for (const auto& para : model.paragraphs) {
        // 除第一个段落外，每个段落前插入换块
        if (!first) {
            cursor.insertBlock();
        }
        first = false;

        QTextBlockFormat blockFmt = mapParagraphStyle(para.style);
        cursor.setBlockFormat(blockFmt);

        // 逐 run 导入，每个 run 可能包含文本和图片
        for (const auto& run : para.runs) {
            QTextCharFormat charFmt = mapTextStyle(run.style);

            // 处理 run 中的内联图片
            for (const auto& img : run.images) {
                auto rawData = wordDoc.rawPart(img.image.partName);
                if (rawData) {
                    // 从 partName 推断图片格式
                    QString format = "png";
                    auto dotPos = img.image.partName.rfind('.');
                    if (dotPos != std::string::npos) {
                        format = QString::fromStdString(img.image.partName.substr(dotPos + 1));
                    }
                    QByteArray imgBytes(reinterpret_cast<const char*>(rawData->data()),
                                       static_cast<int>(rawData->size()));
                    QString resourceName = imageStore_.addImage(imgBytes, format);
                    if (!resourceName.isEmpty()) {
                        // EMU（English Metric Units）到 points 的换算：1pt = 9144 EMU
                        QTextImageFormat imgFmt;
                        imgFmt.setName(resourceName);
                        imgFmt.setWidth(img.options.widthEmu / 9144.0);
                        imgFmt.setHeight(img.options.heightEmu / 9144.0);
                        cursor.insertImage(imgFmt, QTextFrameFormat::InFlow);
                    }
                }
            }

            // 插入 run 的文本内容
            if (!run.text.empty()) {
                cursor.insertText(QString::fromStdString(run.text), charFmt);
            }
        }
    }

    // 导入表格
    for (const auto& tableModel : model.tables) {
        if (tableModel.rows.empty()) continue;

        cursor.insertBlock();
        int numRows = static_cast<int>(tableModel.rows.size());
        int numCols = 0;
        for (const auto& row : tableModel.rows) {
            numCols = std::max(numCols, static_cast<int>(row.size()));
        }
        if (numCols == 0) continue;

        QTextTableFormat tableFmt;
        tableFmt.setBorder(1);
        tableFmt.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
        tableFmt.setCellPadding(4);
        tableFmt.setCellSpacing(0);

        QTextTable* table = cursor.insertTable(numRows, numCols, tableFmt);

        // 逐单元格填充文本内容
        for (int r = 0; r < numRows; ++r) {
            for (int c = 0; c < numCols; ++c) {
                QTextTableCell cell = table->cellAt(r, c);
                QTextCursor cellCursor = cell.firstCursorPosition();
                if (c < static_cast<int>(tableModel.rows[r].size())) {
                    cellCursor.insertText(
                        QString::fromStdString(tableModel.rows[r][c].text));
                }
            }
        }

        // 将光标移到文档末尾，以便后续内容继续
        cursor.setPosition(doc.rootFrame()->lastPosition());
    }

    // 将所有图片注册为 QTextDocument 的资源，使 QTextEdit 能正确渲染
    imageStore_.addImagesToDocument(doc);
}

/**
 * 从 QTextDocument 导出为 cppwordkit::DocumentModel
 * 遍历文档的帧结构（rootFrame），识别段落和表格并还原为模型数据
 */
cppwordkit::DocumentModel QtDocumentMapper::exportModel(const QTextDocument& doc, cppwordkit::WordDocument& wordDoc)
{
    cppwordkit::DocumentModel model;
    QTextFrame* rootFrame = doc.rootFrame();
    if (!rootFrame) return model;

    // 遍历文档根帧的所有子元素：区分表格（QTextFrame 子类）和普通段落
    for (auto it = rootFrame->begin(); it != rootFrame->end(); ++it) {
        QTextBlock block = it.currentBlock();
        QTextFrame* childFrame = it.currentFrame();

        if (childFrame) {
            // 处理表格：遍历行和列，提取单元格文本
            QTextTable* table = qobject_cast<QTextTable*>(childFrame);
            if (table) {
                cppwordkit::TableData tableData;
                int rows = table->rows();
                int cols = table->columns();

                for (int r = 0; r < rows; ++r) {
                    cppwordkit::TableRow tableRow;
                    for (int c = 0; c < cols; ++c) {
                        QTextTableCell cell = table->cellAt(r, c);
                        QTextCursor cellStart = cell.firstCursorPosition();
                        QTextCursor cellEnd = cell.lastCursorPosition();
                        cellStart.setPosition(cellEnd.position(), QTextCursor::KeepAnchor);

                        QString text = cellStart.selectedText().trimmed();
                        tableRow.push_back(cppwordkit::TableCell{text.toStdString()});
                    }
                    tableData.push_back(std::move(tableRow));
                }

                model.tables.push_back(cppwordkit::Table{std::move(tableData)});
            }
        } else if (block.isValid()) {
            // 处理段落：提取段落样式后遍历文本片段
            cppwordkit::Paragraph para;
            para.style = extractParagraphStyle(block.blockFormat());

            cppwordkit::Run currentRun;
            QTextCharFormat prevFmt;

            for (auto fragIt = block.begin(); !fragIt.atEnd(); ++fragIt) {
                QTextFragment fragment = fragIt.fragment();
                if (!fragment.isValid()) continue;

                QTextCharFormat fragFmt = fragment.charFormat();

                // 如果是图片片段，将图片数据写入 WordDocument 并记录 InlineImage
                if (fragFmt.isImageFormat()) {
                    QTextImageFormat imgFmt = fragFmt.toImageFormat();
                    QString resourceName = imgFmt.name();

                    QByteArray imgData = imageStore_.imageData(resourceName);
                    if (!imgData.isEmpty()) {
                        // 将图片数据写入临时文件，通过 cppwordkit 的 addImage 导入
                        QTemporaryFile* tempFile = new QTemporaryFile(
                            QDir::temp().filePath("cppwordkit_img_XXXXXX.png"));
                        tempFile->setAutoRemove(true);
                        if (tempFile->open()) {
                            tempFile->write(imgData);
                            tempFile->close();
                        }

                        try {
                            auto imageId = wordDoc.addImage(
                                tempFile->fileName().toStdString());

                            // EMU 换算：若 Qt 端记录了宽高则转换为 EMU，否则使用默认值
                            cppwordkit::ImageOptions opts;
                            if (imgFmt.width() > 0) {
                                opts.widthEmu = static_cast<std::int64_t>(imgFmt.width() * 9144.0);
                            }
                            if (imgFmt.height() > 0) {
                                opts.heightEmu = static_cast<std::int64_t>(imgFmt.height() * 9144.0);
                            }
                            opts.description = imgFmt.name().toStdString();

                            cppwordkit::InlineImage inlineImg;
                            inlineImg.image = imageId;
                            inlineImg.options = opts;

                            currentRun.images.push_back(std::move(inlineImg));
                        } catch (const std::exception&) {
                            // 图片添加失败时静默跳过
                        }

                        delete tempFile;
                    }
                    continue;
                }

                // 合并连续同格式的文本片段：格式不变则追加文本，否则新建 Run
                if (currentRun.text.empty()) {
                    currentRun.style = extractTextStyle(fragFmt);
                    currentRun.text = fragment.text().toStdString();
                    prevFmt = fragFmt;
                } else if (fragFmt == prevFmt) {
                    currentRun.text += fragment.text().toStdString();
                } else {
                    if (!currentRun.text.empty() || !currentRun.images.empty()) {
                        para.runs.push_back(std::move(currentRun));
                    }
                    currentRun = cppwordkit::Run{};
                    currentRun.style = extractTextStyle(fragFmt);
                    currentRun.text = fragment.text().toStdString();
                    prevFmt = fragFmt;
                }
            }

            // 提交最后一个 Run
            if (!currentRun.text.empty() || !currentRun.images.empty()) {
                para.runs.push_back(std::move(currentRun));
            }

            // 空段落至少需要一个空的 Run，以保证段落结构完整
            if (para.runs.empty()) {
                cppwordkit::Run emptyRun;
                emptyRun.text = "";
                para.runs.push_back(std::move(emptyRun));
            }

            model.paragraphs.push_back(std::move(para));
        }
    }

    return model;
}

/**
 * 从 QTextCharFormat 提取 cppwordkit 字体样式
 * 与 mapTextStyle 互为逆操作
 */
cppwordkit::TextStyle QtDocumentMapper::extractTextStyle(const QTextCharFormat& fmt) const
{
    cppwordkit::TextStyle style;

    if (fmt.hasProperty(QTextFormat::FontFamilies)) {
        const QStringList families = fmt.property(QTextFormat::FontFamilies).toStringList();
        if (!families.isEmpty()) {
            style.fontFamily = families.first().toStdString();
        }
    }

    if (fmt.hasProperty(QTextFormat::FontPointSize)) {
        style.fontSizeHalfPoints = fontSizeToHalfPoints(fmt.fontPointSize());
    }

    if (fmt.hasProperty(QTextFormat::FontWeight)) {
        style.bold = (fmt.fontWeight() >= QFont::Bold);
    }

    if (fmt.hasProperty(QTextFormat::FontItalic)) {
        style.italic = fmt.fontItalic();
    }

    if (fmt.hasProperty(QTextFormat::TextUnderlineStyle)) {
        style.underline = (fmt.underlineStyle() != QTextCharFormat::NoUnderline);
    }

    if (fmt.hasProperty(QTextFormat::FontStrikeOut)) {
        style.strike = fmt.fontStrikeOut();
    }

    if (fmt.hasProperty(QTextFormat::TextVerticalAlignment)) {
        auto va = fmt.verticalAlignment();
        if (va == QTextCharFormat::AlignSuperScript) {
            style.superscript = true;
        } else if (va == QTextCharFormat::AlignSubScript) {
            style.subscript = true;
        }
    }

    if (fmt.hasProperty(QTextFormat::ForegroundBrush)) {
        QColor color = fmt.foreground().color();
        if (color.isValid()) {
            style.colorHex = color.name().mid(1).toStdString();
        }
    }

    if (fmt.hasProperty(QTextFormat::BackgroundBrush)) {
        QColor bg = fmt.background().color();
        if (bg.isValid() && bg.alpha() > 0) {
            style.backgroundColorHex = bg.name().mid(1).toStdString();
        }
    }

    return style;
}

/**
 * 从 QTextBlockFormat 提取 cppwordkit 段落样式
 * 与 mapParagraphStyle 互为逆操作，points -> twips 换算
 */
cppwordkit::ParagraphStyle QtDocumentMapper::extractParagraphStyle(const QTextBlockFormat& fmt) const
{
    cppwordkit::ParagraphStyle style;
    style.alignment = extractAlignment(fmt.alignment());

    if (fmt.hasProperty(QTextFormat::BlockTopMargin)) {
        style.spacingBeforeTwips = static_cast<std::int32_t>(fmt.topMargin() * 20.0);
    }
    if (fmt.hasProperty(QTextFormat::BlockBottomMargin)) {
        style.spacingAfterTwips = static_cast<std::int32_t>(fmt.bottomMargin() * 20.0);
    }
    if (fmt.hasProperty(QTextFormat::BlockLeftMargin)) {
        style.leftIndentTwips = static_cast<std::int32_t>(fmt.leftMargin() * 20.0);
    }
    if (fmt.hasProperty(QTextFormat::BlockRightMargin)) {
        style.rightIndentTwips = static_cast<std::int32_t>(fmt.rightMargin() * 20.0);
    }
    if (fmt.hasProperty(QTextFormat::TextIndent)) {
        style.firstLineIndentTwips = static_cast<std::int32_t>(fmt.textIndent() * 20.0);
    }

    return style;
}

cppwordkit::ParagraphAlignment QtDocumentMapper::extractAlignment(Qt::Alignment alignment) const
{
    if (alignment & Qt::AlignHCenter)  return cppwordkit::ParagraphAlignment::Center;
    if (alignment & Qt::AlignRight)    return cppwordkit::ParagraphAlignment::Right;
    if (alignment & Qt::AlignJustify)  return cppwordkit::ParagraphAlignment::Justify;
    return cppwordkit::ParagraphAlignment::Left;
}
