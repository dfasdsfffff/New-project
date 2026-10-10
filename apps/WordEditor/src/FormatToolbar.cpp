//! @file FormatToolbar.cpp 功能区格式工具栏实现

#include "FormatToolbar.hpp"
#include "RibbonGroup.hpp"
#include "PageView.hpp"

#include <QFontDatabase>
#include <QColorDialog>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QActionGroup>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPen>

namespace {

constexpr int HighlightColorProperty = QTextFormat::UserProperty + 101;
constexpr int BorderColorProperty = QTextFormat::UserProperty + 102;
constexpr int EnclosedCircleProperty = QTextFormat::UserProperty + 103;
constexpr int ShadingColorProperty = QTextFormat::UserProperty + 104;

void addHighlightColor(QMenu& menu, const QString& name, const QString& value)
{
    QAction* action = menu.addAction(name);
    action->setData(value);
}

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

// 功能区样式常量
namespace RibbonStyle {
    constexpr int buttonIconSize = 16;
    constexpr int comboMinWidth = 140;
    constexpr int comboHeight = 22;
    constexpr int fontSizeComboWidth = 56;
    constexpr int rowHeight = 30;
    constexpr int ribbonHeight = 110;
}

FormatToolbar::FormatToolbar(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    setupConnections();
}

// 创建带关联 QAction 的 QToolButton，outAction 输出 action 指针供信号连接
QToolButton* makeActionButton(QWidget* parent, QAction** outAction, const QString& text, const QString& tip, bool checkable)
{
    auto* action = new QAction(text, parent);
    action->setCheckable(checkable);
    action->setToolTip(tip);

    auto* btn = new QToolButton(parent);
    btn->setDefaultAction(action);
    btn->setIconSize(QSize(RibbonStyle::buttonIconSize, RibbonStyle::buttonIconSize));
    btn->setAutoRaise(true);
    btn->setStyleSheet(
        "QToolButton { padding: 2px 6px; }"
        "QToolButton:checked { background-color: #c0d8f0; border: 1px solid #8bb8e0; border-radius: 3px; }"
    );
    *outAction = action;
    return btn;
}

// 无关联 action 的菜单按钮（信号通过 QMenu::triggered 发射）
QToolButton* makeMenuButton(QWidget* parent, const QString& text, const QString& tip, QMenu* menu)
{
    auto* btn = new QToolButton(parent);
    btn->setText(text);
    btn->setToolTip(tip);
    btn->setMenu(menu);
    btn->setPopupMode(QToolButton::InstantPopup);
    btn->setIconSize(QSize(RibbonStyle::buttonIconSize, RibbonStyle::buttonIconSize));
    btn->setAutoRaise(true);
    btn->setStyleSheet(
        "QToolButton { padding: 2px 6px; }"
    );
    return btn;
}

// 简单的纯文本按钮（无 action），返回 QToolButton 指针
QToolButton* makeTextButton(QWidget* parent, const QString& text, const QString& tip)
{
    auto* btn = new QToolButton(parent);
    btn->setText(text);
    btn->setToolTip(tip);
    btn->setIconSize(QSize(RibbonStyle::buttonIconSize, RibbonStyle::buttonIconSize));
    btn->setAutoRaise(true);
    btn->setStyleSheet(
        "QToolButton { padding: 2px 6px; }"
    );
    return btn;
}

void FormatToolbar::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 选项卡栏
    tabBar_ = new QTabBar(this);
    tabBar_->setExpanding(false);
    tabBar_->setDrawBase(false);
    tabBar_->addTab("开始");
    tabBar_->addTab("页面");
    tabBar_->setStyleSheet(
        "QTabBar::tab { padding: 6px 20px; border: none; background: transparent; font-size: 12px; }"
        "QTabBar::tab:selected { background-color: #f0f0f0; font-weight: bold; }"
        "QTabBar::tab:hover:!selected { background-color: #e8e8e8; }"
    );
    mainLayout->addWidget(tabBar_);

    // 选项卡内容区
    tabPages_ = new QStackedWidget(this);
    tabPages_->setStyleSheet("QStackedWidget { background-color: #f0f0f0; }");

    // ---- 开始选项卡 ----
    auto* homePage = new QWidget(this);
    auto* homeLayout = new QHBoxLayout(homePage);
    homeLayout->setContentsMargins(4, 4, 4, 4);
    homeLayout->setSpacing(0);

