//! @file FormatToolbar.hpp 格式工具栏，提供富文本格式编辑控件

#pragma once

#include <QToolBar>
#include <QFontComboBox>
#include <QComboBox>
#include <QAction>
#include <QTextEdit>
#include <QColor>
#include <QLabel>

/**
 * @brief 格式工具栏
 *
 * 提供字体选择、字号选择、粗体/斜体/下划线/删除线、文字颜色、背景颜色、
 * 段落对齐等富文本格式控制功能。
 * 监听编辑器的光标位置变化以实时同步当前格式状态。
 */
class FormatToolbar : public QToolBar {
    Q_OBJECT
public:
    explicit FormatToolbar(QWidget* parent = nullptr);

    /// 关联到目标编辑器，设置信号连接并初始化格式状态
    void setEditor(QTextEdit* editor);

private slots:
    void onFontFamilyChanged(const QString& family);     ///< 字体选择变化时应用
    void onFontSizeChanged(const QString& sizeText);     ///< 字号选择变化时应用
    void onBoldToggled(bool checked);                    ///< 粗体开关
    void onItalicToggled(bool checked);                  ///< 斜体开关
    void onUnderlineToggled(bool checked);               ///< 下划线开关
    void onStrikeToggled(bool checked);                  ///< 删除线开关
    void onTextColorClicked();                           ///< 文字颜色选择
    void onBackgroundColorClicked();                     ///< 背景颜色选择
    void onAlignmentChanged(QAction* action);            ///< 段落对齐切换
    /// 响应编辑器光标/格式变化，同步工具栏状态
    void updateStateFromEditor();

private:
    void setupUi();          // 创建工具栏控件布局
    void setupConnections(); // 连接控件信号到槽函数

    QFontComboBox* fontCombo_;           ///< 字体选择下拉框
    QComboBox* fontSizeCombo_;           ///< 字号选择下拉框
    QAction* boldAction_;                ///< 粗体按钮
    QAction* italicAction_;              ///< 斜体按钮
    QAction* underlineAction_;           ///< 下划线按钮
    QAction* strikeAction_;              ///< 删除线按钮
    QAction* textColorAction_;           ///< 文字颜色按钮
    QAction* backgroundColorAction_;     ///< 背景颜色按钮
    QAction* alignLeftAction_;           ///< 左对齐按钮
    QAction* alignCenterAction_;         ///< 居中对齐按钮
    QAction* alignRightAction_;          ///< 右对齐按钮
    QAction* alignJustifyAction_;        ///< 两端对齐按钮

    QTextEdit* editor_ = nullptr;        ///< 当前关联的编辑器
};
