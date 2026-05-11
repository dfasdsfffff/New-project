//! @file PageSetupDialog.hpp 页面设置对话框

#pragma once

#include <QDialog>
#include <QComboBox>
#include <QDoubleSpinBox>

/**
 * @brief 页面设置对话框
 *
 * 提供页面尺寸选择和页边距（上/下/左/右，单位 mm）的设置界面。
 * 调用者通过 getter 获取用户选择的页面尺寸和边距值（磅）。
 */
class PageSetupDialog : public QDialog {
    Q_OBJECT
public:
    explicit PageSetupDialog(QWidget* parent = nullptr);

    /// 设置当前页面尺寸（磅），用于初始化对话框显示
    void setPageSizeInPoints(qreal widthPt, qreal heightPt);
    /// 设置当前页边距（磅）
    void setMarginsInPoints(qreal left, qreal top, qreal right, qreal bottom);

    /// 获取用户选择的页面尺寸（磅）
    QSizeF pageSizeInPoints() const;
    /// 获取用户设置的页边距（磅）
    qreal marginLeftInPoints() const;
    qreal marginTopInPoints() const;
    qreal marginRightInPoints() const;
    qreal marginBottomInPoints() const;

private:
    void setupUi();

    QComboBox* pageSizeCombo_;
    QDoubleSpinBox* marginTopSpin_;
    QDoubleSpinBox* marginBottomSpin_;
    QDoubleSpinBox* marginLeftSpin_;
    QDoubleSpinBox* marginRightSpin_;
};
