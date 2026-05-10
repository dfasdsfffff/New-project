//! @file MainWindow.cpp 应用程序主窗口实现

#include "MainWindow.hpp"

#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QPrinter>
#include <QPageSize>
#include <QTextTable>
#include <QTextCursor>
#include <QTextBlock>
#include <QClipboard>
#include <QFileInfo>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , editor_(new QTextEdit(this))
    , formatToolbar_(new FormatToolbar(this))
    , controller_(new DocumentController(this))
    , imageStore_(new ImageStore(this))
    , mapper_(new QtDocumentMapper(*imageStore_))
{
    setupMenus();
    setupToolbars();
    setupStatusBar();
    setupEditor();
    connectSignals();
    updateTitle();

    // 初始化时创建一个空白文档并导入到编辑器
    controller_->newDocument();
    doImport();
}

void MainWindow::setupMenus()
{
    // 文件菜单
    fileMenu_ = menuBar()->addMenu("文件(&F)");

    newAction_ = fileMenu_->addAction("新建(&N)");
    newAction_->setShortcut(QKeySequence::New);
    connect(newAction_, &QAction::triggered, this, &MainWindow::onNewDocument);

    openAction_ = fileMenu_->addAction("打开(&O)...");
    openAction_->setShortcut(QKeySequence::Open);
    connect(openAction_, &QAction::triggered, this, &MainWindow::onOpenDocument);

    fileMenu_->addSeparator();

    saveAction_ = fileMenu_->addAction("保存(&S)");
    saveAction_->setShortcut(QKeySequence::Save);
    connect(saveAction_, &QAction::triggered, this, &MainWindow::onSaveDocument);

    saveAsAction_ = fileMenu_->addAction("另存为(&A)...");
    saveAsAction_->setShortcut(QKeySequence("Ctrl+Shift+S"));
    connect(saveAsAction_, &QAction::triggered, this, &MainWindow::onSaveDocumentAs);

    fileMenu_->addSeparator();

    exportPdfAction_ = fileMenu_->addAction("导出 PDF(&E)...");
    connect(exportPdfAction_, &QAction::triggered, this, &MainWindow::onExportPdf);

    fileMenu_->addSeparator();

    exitAction_ = fileMenu_->addAction("退出(&X)");
    exitAction_->setShortcut(QKeySequence("Ctrl+Q"));
    connect(exitAction_, &QAction::triggered, this, &QWidget::close);

    // 编辑菜单
    editMenu_ = menuBar()->addMenu("编辑(&E)");

    undoAction_ = editMenu_->addAction("撤销(&U)");
    undoAction_->setShortcut(QKeySequence::Undo);
    connect(undoAction_, &QAction::triggered, editor_, &QTextEdit::undo);

    redoAction_ = editMenu_->addAction("重做(&R)");
    redoAction_->setShortcut(QKeySequence::Redo);
    connect(redoAction_, &QAction::triggered, editor_, &QTextEdit::redo);

    editMenu_->addSeparator();

    cutAction_ = editMenu_->addAction("剪切(&T)");
    cutAction_->setShortcut(QKeySequence::Cut);
    connect(cutAction_, &QAction::triggered, editor_, &QTextEdit::cut);

    copyAction_ = editMenu_->addAction("复制(&C)");
    copyAction_->setShortcut(QKeySequence::Copy);
    connect(copyAction_, &QAction::triggered, editor_, &QTextEdit::copy);

    pasteAction_ = editMenu_->addAction("粘贴(&P)");
    pasteAction_->setShortcut(QKeySequence::Paste);
    connect(pasteAction_, &QAction::triggered, editor_, &QTextEdit::paste);

    editMenu_->addSeparator();

    selectAllAction_ = editMenu_->addAction("全选(&A)");
    selectAllAction_->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction_, &QAction::triggered, editor_, &QTextEdit::selectAll);

    // 插入菜单
    insertMenu_ = menuBar()->addMenu("插入(&I)");

    insertImageAction_ = insertMenu_->addAction("图片(&P)...");
    connect(insertImageAction_, &QAction::triggered, this, &MainWindow::onInsertImage);

    insertTableAction_ = insertMenu_->addAction("表格(&T)...");
    connect(insertTableAction_, &QAction::triggered, this, &MainWindow::onInsertTable);
}

void MainWindow::setupToolbars()
{
    // 将格式工具栏与编辑器关联并添加到主窗口
    formatToolbar_->setEditor(editor_);
    addToolBar(formatToolbar_);
}

