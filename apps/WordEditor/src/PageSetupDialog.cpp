//! @file PageSetupDialog.cpp 页面设置对话框实现

#include "PageSetupDialog.hpp"
#include "PageView.hpp"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>

namespace {

// 预设页面尺寸（磅）
struct Preset {
    QString label;
    QSizeF size;
};

const Preset presets[] = {
    {"A4 (210 x 297 mm)",            PageSizes::A4()},
    {"A3 (297 x 420 mm)",            PageSizes::A3()},
    {"A5 (148 x 210 mm)",            PageSizes::A5()},
    {"Letter (8.5 x 11 inch)",      PageSizes::Letter()},
    {"Legal (8.5 x 14 inch)",       PageSizes::Legal()},
};

// mm → 磅（1mm = 72/25.4 pt）
constexpr qreal mmToPt(qreal mm) { return mm * 72.0 / 25.4; }
// 磅 → mm
constexpr qreal ptToMm(qreal pt) { return pt * 25.4 / 72.0; }

} // namespace

PageSetupDialog::PageSetupDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
}

void PageSetupDialog::setupUi()
{
    setWindowTitle("页面设置");
    setMinimumWidth(360);

    auto* mainLayout = new QVBoxLayout(this);

    // 页面尺寸
    auto* formLayout = new QFormLayout;
    pageSizeCombo_ = new QComboBox(this);
    for (const auto& p : presets) {
        pageSizeCombo_->addItem(p.label);
    }
    pageSizeCombo_->setCurrentIndex(0);
    formLayout->addRow("页面尺寸:", pageSizeCombo_);

    // 页边距（mm）
    auto createMarginSpin = [this](double defaultMm) {
        auto* spin = new QDoubleSpinBox(this);
        spin->setRange(0.0, 100.0);
        spin->setDecimals(1);
        spin->setSingleStep(1.0);
        spin->setSuffix(" mm");
        spin->setValue(defaultMm);
        return spin;
    };

    marginTopSpin_ = createMarginSpin(25.4);
    marginBottomSpin_ = createMarginSpin(25.4);
    marginLeftSpin_ = createMarginSpin(31.8);
    marginRightSpin_ = createMarginSpin(31.8);

    formLayout->addRow("上边距:", marginTopSpin_);
    formLayout->addRow("下边距:", marginBottomSpin_);
    formLayout->addRow("左边距:", marginLeftSpin_);
    formLayout->addRow("右边距:", marginRightSpin_);

    mainLayout->addLayout(formLayout);
    mainLayout->addSpacing(12);

    // 确定/取消按钮
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void PageSetupDialog::setPageSizeInPoints(qreal widthPt, qreal heightPt)
{
    const QSizeF target(widthPt, heightPt);
    for (int i = 0; i < pageSizeCombo_->count(); ++i) {
        if (qFuzzyCompare(presets[i].size.width(), target.width()) &&
            qFuzzyCompare(presets[i].size.height(), target.height())) {
            pageSizeCombo_->setCurrentIndex(i);
            return;
        }
    }
    // 非预设尺寸时，保留当前选择
}

void PageSetupDialog::setMarginsInPoints(qreal left, qreal top, qreal right, qreal bottom)
{
    marginLeftSpin_->setValue(ptToMm(left));
    marginTopSpin_->setValue(ptToMm(top));
    marginRightSpin_->setValue(ptToMm(right));
    marginBottomSpin_->setValue(ptToMm(bottom));
}

QSizeF PageSetupDialog::pageSizeInPoints() const
{
    int idx = pageSizeCombo_->currentIndex();
    if (idx >= 0 && idx < static_cast<int>(std::size(presets))) {
        return presets[idx].size;
    }
    return PageSizes::A4();
}

qreal PageSetupDialog::marginLeftInPoints() const   { return mmToPt(static_cast<qreal>(marginLeftSpin_->value())); }
qreal PageSetupDialog::marginTopInPoints() const     { return mmToPt(static_cast<qreal>(marginTopSpin_->value())); }
qreal PageSetupDialog::marginRightInPoints() const   { return mmToPt(static_cast<qreal>(marginRightSpin_->value())); }
qreal PageSetupDialog::marginBottomInPoints() const  { return mmToPt(static_cast<qreal>(marginBottomSpin_->value())); }