    // 字体组
    auto* fontGroup = new RibbonGroup("字体", this);
    setupFontGroup(fontGroup);
    homeLayout->addWidget(fontGroup);

    // 段落组
    auto* paragraphGroup = new RibbonGroup("段落", this);
    setupParagraphGroup(paragraphGroup);
    homeLayout->addWidget(paragraphGroup);

    homeLayout->addStretch();
    tabPages_->addWidget(homePage);

    // ---- 页面选项卡 ----
    auto* pagePage = new QWidget(this);
    auto* pageLayout = new QHBoxLayout(pagePage);
    pageLayout->setContentsMargins(4, 4, 4, 4);
    pageLayout->setSpacing(0);

    auto* pageGroup = new RibbonGroup("页面设置", this);
    setupPageGroup(pageGroup);
    pageLayout->addWidget(pageGroup);

    pageLayout->addStretch();
    tabPages_->addWidget(pagePage);

    mainLayout->addWidget(tabPages_);

    // 选项卡切换
    connect(tabBar_, &QTabBar::currentChanged, tabPages_, &QStackedWidget::setCurrentIndex);

    setFixedHeight(RibbonStyle::ribbonHeight);
    setStyleSheet("FormatToolbar { border-bottom: 1px solid #d0d0d0; }");
}

void FormatToolbar::setupFontGroup(RibbonGroup* group)
{
    // 行 0：字体 + 字号
    fontCombo_ = new QFontComboBox(this);
    fontCombo_->setMinimumWidth(RibbonStyle::comboMinWidth);
    fontCombo_->setMaximumHeight(RibbonStyle::comboHeight);
    fontCombo_->setWritingSystem(QFontDatabase::SimplifiedChinese);
    group->addWidget(fontCombo_, 0, 0, 1, 3);

    fontSizeCombo_ = new QComboBox(this);
    fontSizeCombo_->setEditable(true);
    fontSizeCombo_->setMinimumWidth(RibbonStyle::fontSizeComboWidth);
    fontSizeCombo_->setMaximumHeight(RibbonStyle::comboHeight);
    QStringList sizes;
    sizes << "8" << "9" << "10" << "10.5" << "11" << "12" << "14" << "16"
          << "18" << "20" << "22" << "24" << "26" << "28" << "36" << "48" << "72";
    fontSizeCombo_->addItems(sizes);
    fontSizeCombo_->setCurrentText("12");
    group->addWidget(fontSizeCombo_, 0, 3, 1, 1);

    group->setRowMinimumHeight(0, RibbonStyle::rowHeight);

    // 行 1：样式按钮行
    // 粗体
    auto* boldBtn = makeActionButton(this, &boldAction_, "B", "加粗 (Ctrl+B)", true);
    QFont boldFont = boldBtn->font();
    boldFont.setBold(true);
    boldBtn->setFont(boldFont);
    group->addWidget(boldBtn, 1, 0);

    // 斜体
    auto* italicBtn = makeActionButton(this, &italicAction_, "I", "斜体 (Ctrl+I)", true);
    QFont italicFont = italicBtn->font();
    italicFont.setItalic(true);
    italicBtn->setFont(italicFont);
    group->addWidget(italicBtn, 1, 1);

    // 下划线
    auto* ulBtn = makeActionButton(this, &underlineAction_, "U", "下划线 (Ctrl+U)", true);
    QFont ulFont = ulBtn->font();
    ulFont.setUnderline(true);
    ulBtn->setFont(ulFont);
    group->addWidget(ulBtn, 1, 2);

    // 删除线
    auto* strikeBtn = makeActionButton(this, &strikeAction_, "S", "删除线", true);
    QFont strikeFont = strikeBtn->font();
    strikeFont.setStrikeOut(true);
    strikeBtn->setFont(strikeFont);
    group->addWidget(strikeBtn, 1, 3);

    // 分隔
    group->addColumnSpacer(4, 6);

    // 文字颜色
    auto* colorBtn = makeActionButton(this, &textColorAction_, "A", "文字颜色", false);
    QFont colorFont = colorBtn->font();
    colorFont.setBold(true);
    colorBtn->setFont(colorFont);
    group->addWidget(colorBtn, 1, 5);

    // 字符底纹
    auto* bgBtn = makeActionButton(this, &backgroundColorAction_, "底", "字符底纹", false);
    group->addWidget(bgBtn, 1, 6);

    // 高亮菜单
    highlightMenu_ = new QMenu(this);
    addHighlightColor(*highlightMenu_, "黄色", "yellow");
    addHighlightColor(*highlightMenu_, "绿色", "green");
    addHighlightColor(*highlightMenu_, "青色", "cyan");
    addHighlightColor(*highlightMenu_, "红色", "red");
    addHighlightColor(*highlightMenu_, "蓝色", "blue");
    addHighlightColor(*highlightMenu_, "无高亮", "none");
    auto* hlBtn = makeMenuButton(this, "H", "高亮显示", highlightMenu_);
    group->addWidget(hlBtn, 1, 7);

    // 字符边框
    auto* borderBtn = makeActionButton(this, &borderAction_, "□", "字符边框", true);
    group->addWidget(borderBtn, 1, 8);

    // 带圈字符
    auto* circleBtn = makeActionButton(this, &enclosedCircleAction_, "○", "带圈字符", true);
    group->addWidget(circleBtn, 1, 9);

    // 大小写菜单
    changeCaseMenu_ = new QMenu(this);
    upperCaseAction_ = changeCaseMenu_->addAction("大写");
    lowerCaseAction_ = changeCaseMenu_->addAction("小写");
    titleCaseAction_ = changeCaseMenu_->addAction("首字母大写");
    auto* caseBtn = makeMenuButton(this, "Aa", "更改大小写", changeCaseMenu_);
    group->addWidget(caseBtn, 1, 10);
}

