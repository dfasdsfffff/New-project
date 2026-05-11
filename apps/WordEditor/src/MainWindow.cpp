//! @file MainWindow.cpp 应用程序主窗口实现

#include "MainWindow.hpp"
#include "PageView.hpp"
#include "DocumentController.hpp"
#include "QtDocumentMapper.hpp"
#include "ImageStore.hpp"
#include "PageSetupDialog.hpp"

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
#include <QActionGroup>
#include <QColorDialog>
#include <QFontDatabase>
#include <QStatusBar>
#include <QMenuBar>

namespace {

constexpr int HighlightColorProperty = QTextFormat::UserProperty + 101;
constexpr int BorderColorProperty = QTextFormat::UserProperty + 102;
constexpr int EnclosedCircleProperty = QTextFormat::UserProperty + 103;

QColor highlightColorToQt(const QString& value)
{
    if (value == "yellow") return QColor("#ffff00");
    if (value == "green") return QColor("#00ff00");
    if (value == "cyan") return QColor("#00ffff");
    if (value == "magenta") return QColor("#ff00ff");
    if (value == "blue") return QColor("#0000ff");
    if (value == "red") return QColor("#ff0000");
    if (value == "darkYellow") return QColor("#808000");
    if (value == "darkGreen") return QColor("#008000");
    if (value == "darkCyan") return QColor("#008080");
    if (value == "darkMagenta") return QColor("#800080");
    if (value == "darkBlue") return QColor("#000080");
    if (value == "darkRed") return QColor("#800000");
    if (value == "darkGray") return QColor("#808080");
    if (value == "lightGray") return QColor("#c0c0c0");
    if (value == "black") return QColor("#000000");
    if (value == "white") return QColor("#ffffff");
    return {};
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : SARibbonMainWindow(parent)
    , editor_(new PageView(this))
    , controller_(new DocumentController(this))
    , imageStore_(new ImageStore(this))
    , mapper_(new QtDocumentMapper(*imageStore_))
{
    setupMenus();
    setupRibbon();
    setupStatusBar();
    setupEditor();
    connectSignals();
    updateTitle();

    controller_->newDocument();
    doImport();
}

// ═══════════════════════════════════════════════════════════════
// 菜单
// ═══════════════════════════════════════════════════════════════

void MainWindow::setupMenus()
{
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

    editMenu_ = menuBar()->addMenu("编辑(&E)");

    undoAction_ = editMenu_->addAction("撤销(&U)");
    undoAction_->setShortcut(QKeySequence::Undo);
    connect(undoAction_, &QAction::triggered, editor_, &PageView::undo);

    redoAction_ = editMenu_->addAction("重做(&R)");
    redoAction_->setShortcut(QKeySequence::Redo);
    connect(redoAction_, &QAction::triggered, editor_, &PageView::redo);

    editMenu_->addSeparator();

    cutAction_ = editMenu_->addAction("剪切(&T)");
    cutAction_->setShortcut(QKeySequence::Cut);
    connect(cutAction_, &QAction::triggered, editor_, &PageView::cut);

    copyAction_ = editMenu_->addAction("复制(&C)");
    copyAction_->setShortcut(QKeySequence::Copy);
    connect(copyAction_, &QAction::triggered, editor_, &PageView::copy);

    pasteAction_ = editMenu_->addAction("粘贴(&P)");
    pasteAction_->setShortcut(QKeySequence::Paste);
    connect(pasteAction_, &QAction::triggered, editor_, &PageView::paste);

    editMenu_->addSeparator();

    selectAllAction_ = editMenu_->addAction("全选(&A)");
    selectAllAction_->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction_, &QAction::triggered, editor_, &PageView::selectAll);

    insertMenu_ = menuBar()->addMenu("插入(&I)");
    insertImageAction_ = insertMenu_->addAction("图片(&P)...");
    connect(insertImageAction_, &QAction::triggered, this, &MainWindow::onInsertImage);
    insertTableAction_ = insertMenu_->addAction("表格(&T)...");
    connect(insertTableAction_, &QAction::triggered, this, &MainWindow::onInsertTable);

    QMenu* formatMenu = menuBar()->addMenu("格式(&M)");
    pageSetupAction_ = formatMenu->addAction("页面设置(&U)...");
    connect(pageSetupAction_, &QAction::triggered, this, &MainWindow::onPageSetup);
}

// ═══════════════════════════════════════════════════════════════
// 功能区
// ═══════════════════════════════════════════════════════════════

void MainWindow::setupRibbon()
{
    applyRibbonStyle();

    // ── "开始" 选项卡 ──
    SARibbonCategory* homeCategory = ribbonBar()->addCategoryPage("开始");

    // -- 字体 panel --
    SARibbonPanel* fontPanel = homeCategory->addPanel("字体");
    fontPanel->setPanelLayoutMode(SARibbonPanel::ThreeRowMode);

    // 字体下拉框
    fontCombo_ = new QFontComboBox(this);
    fontCombo_->setMinimumWidth(126);
    fontCombo_->setMaximumHeight(24);
    fontCombo_->setWritingSystem(QFontDatabase::SimplifiedChinese);
    fontPanel->addMediumWidget(fontCombo_);

    // 字号下拉框
    fontSizeCombo_ = new QComboBox(this);
    fontSizeCombo_->setEditable(true);
    fontSizeCombo_->setMinimumWidth(54);
    fontSizeCombo_->setMaximumHeight(24);
    QStringList sizes;
    sizes << "8" << "9" << "10" << "10.5" << "11" << "12" << "14" << "16"
          << "18" << "20" << "22" << "24" << "26" << "28" << "36" << "48" << "72";
    fontSizeCombo_->addItems(sizes);
    fontSizeCombo_->setCurrentText("12");
    fontPanel->addSmallWidget(fontSizeCombo_);

    // 粗体
    boldAction_ = new QAction("B", this);
    boldAction_->setCheckable(true);
    boldAction_->setToolTip("加粗 (Ctrl+B)");
    QFont boldFont = boldAction_->font();
    boldFont.setBold(true);
    boldAction_->setFont(boldFont);
    fontPanel->addSmallAction(boldAction_);

    // 斜体
    italicAction_ = new QAction("I", this);
    italicAction_->setCheckable(true);
    italicAction_->setToolTip("斜体 (Ctrl+I)");
    QFont italicFont = italicAction_->font();
    italicFont.setItalic(true);
    italicAction_->setFont(italicFont);
    fontPanel->addSmallAction(italicAction_);

    // 下划线
    underlineAction_ = new QAction("U", this);
    underlineAction_->setCheckable(true);
    underlineAction_->setToolTip("下划线 (Ctrl+U)");
    QFont ulFont = underlineAction_->font();
    ulFont.setUnderline(true);
    underlineAction_->setFont(ulFont);
    fontPanel->addSmallAction(underlineAction_);

    // 删除线
    strikeAction_ = new QAction("S", this);
    strikeAction_->setCheckable(true);
    strikeAction_->setToolTip("删除线");
    QFont strikeFont = strikeAction_->font();
    strikeFont.setStrikeOut(true);
    strikeAction_->setFont(strikeFont);
    fontPanel->addSmallAction(strikeAction_);

    fontPanel->addSeparator();

    // 文字颜色
    textColorAction_ = new QAction("A", this);
    textColorAction_->setToolTip("文字颜色");
    QFont colorFont = textColorAction_->font();
    colorFont.setBold(true);
    textColorAction_->setFont(colorFont);
    fontPanel->addSmallAction(textColorAction_);

    // 字符底纹
    backgroundColorAction_ = new QAction("底", this);
    backgroundColorAction_->setToolTip("字符底纹");
    fontPanel->addSmallAction(backgroundColorAction_);

    // 高亮菜单
    highlightMenu_ = new QMenu("高亮显示", this);
    auto addHl = [this](const QString& name, const QString& val) {
        QAction* a = highlightMenu_->addAction(name);
        a->setData(val);
    };
    addHl("黄色", "yellow");
    addHl("绿色", "green");
    addHl("青色", "cyan");
    addHl("红色", "red");
    addHl("蓝色", "blue");
    addHl("无高亮", "none");
    fontPanel->addSmallMenu(highlightMenu_);

    // 字符边框
    borderAction_ = new QAction("□", this);
    borderAction_->setCheckable(true);
    borderAction_->setToolTip("字符边框");
    fontPanel->addSmallAction(borderAction_);

    // 带圈字符
    enclosedCircleAction_ = new QAction("○", this);
    enclosedCircleAction_->setCheckable(true);
    enclosedCircleAction_->setToolTip("带圈字符");
    fontPanel->addSmallAction(enclosedCircleAction_);

    // 大小写菜单
    changeCaseMenu_ = new QMenu("更改大小写", this);
    upperCaseAction_ = changeCaseMenu_->addAction("大写");
    lowerCaseAction_ = changeCaseMenu_->addAction("小写");
    titleCaseAction_ = changeCaseMenu_->addAction("首字母大写");
    fontPanel->addSmallMenu(changeCaseMenu_);

    // -- 段落 panel --
    SARibbonPanel* paraPanel = homeCategory->addPanel("段落");

    QActionGroup* alignGroup = new QActionGroup(this);
    alignGroup->setExclusive(true);

    alignLeftAction_ = new QAction("≡", this);
    alignLeftAction_->setCheckable(true);
    alignLeftAction_->setToolTip("左对齐");
    alignLeftAction_->setChecked(true);
    paraPanel->addSmallAction(alignLeftAction_);
    alignGroup->addAction(alignLeftAction_);

    alignCenterAction_ = new QAction("≡", this);
    alignCenterAction_->setCheckable(true);
    alignCenterAction_->setToolTip("居中");
    paraPanel->addSmallAction(alignCenterAction_);
    alignGroup->addAction(alignCenterAction_);

    alignRightAction_ = new QAction("≡", this);
    alignRightAction_->setCheckable(true);
    alignRightAction_->setToolTip("右对齐");
    paraPanel->addSmallAction(alignRightAction_);
    alignGroup->addAction(alignRightAction_);

    alignJustifyAction_ = new QAction("≡", this);
    alignJustifyAction_->setCheckable(true);
    alignJustifyAction_->setToolTip("两端对齐");
    paraPanel->addSmallAction(alignJustifyAction_);
    alignGroup->addAction(alignJustifyAction_);

    // ── "页面" 选项卡 ──
    SARibbonCategory* pageCategory = ribbonBar()->addCategoryPage("页面");

    SARibbonPanel* pageSetupPanel = pageCategory->addPanel("页面设置");
    QAction* pageSetupRibbonAction = new QAction("页面设置", this);
    pageSetupRibbonAction->setToolTip("页面尺寸和页边距");
    pageSetupPanel->addLargeAction(pageSetupRibbonAction);
    connect(pageSetupRibbonAction, &QAction::triggered, this, &MainWindow::onPageSetup);

    // ── 信号连接 ──
    connect(fontCombo_, &QFontComboBox::textActivated,
            this, &MainWindow::onFontFamilyChanged);
    connect(fontSizeCombo_, &QComboBox::textActivated,
            this, &MainWindow::onFontSizeChanged);
    connect(boldAction_, &QAction::toggled, this, &MainWindow::onBoldToggled);
    connect(italicAction_, &QAction::toggled, this, &MainWindow::onItalicToggled);
    connect(underlineAction_, &QAction::toggled, this, &MainWindow::onUnderlineToggled);
    connect(strikeAction_, &QAction::toggled, this, &MainWindow::onStrikeToggled);
    connect(textColorAction_, &QAction::triggered, this, &MainWindow::onTextColorClicked);
    connect(backgroundColorAction_, &QAction::triggered, this, &MainWindow::onBackgroundColorClicked);
    connect(highlightMenu_, &QMenu::triggered, this, &MainWindow::onHighlightSelected);
    connect(borderAction_, &QAction::toggled, this, &MainWindow::onBorderToggled);
    connect(enclosedCircleAction_, &QAction::toggled, this, &MainWindow::onEnclosedCircleToggled);
    connect(upperCaseAction_, &QAction::triggered, this, [this]() { onChangeCaseSelected(upperCaseAction_); });
    connect(lowerCaseAction_, &QAction::triggered, this, [this]() { onChangeCaseSelected(lowerCaseAction_); });
    connect(titleCaseAction_, &QAction::triggered, this, [this]() { onChangeCaseSelected(titleCaseAction_); });
    connect(alignLeftAction_, &QAction::triggered, this, [this]() { onAlignmentChanged(alignLeftAction_); });
    connect(alignCenterAction_, &QAction::triggered, this, [this]() { onAlignmentChanged(alignCenterAction_); });
    connect(alignRightAction_, &QAction::triggered, this, [this]() { onAlignmentChanged(alignRightAction_); });
    connect(alignJustifyAction_, &QAction::triggered, this, [this]() { onAlignmentChanged(alignJustifyAction_); });
}

void MainWindow::applyRibbonStyle()
{
    SARibbonBar* ribbon = ribbonBar();
    if (!ribbon) return;

    // WPS 风格更重视有效编辑面积：三行紧凑布局、较低标题栏和更密集的面板间距。
    ribbon->setRibbonStyle(SARibbonBar::RibbonStyleCompactThreeRow);
    ribbon->setTabBarHeight(28);
    ribbon->setCategoryHeight(92);
    ribbon->setPanelTitleHeight(18);
    ribbon->setPanelSpacing(1);
    ribbon->setEnableWordWrap(false);
    ribbon->setEnableIconRightText(false);
    ribbon->setButtonMaximumAspectRatio(1.18);
    ribbon->setTabBarBaseLineColor(QColor("#d6dde7"));
    ribbon->setWindowTitleTextColor(QColor("#1f2937"));

    ribbon->setStyleSheet(QStringLiteral(R"(
SARibbonBar {
    background: #f6f8fb;
    border-bottom: 1px solid #cfd7e3;
}
SARibbonTabBar {
    background: #f6f8fb;
    border: none;
}
SARibbonTabBar::tab {
    min-width: 58px;
    min-height: 24px;
    padding: 2px 14px;
    margin: 0;
    color: #1f2937;
    border: 1px solid transparent;
    border-bottom: none;
    background: transparent;
}
SARibbonTabBar::tab:hover:!selected {
    background: #e8f1ff;
    border-color: #c7daf7;
}
SARibbonTabBar::tab:selected {
    color: #0f5fd0;
    background: #ffffff;
    border-color: #cfd7e3;
    border-top: 2px solid #2f7ee6;
}
SARibbonCategory {
    background: #ffffff;
}
SARibbonPanel {
    background: #ffffff;
    border-right: 1px solid #e0e5ec;
    margin: 0;
}
SARibbonPanelLabel {
    color: #667085;
    font-size: 10px;
    padding-top: 1px;
}
SARibbonToolButton {
    color: #1f2937;
    background: transparent;
    border: 1px solid transparent;
    border-radius: 2px;
    padding: 2px 4px;
}
SARibbonToolButton:hover {
    background: #eef6ff;
    border-color: #b9d7ff;
}
SARibbonToolButton:pressed,
SARibbonToolButton:checked {
    background: #dbeafe;
    border-color: #7db5f4;
}
SARibbonToolButton:disabled {
    color: #a0a7b2;
}
QComboBox,
QFontComboBox {
    min-height: 22px;
    padding: 1px 22px 1px 6px;
    color: #1f2937;
    background: #ffffff;
    border: 1px solid #cfd7e3;
    border-radius: 2px;
}
QComboBox:hover,
QFontComboBox:hover {
    border-color: #88b8f0;
}
QComboBox:focus,
QFontComboBox:focus {
    border-color: #2f7ee6;
}
QComboBox::drop-down,
QFontComboBox::drop-down {
    width: 18px;
    border-left: 1px solid #e0e5ec;
}
QMenu {
    color: #1f2937;
    background: #ffffff;
    border: 1px solid #cfd7e3;
    padding: 4px;
}
QMenu::item {
    min-height: 22px;
    padding: 3px 28px 3px 24px;
}
QMenu::item:selected {
    background: #e8f1ff;
    color: #0f5fd0;
}
)"));
}

// ═══════════════════════════════════════════════════════════════
// 状态栏与编辑器
// ═══════════════════════════════════════════════════════════════

void MainWindow::setupStatusBar()
{
    filePathLabel_ = new QLabel("未命名文档");
    modifiedLabel_ = new QLabel("");
    messageLabel_ = new QLabel("");
    zoomLabel_ = new QLabel(this);
    zoomLabel_->setMinimumWidth(72);
    zoomLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    statusBar()->addWidget(filePathLabel_, 2);
    statusBar()->addWidget(modifiedLabel_, 1);
    statusBar()->addWidget(messageLabel_, 3);
    statusBar()->addPermanentWidget(zoomLabel_);
    updateZoomLabel();
}

void MainWindow::setupEditor()
{
    setCentralWidget(editor_);
    editor_->setAcceptRichText(true);
    editor_->document()->setDefaultFont(QFont("等线", 12));
    editor_->setPageSize(PageSizes::A4());
}

void MainWindow::connectSignals()
{
    connect(editor_->document(), &QTextDocument::modificationChanged,
            this, &MainWindow::onEditorModified);
    connect(editor_, &PageView::zoomChanged, this, [this](qreal) { updateZoomLabel(); });

    // 光标/格式变化 → 同步功能区按钮状态
    connect(editor_, &PageView::currentCharFormatChanged,
            this, &MainWindow::updateRibbonState);
    connect(editor_, &PageView::cursorPositionChanged,
            this, &MainWindow::updateRibbonState);

    connect(controller_, &DocumentController::modifiedChanged, this, [this](bool modified) {
        modifiedLabel_->setText(modified ? "● 已修改" : "  已保存");
        updateTitle();
    });
    connect(controller_, &DocumentController::pathChanged, this, [this](const QString& path) {
        if (path.isEmpty()) filePathLabel_->setText("未命名文档");
        else filePathLabel_->setText(path);
        updateTitle();
    });
    connect(controller_, &DocumentController::statusMessage, this, [this](const QString& msg) {
        messageLabel_->setText(msg);
    });
    connect(controller_, &DocumentController::errorOccurred, this, [this](const QString& msg) {
        messageLabel_->setText("错误: " + msg);
        QMessageBox::warning(this, "错误", msg);
    });
}

// ═══════════════════════════════════════════════════════════════
// 文件操作
// ═══════════════════════════════════════════════════════════════

void MainWindow::onNewDocument()
{
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
    if (controller_->newDocument()) {
        imageStore_->clear();
        editor_->clear();
        editor_->document()->setModified(false);
    }
}

void MainWindow::onOpenDocument()
{
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
    doExport();
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
    if (!path.endsWith(".docx", Qt::CaseInsensitive)) path += ".docx";
    controller_->saveDocumentAs(path);
}

void MainWindow::onExportPdf()
{
    QString path = QFileDialog::getSaveFileName(this, "导出 PDF", {},
        "PDF 文件 (*.pdf)");
    if (path.isEmpty()) return;
    if (!path.endsWith(".pdf", Qt::CaseInsensitive)) path += ".pdf";
    QPrinter printer;
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageSize(QPageSize(QPageSize::A4));
    editor_->document()->print(&printer);
    messageLabel_->setText("已导出 PDF: " + path);
}

// ═══════════════════════════════════════════════════════════════
// 插入
// ═══════════════════════════════════════════════════════════════

void MainWindow::onInsertImage()
{
    if (!controller_->isOpen()) return;
    QStringList filters;
    filters << "图片文件 (*.png *.jpg *.jpeg *.gif *.bmp)"
            << "PNG (*.png)" << "JPEG (*.jpg *.jpeg)"
            << "GIF (*.gif)" << "BMP (*.bmp)" << "所有文件 (*)";
    QString filePath = QFileDialog::getOpenFileName(this, "插入图片", {},
        filters.join(";;"));
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "插入图片", "无法打开图片文件");
        return;
    }
    QByteArray imgData = file.readAll();
    file.close();

