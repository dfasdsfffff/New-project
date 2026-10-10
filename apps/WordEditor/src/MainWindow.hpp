//! @file MainWindow.hpp 应用程序主窗口，协调编辑器各组件交互

#pragma once

#include <SARibbon.h>

#include <QLabel>
#include <QFontComboBox>
#include <QComboBox>
#include <QAction>
#include <QMenu>

class PageView;
class DocumentController;
class QtDocumentMapper;
class ImageStore;

/**
 * @brief 应用程序主窗口
 *
 * 基于 SARibbonMainWindow，使用 SARibbon 功能区控件。
 * 负责组织文档编辑器的整体布局，包括功能区、编辑器和状态栏。
 */
class MainWindow : public SARibbonMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    // 文件菜单
    void onNewDocument();
    void onOpenDocument();
    void onSaveDocument();
    void onSaveDocumentAs();
    void onExportPdf();
    // 插入菜单
    void onInsertImage();
    void onInsertTable();
    // 页面设置
    void onPageSetup();
    // 格式操作
    void onFontFamilyChanged(const QString& family);
    void onFontSizeChanged(const QString& sizeText);
    void onBoldToggled(bool checked);
    void onItalicToggled(bool checked);
    void onUnderlineToggled(bool checked);
    void onStrikeToggled(bool checked);
    void onTextColorClicked();
    void onBackgroundColorClicked();
    void onHighlightSelected(QAction* action);
    void onBorderToggled(bool checked);
    void onEnclosedCircleToggled(bool checked);
    void onChangeCaseSelected(QAction* action);
    void onAlignmentChanged(QAction* action);
    // 同步
    void onEditorModified();
    void updateStatusFromController();
    void updateZoomLabel();
    void updateRibbonState();

private:
    void setupMenus();
    void setupRibbon();
    void applyRibbonStyle();
    void setupStatusBar();
    void setupEditor();
    void connectSignals();
    void updateTitle();
    void doImport();
    void doExport();
    bool confirmSaveChanges();
    bool saveToPath(const QString& path);

    // ── 编辑器 ──
    PageView* editor_;
    DocumentController* controller_;
    ImageStore* imageStore_;
    QtDocumentMapper* mapper_;

    // ── 状态栏 ──
    QLabel* filePathLabel_;
    QLabel* modifiedLabel_;
    QLabel* messageLabel_;
    QLabel* zoomLabel_;

    // ── 菜单 ──
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
    QAction* pageSetupAction_;

    // ── 功能区控件 ──
    QFontComboBox* fontCombo_;
    QComboBox* fontSizeCombo_;
    QAction* boldAction_;
    QAction* italicAction_;
    QAction* underlineAction_;
    QAction* strikeAction_;
    QAction* textColorAction_;
    QAction* backgroundColorAction_;
    QAction* highlightAction_;
    QAction* borderAction_;
    QAction* enclosedCircleAction_;
    QAction* upperCaseAction_;
    QAction* lowerCaseAction_;
    QAction* titleCaseAction_;
    QMenu* highlightMenu_;
    QMenu* changeCaseMenu_;
    QAction* alignLeftAction_;
    QAction* alignCenterAction_;
    QAction* alignRightAction_;
    QAction* alignJustifyAction_;
};
