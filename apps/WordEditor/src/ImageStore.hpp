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

class ImageStore : public QObject {
    Q_OBJECT
public:
    explicit ImageStore(QObject* parent = nullptr);

    QString addImage(const QByteArray& imageData, const QString& format);
    QString addImageFromPath(const QString& filePath);
    QByteArray imageData(const QString& resourceName) const;

    void addImagesToDocument(QTextDocument& doc);
    void clear();

    struct ImageEntry {
        QString resourceName;
        QByteArray rawData;
        QString format;
        cppwordkit::ImageId wordImageId;
        bool addedToPackage = false;
    };

    const std::vector<ImageEntry>& entries() const;

    void markAddedToPackage(const QString& resourceName, const cppwordkit::ImageId& imageId);

private:
    std::vector<ImageEntry> entries_;
    int counter_ = 0;
    std::vector<QTemporaryFile*> tempFiles_;
};
