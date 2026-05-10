//! @file ImageStore.cpp 图片资源管理器实现

#include "ImageStore.hpp"

#include <QTextDocument>
#include <QImage>
#include <QPixmap>
#include <QFile>
#include <QFileInfo>
#include <QBuffer>
#include <QUrl>

ImageStore::ImageStore(QObject* parent)
    : QObject(parent)
{
}

QString ImageStore::addImage(const QByteArray& imageData, const QString& format)
{
    counter_++;
    // 使用自增计数器生成唯一的资源名称，避免多张图片冲突
    QString resourceName = QString("cppwordkit_image_%1.%2").arg(counter_).arg(format);

    ImageEntry entry;
    entry.resourceName = resourceName;
    entry.rawData = imageData;
    entry.format = format;
    entries_.push_back(std::move(entry));

    return resourceName;
}

QString ImageStore::addImageFromPath(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QByteArray data = file.readAll();
    file.close();

    QFileInfo fi(filePath);
    QString suffix = fi.suffix().toLower();
    if (suffix.isEmpty()) suffix = "png";

    return addImage(data, suffix);
}

QByteArray ImageStore::imageData(const QString& resourceName) const
{
    for (const auto& entry : entries_) {
        if (entry.resourceName == resourceName) {
            return entry.rawData;
        }
    }
    return {};
}

void ImageStore::addImagesToDocument(QTextDocument& doc)
{
    // 将存储的图片数据解码为 QImage 并注册为 QTextDocument 的 ImageResource
    // 这是 QTextEdit 能渲染内嵌图片的关键步骤
    for (const auto& entry : entries_) {
        QImage image;
        if (image.loadFromData(entry.rawData)) {
            doc.addResource(QTextDocument::ImageResource,
                           QUrl(entry.resourceName),
                           QVariant::fromValue(image));
        }
    }
}

void ImageStore::clear()
{
    entries_.clear();
    counter_ = 0;

    // 清理导出时创建的临时文件
    for (auto* tmp : tempFiles_) {
        delete tmp;
    }
    tempFiles_.clear();
}

const std::vector<ImageStore::ImageEntry>& ImageStore::entries() const
{
    return entries_;
}

void ImageStore::markAddedToPackage(const QString& resourceName, const cppwordkit::ImageId& imageId)
{
    for (auto& entry : entries_) {
        if (entry.resourceName == resourceName) {
            entry.wordImageId = imageId;
            entry.addedToPackage = true;
            return;
        }
    }
}
