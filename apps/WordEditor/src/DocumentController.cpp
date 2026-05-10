#include "DocumentController.hpp"
#include "ImageStore.hpp"
#include "QtDocumentMapper.hpp"

#include <QFileInfo>
#include <QTextDocument>

#include <cppwordkit/Error.hpp>

DocumentController::DocumentController(QObject* parent)
    : QObject(parent)
{
}

bool DocumentController::newDocument()
{
    try {
        document_ = std::make_unique<cppwordkit::WordDocument>(
            cppwordkit::WordDocument::create());
        currentPath_.clear();
        modified_ = false;

        emit pathChanged({});
        emit modifiedChanged(false);
        emit statusMessage("新建文档");
        return true;
    } catch (const std::exception& e) {
        emit errorOccurred(QString::fromStdString(e.what()));
        return false;
    }
}

bool DocumentController::openDocument(const QString& path)
{
    try {
        auto doc = cppwordkit::WordDocument::open(path.toStdString());
        document_ = std::make_unique<cppwordkit::WordDocument>(std::move(doc));
        currentPath_ = path;
        modified_ = false;

        emit pathChanged(currentPath_);
        emit modifiedChanged(false);
        emit statusMessage("已打开: " + QFileInfo(path).fileName());
        return true;
    } catch (const cppwordkit::PackageException& e) {
        emit errorOccurred("无法打开文件，文件可能已损坏或不是有效的 docx 文档:\n" + QString::fromStdString(e.what()));
        return false;
    } catch (const std::exception& e) {
        emit errorOccurred("打开文件失败:\n" + QString::fromStdString(e.what()));
        return false;
    }
}

bool DocumentController::saveDocument()
{
    if (!document_ || !document_->isOpen()) {
        emit errorOccurred("没有打开的文档");
        return false;
    }

    if (currentPath_.isEmpty()) {
        emit statusMessage("请选择保存路径");
        return false;
    }

    try {
        document_->save();
        modified_ = false;
        emit modifiedChanged(false);
        emit statusMessage("已保存: " + QFileInfo(currentPath_).fileName());
        return true;
    } catch (const std::exception& e) {
        emit errorOccurred("保存失败:\n" + QString::fromStdString(e.what()));
        return false;
    }
}

bool DocumentController::saveDocumentAs(const QString& path)
{
    if (!document_ || !document_->isOpen()) {
        emit errorOccurred("没有打开的文档");
        return false;
    }

    try {
        document_->saveAs(path.toStdString());
        currentPath_ = path;
        modified_ = false;

        emit pathChanged(currentPath_);
        emit modifiedChanged(false);
        emit statusMessage("已保存: " + QFileInfo(path).fileName());
        return true;
    } catch (const std::exception& e) {
        emit errorOccurred("另存为失败:\n" + QString::fromStdString(e.what()));
        return false;
    }
}

bool DocumentController::exportPdf(const QString& /*path*/)
{
    if (!document_ || !document_->isOpen()) {
        emit errorOccurred("没有打开的文档");
        return false;
    }

    emit statusMessage("导出 PDF 需要通过编辑器内容生成");
    return true;
}

bool DocumentController::isOpen() const
{
    return document_ && document_->isOpen();
}

bool DocumentController::isModified() const
{
    return modified_;
}

QString DocumentController::currentPath() const
{
    return currentPath_;
}

QString DocumentController::currentFileName() const
{
    if (currentPath_.isEmpty()) {
        return "未命名文档";
    }
    return QFileInfo(currentPath_).fileName();
}

cppwordkit::WordDocument* DocumentController::wordDocument()
{
    return document_.get();
}

const cppwordkit::WordDocument* DocumentController::wordDocument() const
{
    return document_.get();
}

void DocumentController::setModified(bool modified)
{
    if (modified_ != modified) {
        modified_ = modified;
        emit modifiedChanged(modified_);
    }
}
