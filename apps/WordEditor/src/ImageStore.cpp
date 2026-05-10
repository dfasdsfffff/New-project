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
