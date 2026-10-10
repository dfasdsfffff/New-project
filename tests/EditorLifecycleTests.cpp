//! 编辑器生命周期回归：保存点、关闭保护、失败保留和页面重置。
#include "MainWindow.hpp"
#include "DocumentController.hpp"
#include "PageView.hpp"
#include "ImageStore.hpp"
#include "QtDocumentMapper.hpp"

#include <QApplication>
#include <QAbstractButton>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message)
{
    // Release 构建也必须执行检查，不能依赖 assert。
    if (!value) throw std::runtime_error(message);
}

void answerMessage(QMessageBox::StandardButton answer)
{
    QTimer::singleShot(0, [answer] {
        for (auto* widget : QApplication::topLevelWidgets()) {
            if (auto* box = qobject_cast<QMessageBox*>(widget); box && box->isVisible()) {
                if (auto* button = box->button(answer)) button->click();
                return;
            }
        }
    });
}
}

int main(int argc, char** argv)
{
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    try {
        QTemporaryDir directory;
        require(directory.isValid(), "temporary directory unavailable");
        // 合并回归：保留 master 的取消高亮、空边框过滤和独立底纹属性。
        ImageStore formatImages;
        QtDocumentMapper formatMapper(formatImages);
        QTextDocument formatDocument;
        QTextCharFormat format;
        format.setBackground(QColor("#ffff00"));
        format.setProperty(QTextFormat::UserProperty + 101, "none");
        format.setProperty(QTextFormat::UserProperty + 102, "");
        format.setProperty(QTextFormat::UserProperty + 104, "ddeeff");
        QTextCursor(&formatDocument).insertText("format", format);
        auto formatWord = cppwordkit::WordDocument::create();
        const auto formatModel = formatMapper.exportModel(formatDocument, formatWord);
        const auto& exportedStyle = formatModel.paragraphs.at(0).runs.at(0).style;
        require(!exportedStyle.highlightColor && !exportedStyle.borderColorHex,
                "cleared highlight or border exported as active style");
        require(exportedStyle.backgroundColorHex == "ddeeff", "shading overwritten by highlight");
        // 独立控制器验证中文及非本地代码页字符的路径往返。
        DocumentController unicodeController;
        require(unicodeController.newDocument(), "unicode fixture creation failed");
        const auto unicodePath = directory.filePath(QString::fromUtf8("中文报告-\xF0\x9F\x93\x84.docx"));
        require(unicodeController.saveDocumentAs(unicodePath), "unicode path save failed");
        require(QFile::exists(unicodePath), "unicode save used wrong filename");
        require(unicodeController.openDocument(unicodePath), "unicode path open failed");
        require(unicodeController.saveDocument(), "unicode path overwrite failed");
        MainWindow window;
        auto* controller = window.findChild<DocumentController*>();
        auto* editor = window.findChild<PageView*>();
        require(controller && editor, "editor components unavailable");
        const auto path = directory.filePath("saved.docx");
        require(controller->saveDocumentAs(path), "initial save failed");

        editor->textCursor().insertText("first");
        require(controller->isModified(), "initial edit not tracked");
        require(QMetaObject::invokeMethod(&window, "onSaveDocument"), "save slot unavailable");
        require(!controller->isModified() && !editor->document()->isModified(), "save point not reset");
        editor->textCursor().insertText("second");
        require(controller->isModified(), "edit after save not tracked");

        window.show();
        answerMessage(QMessageBox::Cancel);
        require(!window.close(), "cancel must prevent close");
        require(controller->isModified(), "cancel discarded edits");
        answerMessage(QMessageBox::Save);
        require(window.close(), "successful save must allow close");
        require(!editor->document()->isModified(), "close-save did not reset save point");
        auto saved = cppwordkit::WordDocument::open(cppwordkit::Path(path.toStdU16String()));
        require(saved.text().find("second") != std::string::npos, "saved content missing");

        // 图片资源缺失时必须阻止保存，不能静默丢图并标记保存成功。
        QFile originalFile(path);
        require(originalFile.open(QIODevice::ReadOnly), "cannot read original document");
        const auto originalBytes = originalFile.readAll();
        originalFile.close();
        editor->textCursor().insertImage("missing-image-resource");
        answerMessage(QMessageBox::Ok);
        QMetaObject::invokeMethod(&window, "onSaveDocument");
        require(controller->isModified(), "missing image save cleared modification flag");
        require(originalFile.open(QIODevice::ReadOnly), "cannot reread original document");
        require(originalFile.readAll() == originalBytes, "missing image save changed disk content");
        originalFile.close();
        editor->undo();

        // 目标变成目录时保存必须失败，编辑内容和修改标记必须保留。
        require(QFile::remove(path) && QDir().mkdir(path), "failed-save fixture unavailable");
        editor->textCursor().insertText("unsaved");
        answerMessage(QMessageBox::Ok);
        QMetaObject::invokeMethod(&window, "onSaveDocument");
        require(controller->isModified() && editor->document()->isModified(), "failed save cleared edits");
        window.show();
        answerMessage(QMessageBox::Discard);
        require(window.close(), "discard must allow close");

        editor->setPageSize(PageSizes::A3());
        editor->setDocumentMargins(10, 20, 30, 40);
        answerMessage(QMessageBox::Discard);
        QMetaObject::invokeMethod(&window, "onNewDocument");
        require(editor->toPlainText().isEmpty(), "new document kept old text");
        require(qAbs(editor->pageSize().width() - PageSizes::A4().width()) < 0.1,
                "new document kept old page size");
        require(editor->pageMarginLeft() == 72 && editor->pageMarginTop() == 72 &&
                editor->pageMarginRight() == 72 && editor->pageMarginBottom() == 72,
                "new document kept old margins");
        require(!controller->isModified(), "new document starts dirty");

        // 新文档退出时选择保存，但取消路径选择，不能丢失当前内容。
        editor->textCursor().insertText("keep after cancel");
        QTimer dialogResponder;
        QObject::connect(&dialogResponder, &QTimer::timeout, [] {
            for (auto* widget : QApplication::topLevelWidgets()) {
                if (!widget->isVisible()) continue;
                if (auto* box = qobject_cast<QMessageBox*>(widget)) {
                    if (auto* button = box->button(QMessageBox::Save)) button->click();
                } else if (auto* dialog = qobject_cast<QFileDialog*>(widget)) {
                    dialog->reject();
                }
            }
        });
        dialogResponder.start(10);
        window.show();
        require(!window.close(), "cancelled save-as must prevent close");
        dialogResponder.stop();
        require(controller->isModified() && editor->toPlainText() == "keep after cancel",
                "cancelled save-as lost edits");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