void MainWindow::setupStatusBar()
{
    // 状态栏：按比例分配空间，显示文件路径、修改状态和操作提示
    filePathLabel_ = new QLabel("未命名文档");
    modifiedLabel_ = new QLabel("");
    messageLabel_ = new QLabel("");

    statusBar()->addWidget(filePathLabel_, 2);
    statusBar()->addWidget(modifiedLabel_, 1);
    statusBar()->addWidget(messageLabel_, 3);
}

void MainWindow::setupEditor()
{
    setCentralWidget(editor_);
    editor_->setAcceptRichText(true);
    // 使用等线字体作为默认字体，12 号字适合中文文档编辑
    editor_->document()->setDefaultFont(QFont("等线", 12));
}

void MainWindow::connectSignals()
{
    // 监听编辑器文档的修改状态变化，同步到控制器
    connect(editor_->document(), &QTextDocument::modificationChanged,
            this, &MainWindow::onEditorModified);

    // 控制器修改状态变化时更新状态栏和标题栏
    connect(controller_, &DocumentController::modifiedChanged, this, [this](bool modified) {
        modifiedLabel_->setText(modified ? "● 已修改" : "  已保存");
        updateTitle();
    });

    // 控制器文件路径变化时同步更新状态栏
    connect(controller_, &DocumentController::pathChanged, this, [this](const QString& path) {
        if (path.isEmpty()) {
            filePathLabel_->setText("未命名文档");
        } else {
            filePathLabel_->setText(path);
        }
        updateTitle();
    });

    // 控制器的状态消息显示到状态栏
    connect(controller_, &DocumentController::statusMessage, this, [this](const QString& msg) {
        messageLabel_->setText(msg);
    });

    // 控制器的错误信号：状态栏显示错误消息并弹出警告对话框
    connect(controller_, &DocumentController::errorOccurred, this, [this](const QString& msg) {
        messageLabel_->setText("错误: " + msg);
        QMessageBox::warning(this, "错误", msg);
    });
}

void MainWindow::onNewDocument()
{
    // 如果当前文档已修改，提示用户保存
    if (controller_->isModified()) {
        auto result = QMessageBox::question(this, "新建文档",
            "当前文档已修改，是否保存？",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (result == QMessageBox::Cancel) return;
        if (result == QMessageBox::Save) {
            onSaveDocument();
            if (controller_->isModified()) return;
        }
    }

    // 创建新文档后清空图片缓存和编辑器内容
    if (controller_->newDocument()) {
        imageStore_->clear();
        editor_->clear();
        editor_->document()->setModified(false);
    }
}

void MainWindow::onOpenDocument()
{
    // 如果当前文档已修改，提示用户保存
    if (controller_->isModified()) {
        auto result = QMessageBox::question(this, "打开文档",
            "当前文档已修改，是否保存？",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (result == QMessageBox::Cancel) return;
        if (result == QMessageBox::Save) {
            onSaveDocument();
            if (controller_->isModified()) return;
        }
    }

    QString path = QFileDialog::getOpenFileName(this, "打开文档", {},
        "Word 文档 (*.docx);;所有文件 (*)");
    if (path.isEmpty()) return;

    if (controller_->openDocument(path)) {
        doImport();
        editor_->document()->setModified(false);
    }
}

void MainWindow::onSaveDocument()
{
    // 保存前先导出编辑器内容到模型，确保数据一致性
    doExport();

    // 若文档尚未保存过（无路径），则转交另存为流程
    if (controller_->currentPath().isEmpty()) {
        onSaveDocumentAs();
        return;
    }

    controller_->saveDocument();
}

void MainWindow::onSaveDocumentAs()
{
    doExport();

    QString path = QFileDialog::getSaveFileName(this, "另存为", {},
        "Word 文档 (*.docx)");
    if (path.isEmpty()) return;

    // 自动补全 .docx 扩展名
    if (!path.endsWith(".docx", Qt::CaseInsensitive)) {
        path += ".docx";
    }

    controller_->saveDocumentAs(path);
}

void MainWindow::onExportPdf()
{
    QString path = QFileDialog::getSaveFileName(this, "导出 PDF", {},
        "PDF 文件 (*.pdf)");
    if (path.isEmpty()) return;

    // 自动补全 .pdf 扩展名
    if (!path.endsWith(".pdf", Qt::CaseInsensitive)) {
        path += ".pdf";
    }

    // 使用 QPrinter 将编辑器内容渲染为 PDF，A4 页面
    QPrinter printer;
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageSize(QPageSize(QPageSize::A4));

    editor_->document()->print(&printer);

    controller_->exportPdf(path);
    messageLabel_->setText("已导出 PDF: " + path);
}

void MainWindow::onInsertImage()
{
    if (!controller_->isOpen()) return;

    // 选择图片文件，支持常见图片格式
    QStringList filters;
    filters << "图片文件 (*.png *.jpg *.jpeg *.gif *.bmp)"
            << "PNG (*.png)"
            << "JPEG (*.jpg *.jpeg)"
            << "GIF (*.gif)"
            << "BMP (*.bmp)"
            << "所有文件 (*)";

    QString filePath = QFileDialog::getOpenFileName(this, "插入图片", {},
        filters.join(";;"));
    if (filePath.isEmpty()) return;

    // 读取原始图片数据
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "插入图片", "无法打开图片文件");
        return;
    }

    QByteArray imgData = file.readAll();
    file.close();

    // 提取扩展名以确定图片格式
    QFileInfo fi(filePath);
    QString suffix = fi.suffix().toLower();
    if (suffix.isEmpty()) suffix = "png";

    QString resourceName = imageStore_->addImage(imgData, suffix);
    if (resourceName.isEmpty()) {
        QMessageBox::warning(this, "插入图片", "无法添加图片");
        return;
    }

    // 将图片作为 QTextDocument 的 ImageResource 注册，以使 QTextEdit 可渲染
    imageStore_->addImagesToDocument(*editor_->document());

    // 根据图片原始尺寸设置显示尺寸，宽度超过编辑区 80% 时等比缩放
    QTextImageFormat imgFmt;
    imgFmt.setName(resourceName);

    QImage image;
    if (image.loadFromData(imgData)) {
        qreal width = image.width();
        qreal height = image.height();
        qreal maxWidth = editor_->viewport()->width() * 0.8;
        if (width > maxWidth) {
            qreal ratio = maxWidth / width;
            width = maxWidth;
            height *= ratio;
        }
        imgFmt.setWidth(width);
        imgFmt.setHeight(height);
    } else {
        // 图片无法解码时使用默认占位尺寸
        imgFmt.setWidth(200);
        imgFmt.setHeight(150);
    }

    // 在光标位置插入图片
    QTextCursor cursor = editor_->textCursor();
    cursor.insertImage(imgFmt);
    editor_->setTextCursor(cursor);

    messageLabel_->setText("已插入图片");
}

