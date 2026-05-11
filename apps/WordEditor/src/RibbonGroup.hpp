//! @file RibbonGroup.hpp 功能区组控件

#pragma once

#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

/**
 * @brief 功能区组 —— 在 Ribbon 选项卡内容区中分组控件
 *
 * 每个组包含一个内容区（QGridLayout）和底部标题。
 * 组之间有视觉分隔线。
 */
class RibbonGroup : public QFrame {
    Q_OBJECT
public:
    explicit RibbonGroup(const QString& title, QWidget* parent = nullptr);

    /// 在指定行、列添加控件
    void addWidget(QWidget* widget, int row, int col, int rowSpan = 1, int colSpan = 1,
                   Qt::Alignment align = Qt::AlignLeft);
    /// 添加一个不可见占位，用于撑开列宽
    void addColumnSpacer(int col, int minWidth);
    /// 设置行高，保证统一高度
    void setRowMinimumHeight(int row, int height);

private:
    QGridLayout* grid_;
    QLabel* titleLabel_;
};
