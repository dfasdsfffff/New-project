#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <cppwordkit/CppWordKit.hpp>

class DocumentController : public QObject {
    Q_OBJECT
public:
    explicit DocumentController(QObject* parent = nullptr);

    bool newDocument();
    bool openDocument(const QString& path);
    bool saveDocument();
    bool saveDocumentAs(const QString& path);
    bool exportPdf(const QString& path);

    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] bool isModified() const;
    [[nodiscard]] QString currentPath() const;
    [[nodiscard]] QString currentFileName() const;

    cppwordkit::WordDocument* wordDocument();
    const cppwordkit::WordDocument* wordDocument() const;

    void setModified(bool modified);

signals:
    void modifiedChanged(bool modified);
    void pathChanged(const QString& path);
    void statusMessage(const QString& message);
    void errorOccurred(const QString& message);

private:
    std::unique_ptr<cppwordkit::WordDocument> document_;
    QString currentPath_;
    bool modified_ = false;
};