void FormatToolbar::setupParagraphGroup(RibbonGroup* group)
{
    QActionGroup* alignGroup = new QActionGroup(this);
    alignGroup->setExclusive(true);

    // 左对齐
    auto* leftBtn = makeActionButton(this, &alignLeftAction_, "≡", "左对齐", true);
    group->addWidget(leftBtn, 0, 0);
    alignGroup->addAction(alignLeftAction_);

    // 居中
    auto* centerBtn = makeActionButton(this, &alignCenterAction_, "≡", "居中", true);
    group->addWidget(centerBtn, 0, 1);
    alignGroup->addAction(alignCenterAction_);

    // 右对齐
    auto* rightBtn = makeActionButton(this, &alignRightAction_, "≡", "右对齐", true);
    group->addWidget(rightBtn, 0, 2);
    alignGroup->addAction(alignRightAction_);

    // 两端对齐
    auto* justifyBtn = makeActionButton(this, &alignJustifyAction_, "≡", "两端对齐", true);
    group->addWidget(justifyBtn, 0, 3);
    alignGroup->addAction(alignJustifyAction_);
}

void FormatToolbar::setupPageGroup(RibbonGroup* group)
{
    auto* pageBtn = makeTextButton(this, "页面设置", "页面尺寸和页边距");
    pageBtn->setMinimumWidth(80);
    group->addWidget(pageBtn, 0, 0);
    connect(pageBtn, &QToolButton::clicked, this, &FormatToolbar::pageSetupRequested);
}

void FormatToolbar::setupConnections()
{
    connect(fontCombo_, &QFontComboBox::textActivated,
            this, &FormatToolbar::onFontFamilyChanged);
    connect(fontSizeCombo_, &QComboBox::textActivated,
            this, &FormatToolbar::onFontSizeChanged);
    connect(boldAction_, &QAction::toggled,
            this, &FormatToolbar::onBoldToggled);
    connect(italicAction_, &QAction::toggled,
            this, &FormatToolbar::onItalicToggled);
    connect(underlineAction_, &QAction::toggled,
            this, &FormatToolbar::onUnderlineToggled);
    connect(strikeAction_, &QAction::toggled,
            this, &FormatToolbar::onStrikeToggled);
    connect(textColorAction_, &QAction::triggered,
            this, &FormatToolbar::onTextColorClicked);
    connect(backgroundColorAction_, &QAction::triggered,
            this, &FormatToolbar::onBackgroundColorClicked);

    connect(highlightMenu_, &QMenu::triggered,
            this, &FormatToolbar::onHighlightSelected);
    connect(borderAction_, &QAction::toggled,
            this, &FormatToolbar::onBorderToggled);
    connect(enclosedCircleAction_, &QAction::toggled,
            this, &FormatToolbar::onEnclosedCircleToggled);

    connect(upperCaseAction_, &QAction::triggered,
            this, [this]() { onChangeCaseSelected(upperCaseAction_); });
    connect(lowerCaseAction_, &QAction::triggered,
            this, [this]() { onChangeCaseSelected(lowerCaseAction_); });
    connect(titleCaseAction_, &QAction::triggered,
            this, [this]() { onChangeCaseSelected(titleCaseAction_); });

    connect(alignLeftAction_, &QAction::triggered,
            this, [this]() { onAlignmentChanged(alignLeftAction_); });
    connect(alignCenterAction_, &QAction::triggered,
            this, [this]() { onAlignmentChanged(alignCenterAction_); });
    connect(alignRightAction_, &QAction::triggered,
            this, [this]() { onAlignmentChanged(alignRightAction_); });
    connect(alignJustifyAction_, &QAction::triggered,
            this, [this]() { onAlignmentChanged(alignJustifyAction_); });
}

