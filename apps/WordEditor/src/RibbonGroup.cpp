//! @file RibbonGroup.cpp 功能区组实现

#include "RibbonGroup.hpp"

#include <QPainter>
#include <QStyleOption>

RibbonGroup::RibbonGroup(const QString& title, QWidget* parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::NoFrame);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 2);
    mainLayout->setSpacing(2);

    // 内容网格
    grid_ = new QGridLayout;
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setSpacing(3);
    grid_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    mainLayout->addLayout(grid_);

    mainLayout->addStretch();

    // 底部标题
    titleLabel_ = new QLabel(title, this);
    titleLabel_->setAlignment(Qt::AlignCenter);
    titleLabel_->setStyleSheet("color: #666; font-size: 10px;");
    mainLayout->addWidget(titleLabel_);
}

void RibbonGroup::addWidget(QWidget* widget, int row, int col, int rowSpan, int colSpan,
                            Qt::Alignment align)
{
    grid_->addWidget(widget, row, col, rowSpan, colSpan, align);
}

void RibbonGroup::addColumnSpacer(int col, int minWidth)
{
    auto* spacer = new QWidget(this);
    spacer->setMinimumWidth(minWidth);
    spacer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    grid_->addWidget(spacer, 0, col, 1, 1);
}

void RibbonGroup::setRowMinimumHeight(int row, int height)
{
    grid_->setRowMinimumHeight(row, height);
}