    QFileInfo fi(filePath);
    QString suffix = fi.suffix().toLower();
    if (suffix.isEmpty()) suffix = "png";

    QString resourceName = imageStore_->addImage(imgData, suffix);
    if (resourceName.isEmpty()) {
        QMessageBox::warning(this, "插入图片", "无法添加图片");
        return;
    }
    imageStore_->addImagesToDocument(*editor_->document());

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
        imgFmt.setWidth(200);
        imgFmt.setHeight(150);
    }
    QTextCursor cursor = editor_->textCursor();
    cursor.insertImage(imgFmt);
    editor_->setTextCursor(cursor);
    messageLabel_->setText("已插入图片");
}

void MainWindow::onInsertTable()
{
    if (!controller_->isOpen()) return;
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

// ═══════════════════════════════════════════════════════════════
// 页面设置
// ═══════════════════════════════════════════════════════════════

void MainWindow::onPageSetup()
{
    PageSetupDialog dialog(this);
    dialog.setPageSizeInPoints(editor_->pageSize().width(), editor_->pageSize().height());
    dialog.setMarginsInPoints(editor_->pageMarginLeft(), editor_->pageMarginTop(),
                              editor_->pageMarginRight(), editor_->pageMarginBottom());
    if (dialog.exec() != QDialog::Accepted) return;
    editor_->setPageSize(dialog.pageSizeInPoints());
    editor_->setDocumentMargins(dialog.marginLeftInPoints(), dialog.marginTopInPoints(),
                                dialog.marginRightInPoints(), dialog.marginBottomInPoints());
    QSizeF size = dialog.pageSizeInPoints();
    messageLabel_->setText(QString("页面设置: %.0f x %.0f mm")
        .arg(size.width() * 25.4 / 72.0).arg(size.height() * 25.4 / 72.0));
}

// ═══════════════════════════════════════════════════════════════
// 格式操作
// ═══════════════════════════════════════════════════════════════

void MainWindow::onFontFamilyChanged(const QString& family)
{
    QTextCharFormat fmt;
    fmt.setFontFamilies({family});
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void MainWindow::onFontSizeChanged(const QString& sizeText)
{
    bool ok = false;
    qreal size = sizeText.toDouble(&ok);
    if (!ok || size <= 0) return;
    QTextCharFormat fmt;
    fmt.setFontPointSize(size);
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void MainWindow::onBoldToggled(bool checked)
{
    QTextCharFormat fmt;
    fmt.setFontWeight(checked ? QFont::Bold : QFont::Normal);
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void MainWindow::onItalicToggled(bool checked)
{
    QTextCharFormat fmt;
    fmt.setFontItalic(checked);
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void MainWindow::onUnderlineToggled(bool checked)
{
    QTextCharFormat fmt;
    fmt.setUnderlineStyle(checked ? QTextCharFormat::SingleUnderline
                                  : QTextCharFormat::NoUnderline);
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void MainWindow::onStrikeToggled(bool checked)
{
    QTextCharFormat fmt;
    fmt.setFontStrikeOut(checked);
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void MainWindow::onTextColorClicked()
{
    QColor color = QColorDialog::getColor(editor_->textColor(), this, "选择文字颜色");
    if (!color.isValid()) return;
    QTextCharFormat fmt;
    fmt.setForeground(color);
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void MainWindow::onBackgroundColorClicked()
{
    QColor color = QColorDialog::getColor(editor_->textBackgroundColor(), this, "选择背景颜色");
    if (!color.isValid()) return;
    QTextCharFormat fmt;
    fmt.setBackground(color);
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void MainWindow::onHighlightSelected(QAction* action)
{
    const QString value = action->data().toString();
    QTextCharFormat fmt;
    fmt.setProperty(HighlightColorProperty, value);
    if (value == "none") {
        fmt.clearProperty(HighlightColorProperty);
    } else {
        const QColor displayColor = highlightColorToQt(value);
        if (displayColor.isValid()) {
            fmt.setBackground(displayColor);
        }
    }
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void MainWindow::onBorderToggled(bool checked)
{
    QTextCharFormat fmt;
    if (checked) {
        fmt.setProperty(BorderColorProperty, QStringLiteral("000000"));
    } else {
        fmt.setProperty(BorderColorProperty, QString());
    }
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void MainWindow::onEnclosedCircleToggled(bool checked)
{
    QTextCharFormat fmt;
    fmt.setProperty(EnclosedCircleProperty, checked);
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void MainWindow::onChangeCaseSelected(QAction* action)
{
    if (action == upperCaseAction_) {
        editor_->applyTextCase(PageView::TextCaseTransform::Upper);
    } else if (action == lowerCaseAction_) {
        editor_->applyTextCase(PageView::TextCaseTransform::Lower);
    } else if (action == titleCaseAction_) {
        editor_->applyTextCase(PageView::TextCaseTransform::Title);
    }
    editor_->setFocus();
}

void MainWindow::onAlignmentChanged(QAction* action)
{
    Qt::Alignment align = Qt::AlignLeft;
    if (action == alignCenterAction_) align = Qt::AlignHCenter;
    else if (action == alignRightAction_) align = Qt::AlignRight;
    else if (action == alignJustifyAction_) align = Qt::AlignJustify;
    QTextBlockFormat blockFmt;
    blockFmt.setAlignment(align);
    QTextCursor cursor = editor_->textCursor();
    cursor.mergeBlockFormat(blockFmt);
    editor_->setTextCursor(cursor);
    editor_->setFocus();
}

// ═══════════════════════════════════════════════════════════════
// 状态同步
// ═══════════════════════════════════════════════════════════════

void MainWindow::updateRibbonState()
{
    QTextCharFormat fmt = editor_->currentCharFormat();

    boldAction_->blockSignals(true);
    boldAction_->setChecked(fmt.fontWeight() >= QFont::Bold);
    boldAction_->blockSignals(false);

    italicAction_->blockSignals(true);
    italicAction_->setChecked(fmt.fontItalic());
    italicAction_->blockSignals(false);

    underlineAction_->blockSignals(true);
    underlineAction_->setChecked(fmt.underlineStyle() != QTextCharFormat::NoUnderline);
    underlineAction_->blockSignals(false);

    strikeAction_->blockSignals(true);
    strikeAction_->setChecked(fmt.fontStrikeOut());
    strikeAction_->blockSignals(false);

    fontCombo_->blockSignals(true);
    const QStringList families = fmt.property(QTextFormat::FontFamilies).toStringList();
    if (!families.isEmpty()) fontCombo_->setCurrentFont(QFont(families.first()));
    fontCombo_->blockSignals(false);

    fontSizeCombo_->blockSignals(true);
    if (fmt.hasProperty(QTextFormat::FontPointSize))
        fontSizeCombo_->setCurrentText(QString::number(fmt.fontPointSize()));
    fontSizeCombo_->blockSignals(false);

    borderAction_->blockSignals(true);
    const QVariant borderProp = fmt.property(BorderColorProperty);
    borderAction_->setChecked(borderProp.isValid() && !borderProp.toString().isEmpty());
    borderAction_->blockSignals(false);

    enclosedCircleAction_->blockSignals(true);
    enclosedCircleAction_->setChecked(fmt.property(EnclosedCircleProperty).toBool());
    enclosedCircleAction_->blockSignals(false);

    QTextBlockFormat blockFmt = editor_->textCursor().blockFormat();
    alignLeftAction_->blockSignals(true);
    alignCenterAction_->blockSignals(true);
    alignRightAction_->blockSignals(true);
    alignJustifyAction_->blockSignals(true);
    Qt::Alignment align = blockFmt.alignment();
    if (align & Qt::AlignHCenter) alignCenterAction_->setChecked(true);
    else if (align & Qt::AlignRight) alignRightAction_->setChecked(true);
    else if (align & Qt::AlignJustify) alignJustifyAction_->setChecked(true);
    else alignLeftAction_->setChecked(true);
    alignLeftAction_->blockSignals(false);
    alignCenterAction_->blockSignals(false);
    alignRightAction_->blockSignals(false);
    alignJustifyAction_->blockSignals(false);
}

void MainWindow::onEditorModified()
{
    controller_->setModified(editor_->document()->isModified());
}

void MainWindow::updateStatusFromController()
{
    updateTitle();
}

void MainWindow::updateTitle()
{
    QString title = controller_->currentFileName();
    if (controller_->isModified()) title += " *";
    title += " - CppWordKit Word Editor";
    setWindowTitle(title);
}

void MainWindow::updateZoomLabel()
{
    zoomLabel_->setText(QString("缩放 %1%").arg(qRound(editor_->displayZoom() * 100.0)));
}

// ═══════════════════════════════════════════════════════════════
// 导入 / 导出
// ═══════════════════════════════════════════════════════════════

void MainWindow::doImport()
{
    if (!controller_->wordDocument()) return;
    auto model = controller_->wordDocument()->model();
    mapper_->importModel(*editor_->document(), model, *controller_->wordDocument());

    if (model.pageSetup.pageWidthTwips && model.pageSetup.pageHeightTwips) {
        QSizeF size(*model.pageSetup.pageWidthTwips / 20.0,
                    *model.pageSetup.pageHeightTwips / 20.0);
        editor_->setPageSize(size);
    } else {
        editor_->setPageSize(PageSizes::A4());
    }
    if (model.pageSetup.marginLeftTwips || model.pageSetup.marginTopTwips ||
        model.pageSetup.marginRightTwips || model.pageSetup.marginBottomTwips) {
        qreal l = model.pageSetup.marginLeftTwips.value_or(1440) / 20.0;
        qreal t = model.pageSetup.marginTopTwips.value_or(1440) / 20.0;
        qreal r = model.pageSetup.marginRightTwips.value_or(1440) / 20.0;
        qreal b = model.pageSetup.marginBottomTwips.value_or(1440) / 20.0;
        editor_->setDocumentMargins(l, t, r, b);
    }
    editor_->document()->setModified(false);
}

void MainWindow::doExport()
{
    if (!controller_->wordDocument()) return;
    auto model = mapper_->exportModel(*editor_->document(), *controller_->wordDocument());
    QSizeF size = editor_->pageSize();
    model.pageSetup.pageWidthTwips = static_cast<std::int32_t>(size.width() * 20.0);
    model.pageSetup.pageHeightTwips = static_cast<std::int32_t>(size.height() * 20.0);
    model.pageSetup.marginTopTwips = static_cast<std::int32_t>(editor_->pageMarginTop() * 20.0);
    model.pageSetup.marginBottomTwips = static_cast<std::int32_t>(editor_->pageMarginBottom() * 20.0);
    model.pageSetup.marginLeftTwips = static_cast<std::int32_t>(editor_->pageMarginLeft() * 20.0);
    model.pageSetup.marginRightTwips = static_cast<std::int32_t>(editor_->pageMarginRight() * 20.0);
    controller_->wordDocument()->replaceBody(model);
}
