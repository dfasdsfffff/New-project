#pragma once

#include <QMainWindow>
#include <QTextEdit>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>

#include "DocumentController.hpp"
#include "QtDocumentMapper.hpp"
#include "ImageStore.hpp"
#include "FormatToolbar.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onNewDocument();
    void onOpenDocument();
    void onSaveDocument();
    void onSaveDocumentAs();
    void onExportPdf();
    void onInsertImage();
    void onInsertTable();
    void onEditorModified();
    void updateStatusFromController();

private:
    void setupMenus();
    void setupToolbars();
    void setupStatusBar();
    void setupEditor();
    void connectSignals();
    void updateTitle();

    void doImport();
    void doExport();

    QTextEdit* editor_;
    FormatToolbar* formatToolbar_;
    DocumentController* controller_;
    ImageStore* imageStore_;
    QtDocumentMapper* mapper_;

    QLabel* filePathLabel_;
    QLabel* modifiedLabel_;
    QLabel* messageLabel_;

    QMenu* fileMenu_;
    QMenu* editMenu_;
    QMenu* insertMenu_;

    QAction* newAction_;
    QAction* openAction_;
    QAction* saveAction_;
    QAction* saveAsAction_;
    QAction* exportPdfAction_;
    QAction* exitAction_;

    QAction* undoAction_;
    QAction* redoAction_;
    QAction* cutAction_;
    QAction* copyAction_;
    QAction* pasteAction_;
    QAction* selectAllAction_;

    QAction* insertImageAction_;
    QAction* insertTableAction_;
};