void MainWindow::onInsertTable()
{
    if (!controller_->isOpen()) return;

    // 通过对话框输入表格的行数和列数
    bool ok = false;
    int rows = QInputDialog::getInt(this, "插入表格", "行数:", 3, 1, 100, 1, &ok);
    if (!ok) return;

    int cols = QInputDialog::getInt(this, "插入表格", "列数:", 3, 1, 20, 1, &ok);
    if (!ok) return;

    QTextTableFormat tableFmt;
    tableFmt.setBorder(1);
    tableFmt.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
    tableFmt.setCellPadding(4);
    tableFmt.setCellSpacing(0);

    QTextCursor cursor = editor_->textCursor();
    cursor.insertTable(rows, cols, tableFmt);

    editor_->setTextCursor(cursor);
    messageLabel_->setText(QString("已插入 %1 x %2 表格").arg(rows).arg(cols));
}

void MainWindow::onEditorModified()
{
    // 将 QTextEdit 的修改状态同步到控制器
    bool modified = editor_->document()->isModified();
    controller_->setModified(modified);
}

void MainWindow::updateStatusFromController()
{
    updateTitle();
}

void MainWindow::updateTitle()
{
    // 标题格式：文件名 [+ 修改标记] - 应用程序名
    QString title = controller_->currentFileName();
    if (controller_->isModified()) {
        title += " *";
    }
    title += " - CppWordKit Word Editor";
    setWindowTitle(title);
}

/**
 * 从控制器模型导入到 QTextDocument
 * 将 cppwordkit::DocumentModel 转换为 QTextDocument 内容供编辑器显示
 */
void MainWindow::doImport()
{
    if (!controller_->wordDocument()) return;

    auto model = controller_->wordDocument()->model();
    mapper_->importModel(*editor_->document(), model, *controller_->wordDocument());
    editor_->document()->setModified(false);
}

/**
 * 从 QTextDocument 导出回控制器模型
 * 将编辑器内容同步为 cppwordkit::DocumentModel，再写回 WordDocument
 */
void MainWindow::doExport()
{
    if (!controller_->wordDocument()) return;

    auto model = mapper_->exportModel(*editor_->document(), *controller_->wordDocument());
    controller_->wordDocument()->replaceBody(model);
}
