//! @file FormatToolbar.hpp 功能区格式工具栏

#pragma once

#include <QWidget>
#include <QTabBar>
#include <QStackedWidget>
#include <QFontComboBox>
#include <QComboBox>
#include <QToolButton>
#include <QAction>
#include <QMenu>
#include <QLabel>

class PageView;
class RibbonGroup;

/**
 * @brief 功能区样式格式工具栏
 *
 * 采用 Ribbon 设计，将编辑功能分为"开始"和"页面"两个选项卡。
 * 每个选项卡内按功能分组（字体、段落、页面设置）。
 * 不再继承 QToolBar，而是作为普通 QWidget 嵌入主窗口布局。
 */
class FormatToolbar : public QWidget {
    Q_OBJECT
public:
    explicit FormatToolbar(QWidget* parent = nullptr);

    /// 关联到目标编辑器，设置信号连接并初始化格式状态
    void setEditor(PageView* editor);

signals:
    /// 用户点击"页面设置"按钮时发出，由 MainWindow 处理
    void pageSetupRequested();

private slots:
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
    /// 响应编辑器光标/格式变化，同步工具栏状态
    void updateStateFromEditor();

private:
    void setupUi();
    void setupConnections();
    void setupFontGroup(RibbonGroup* group);
    void setupParagraphGroup(RibbonGroup* group);
    void setupPageGroup(RibbonGroup* group);

    // 选项卡
    QTabBar* tabBar_;
    QStackedWidget* tabPages_;

    // 字体控件
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

    // 段落控件
    QAction* alignLeftAction_;
    QAction* alignCenterAction_;
    QAction* alignRightAction_;
    QAction* alignJustifyAction_;

    PageView* editor_ = nullptr;
};