void FormatToolbar::setEditor(PageView* editor)
{
    if (editor_) {
        disconnect(editor_, &PageView::currentCharFormatChanged,
                   this, &FormatToolbar::updateStateFromEditor);
        disconnect(editor_, &PageView::cursorPositionChanged,
                   this, &FormatToolbar::updateStateFromEditor);
    }

    editor_ = editor;

    if (editor_) {
        connect(editor_, &PageView::currentCharFormatChanged,
                this, &FormatToolbar::updateStateFromEditor);
        connect(editor_, &PageView::cursorPositionChanged,
                this, &FormatToolbar::updateStateFromEditor);
        updateStateFromEditor();
    }
}

void FormatToolbar::onFontFamilyChanged(const QString& family)
{
    if (!editor_) return;
    QTextCharFormat fmt;
    fmt.setFontFamilies({family});
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void FormatToolbar::onFontSizeChanged(const QString& sizeText)
{
    if (!editor_) return;
    bool ok = false;
    qreal size = sizeText.toDouble(&ok);
    if (!ok || size <= 0) return;
    QTextCharFormat fmt;
    fmt.setFontPointSize(size);
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void FormatToolbar::onBoldToggled(bool checked)
{
    if (!editor_) return;
    QTextCharFormat fmt;
    fmt.setFontWeight(checked ? QFont::Bold : QFont::Normal);
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void FormatToolbar::onItalicToggled(bool checked)
{
    if (!editor_) return;
    QTextCharFormat fmt;
    fmt.setFontItalic(checked);
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void FormatToolbar::onUnderlineToggled(bool checked)
{
    if (!editor_) return;
    QTextCharFormat fmt;
    fmt.setUnderlineStyle(checked ? QTextCharFormat::SingleUnderline
                                  : QTextCharFormat::NoUnderline);
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void FormatToolbar::onStrikeToggled(bool checked)
{
    if (!editor_) return;
    QTextCharFormat fmt;
    fmt.setFontStrikeOut(checked);
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void FormatToolbar::onTextColorClicked()
{
    if (!editor_) return;
    QColor color = QColorDialog::getColor(editor_->textColor(), this, "选择文字颜色");
    if (!color.isValid()) return;
    QTextCharFormat fmt;
    fmt.setForeground(color);
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void FormatToolbar::onBackgroundColorClicked()
{
    if (!editor_) return;
    QColor color = QColorDialog::getColor(editor_->textBackgroundColor(), this, "选择背景颜色");
    if (!color.isValid()) return;
    QTextCharFormat fmt;
    fmt.setBackground(color);
    fmt.setProperty(ShadingColorProperty, color.name().mid(1));
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void FormatToolbar::onHighlightSelected(QAction* action)
{
    if (!editor_) return;
    const QString value = action->data().toString();
    QTextCharFormat fmt;
    fmt.setProperty(HighlightColorProperty, value);
    if (value == "none") {
        // 保留取消标记，模型导出时忽略 none，避免合并格式后旧高亮仍被保存。
        fmt.clearBackground();
    } else {
        const QColor displayColor = highlightColorToQt(value);
        if (displayColor.isValid()) {
            fmt.setBackground(displayColor);
        }
    }
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void FormatToolbar::onBorderToggled(bool checked)
{
    if (!editor_) return;
    QTextCharFormat fmt;
    if (checked) {
        fmt.setProperty(BorderColorProperty, QStringLiteral("000000"));
    } else {
        fmt.setProperty(BorderColorProperty, QString());
    }
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void FormatToolbar::onEnclosedCircleToggled(bool checked)
{
    if (!editor_) return;
    QTextCharFormat fmt;
    fmt.setProperty(EnclosedCircleProperty, checked);
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void FormatToolbar::onChangeCaseSelected(QAction* action)
{
    if (!editor_) return;
    if (action == upperCaseAction_) {
        editor_->applyTextCase(PageView::TextCaseTransform::Upper);
    } else if (action == lowerCaseAction_) {
        editor_->applyTextCase(PageView::TextCaseTransform::Lower);
    } else if (action == titleCaseAction_) {
        editor_->applyTextCase(PageView::TextCaseTransform::Title);
    }
    editor_->setFocus();
}

void FormatToolbar::onAlignmentChanged(QAction* action)
{
    if (!editor_) return;
    Qt::Alignment align = Qt::AlignLeft;
    if (action == alignCenterAction_) {
        align = Qt::AlignHCenter;
    } else if (action == alignRightAction_) {
        align = Qt::AlignRight;
    } else if (action == alignJustifyAction_) {
        align = Qt::AlignJustify;
    }
    QTextBlockFormat blockFmt;
    blockFmt.setAlignment(align);
    QTextCursor cursor = editor_->textCursor();
    cursor.mergeBlockFormat(blockFmt);
    editor_->setTextCursor(cursor);
    editor_->setFocus();
}

void FormatToolbar::updateStateFromEditor()
{
    if (!editor_) return;

    QTextCharFormat fmt = editor_->currentCharFormat();

    // 粗体
    boldAction_->blockSignals(true);
    boldAction_->setChecked(fmt.fontWeight() >= QFont::Bold);
    boldAction_->blockSignals(false);

    // 斜体
    italicAction_->blockSignals(true);
    italicAction_->setChecked(fmt.fontItalic());
    italicAction_->blockSignals(false);

    // 下划线
    underlineAction_->blockSignals(true);
    underlineAction_->setChecked(fmt.underlineStyle() != QTextCharFormat::NoUnderline);
    underlineAction_->blockSignals(false);

    // 删除线
    strikeAction_->blockSignals(true);
    strikeAction_->setChecked(fmt.fontStrikeOut());
    strikeAction_->blockSignals(false);

    // 字体
    fontCombo_->blockSignals(true);
    const QStringList families = fmt.property(QTextFormat::FontFamilies).toStringList();
    if (!families.isEmpty()) {
        fontCombo_->setCurrentFont(QFont(families.first()));
    }
    fontCombo_->blockSignals(false);

    // 字号
    fontSizeCombo_->blockSignals(true);
    if (fmt.hasProperty(QTextFormat::FontPointSize)) {
        fontSizeCombo_->setCurrentText(QString::number(fmt.fontPointSize()));
    }
    fontSizeCombo_->blockSignals(false);

    // 字符边框
    borderAction_->blockSignals(true);
    const QVariant borderProp = fmt.property(BorderColorProperty);
    borderAction_->setChecked(borderProp.isValid() && !borderProp.toString().isEmpty());
    borderAction_->blockSignals(false);

    // 带圈字符
    enclosedCircleAction_->blockSignals(true);
    enclosedCircleAction_->setChecked(fmt.property(EnclosedCircleProperty).toBool());
    enclosedCircleAction_->blockSignals(false);

    // 对齐状态
    QTextBlockFormat blockFmt = editor_->textCursor().blockFormat();
    alignLeftAction_->blockSignals(true);
    alignCenterAction_->blockSignals(true);
    alignRightAction_->blockSignals(true);
    alignJustifyAction_->blockSignals(true);

    Qt::Alignment align = blockFmt.alignment();
    if (align & Qt::AlignHCenter) {
        alignCenterAction_->setChecked(true);
    } else if (align & Qt::AlignRight) {
        alignRightAction_->setChecked(true);
    } else if (align & Qt::AlignJustify) {
        alignJustifyAction_->setChecked(true);
    } else {
        alignLeftAction_->setChecked(true);
    }

    alignLeftAction_->blockSignals(false);
    alignCenterAction_->blockSignals(false);
    alignRightAction_->blockSignals(false);
    alignJustifyAction_->blockSignals(false);
}
