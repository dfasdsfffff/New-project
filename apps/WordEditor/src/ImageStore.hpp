//! @file ImageStore.hpp 图片资源管理器，暂存文档内嵌图片数据

#pragma once

#include <QObject>
#include <QImage>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QTemporaryFile>
#include <QDir>

#include <unordered_map>
#include <string>
#include <vector>

#include <cppwordkit/CppWordKit.hpp>

class QTextDocument;

/**
 * @brief 图片资源管理器
 *
 * 在文档编辑过程中暂存所有内嵌图片的原始数据（未写入包文件前），
 * 并提供将图片注册到 QTextDocument 资源系统中的能力。
 * 导入时从 docx 包提取图片数据存于此，导出时从此处读取并写入新的包文件。
 */
class ImageStore : public QObject {
    Q_OBJECT
public:
    explicit ImageStore(QObject* parent = nullptr);

    /// 添加图片数据，返回生成的资源名称
    QString addImage(const QByteArray& imageData, const QString& format);
    /// 从文件路径添加图片
    QString addImageFromPath(const QString& filePath);
    /// 根据资源名称查询原始图片数据
    QByteArray imageData(const QString& resourceName) const;

    /// 将所有已存储的图片注册为 QTextDocument 的资源，供 QTextEdit 渲染
    void addImagesToDocument(QTextDocument& doc);
    void clear();

    /// 单张图片的存储条目
    struct ImageEntry {
        QString resourceName;               ///< 资源名称，用于 QTextDocument 中引用
        QByteArray rawData;                 ///< 原始图片二进制数据
        QString format;                     ///< 图片格式（png, jpg, gif 等）
        cppwordkit::ImageId wordImageId;    ///< 导出后对应的 cppwordkit 图片 ID
        bool addedToPackage = false;        ///< 是否已写入 Word 包文件
    };

    const std::vector<ImageEntry>& entries() const;

    /// 标记某图片已写入包文件，记录其 ImageId
    void markAddedToPackage(const QString& resourceName, const cppwordkit::ImageId& imageId);

private:
    std::vector<ImageEntry> entries_;   ///< 所有图片条目
    int counter_ = 0;                   ///< 自增计数器，用于生成唯一资源名称
    std::vector<QTemporaryFile*> tempFiles_;  ///< 导出时创建的临时文件列表
};
