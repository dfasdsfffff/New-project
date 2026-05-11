//! @file FormatToolbar.cpp 格式工具栏实现。
#include "FormatToolbar.hpp"
#include "PageView.hpp"

#include <QActionGroup>
#include <QColorDialog>
#include <QFontDatabase>
#include <QLabel>
#include <QMenu>
#include <QPen>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextList>
#include <QToolButton>

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
    if (value == "red") return QColor("#ff0000");
    if (value == "blue") return QColor("#0000ff");
    return {};
}

} // namespace

FormatToolbar::FormatToolbar(QWidget* parent)
    : QToolBar("格式", parent)
{
    setupUi();
    setupConnections();
}

void FormatToolbar::setupUi()
{
    setMovable(false);
    setIconSize(QSize(16, 16));

    addWidget(new QLabel("字体 "));
    fontCombo_ = new QFontComboBox(this);
    fontCombo_->setMinimumWidth(160);
    fontCombo_->setWritingSystem(QFontDatabase::SimplifiedChinese);
    addWidget(fontCombo_);

    addSeparator();
    addWidget(new QLabel("字号 "));
    fontSizeCombo_ = new QComboBox(this);
    fontSizeCombo_->setEditable(true);
    fontSizeCombo_->setMinimumWidth(60);
    fontSizeCombo_->addItems({
        "8", "9", "10", "10.5", "11", "12", "14", "16",
        "18", "20", "22", "24", "26", "28", "36", "48", "72"
    });
    fontSizeCombo_->setCurrentText("12");
    addWidget(fontSizeCombo_);

    addSeparator();
    boldAction_ = addAction("B");
    boldAction_->setCheckable(true);
    boldAction_->setToolTip("加粗 (Ctrl+B)");
    QFont boldFont = boldAction_->font();
    boldFont.setBold(true);
    boldAction_->setFont(boldFont);

    italicAction_ = addAction("I");
    italicAction_->setCheckable(true);
    italicAction_->setToolTip("斜体 (Ctrl+I)");
    QFont italicFont = italicAction_->font();
    italicFont.setItalic(true);
    italicAction_->setFont(italicFont);

    underlineAction_ = addAction("U");
    underlineAction_->setCheckable(true);
    underlineAction_->setToolTip("下划线 (Ctrl+U)");
    QFont ulFont = underlineAction_->font();
    ulFont.setUnderline(true);
    underlineAction_->setFont(ulFont);

    strikeAction_ = addAction("S");
    strikeAction_->setCheckable(true);
    strikeAction_->setToolTip("删除线");
    QFont strikeFont = strikeAction_->font();
    strikeFont.setStrikeOut(true);
    strikeAction_->setFont(strikeFont);

    addSeparator();
    textColorAction_ = addAction("A");
    textColorAction_->setToolTip("文字颜色");
    QFont colorFont = textColorAction_->font();
    colorFont.setBold(true);
    textColorAction_->setFont(colorFont);

    backgroundColorAction_ = addAction("底");
    backgroundColorAction_->setToolTip("字符底纹");
    QFont bgFont = backgroundColorAction_->font();
    bgFont.setBold(true);
    backgroundColorAction_->setFont(bgFont);

    highlightMenu_ = new QMenu(this);
    addHighlightColor(*highlightMenu_, "黄色", "yellow");
    addHighlightColor(*highlightMenu_, "绿色", "green");
    addHighlightColor(*highlightMenu_, "青色", "cyan");
    addHighlightColor(*highlightMenu_, "红色", "red");
    addHighlightColor(*highlightMenu_, "蓝色", "blue");
    addHighlightColor(*highlightMenu_, "无高亮", "none");
    highlightAction_ = addAction("H");
    highlightAction_->setToolTip("高亮显示");
    if (auto* button = qobject_cast<QToolButton*>(widgetForAction(highlightAction_))) {
        button->setMenu(highlightMenu_);
        button->setPopupMode(QToolButton::InstantPopup);
    }

    borderAction_ = addAction("框");
    borderAction_->setCheckable(true);
    borderAction_->setToolTip("字符边框");

    enclosedCircleAction_ = addAction("圈");
    enclosedCircleAction_->setCheckable(true);
    enclosedCircleAction_->setToolTip("带圈字符");

    changeCaseMenu_ = new QMenu(this);
    upperCaseAction_ = changeCaseMenu_->addAction("大写");
    lowerCaseAction_ = changeCaseMenu_->addAction("小写");
    titleCaseAction_ = changeCaseMenu_->addAction("首字母大写");
    QAction* changeCaseAction = addAction("Aa");
    changeCaseAction->setToolTip("更改大小写");
    if (auto* button = qobject_cast<QToolButton*>(widgetForAction(changeCaseAction))) {
        button->setMenu(changeCaseMenu_);
        button->setPopupMode(QToolButton::InstantPopup);
    }

    addSeparator();
    QActionGroup* alignGroup = new QActionGroup(this);
    alignGroup->setExclusive(true);

    alignLeftAction_ = addAction("左");
    alignLeftAction_->setCheckable(true);
    alignLeftAction_->setToolTip("左对齐");
    alignLeftAction_->setChecked(true);
    alignGroup->addAction(alignLeftAction_);

    alignCenterAction_ = addAction("中");
    alignCenterAction_->setCheckable(true);
    alignCenterAction_->setToolTip("居中");
    alignGroup->addAction(alignCenterAction_);

    alignRightAction_ = addAction("右");
    alignRightAction_->setCheckable(true);
    alignRightAction_->setToolTip("右对齐");
    alignGroup->addAction(alignRightAction_);

    alignJustifyAction_ = addAction("齐");
    alignJustifyAction_->setCheckable(true);
    alignJustifyAction_->setToolTip("两端对齐");
    alignGroup->addAction(alignJustifyAction_);
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
    connect(changeCaseMenu_, &QMenu::triggered,
            this, &FormatToolbar::onChangeCaseSelected);

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

    QColor color = QColorDialog::getColor(editor_->textBackgroundColor(), this, "选择字符底纹");
    if (!color.isValid()) return;

    QTextCharFormat fmt;
    fmt.setBackground(color);
    fmt.setProperty(ShadingColorProperty, color.name().mid(1));
    editor_->mergeCurrentCharFormat(fmt);
    editor_->setFocus();
}

void FormatToolbar::onHighlightSelected(QAction* action)
{
    if (!editor_ || !action) return;

    const QString value = action->data().toString();
    QTextCharFormat fmt;
    fmt.setProperty(HighlightColorProperty, value);
    if (value == "none") {
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
        const QColor color = editor_->textColor().isValid() ? editor_->textColor() : Qt::black;
        fmt.setProperty(BorderColorProperty, color.name().mid(1));
        fmt.setTextOutline(QPen(color, 0.6));
    } else {
        fmt.setProperty(BorderColorProperty, QString());
        fmt.setTextOutline(QPen(Qt::NoPen));
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
    if (!editor_ || !action) return;

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

    borderAction_->blockSignals(true);
    borderAction_->setChecked(fmt.hasProperty(BorderColorProperty) &&
                              !fmt.property(BorderColorProperty).toString().isEmpty());
    borderAction_->blockSignals(false);

    enclosedCircleAction_->blockSignals(true);
    enclosedCircleAction_->setChecked(fmt.property(EnclosedCircleProperty).toBool());
    enclosedCircleAction_->blockSignals(false);

    fontCombo_->blockSignals(true);
    const QStringList families = fmt.property(QTextFormat::FontFamilies).toStringList();
    if (!families.isEmpty()) {
        fontCombo_->setCurrentFont(QFont(families.first()));
    }
    fontCombo_->blockSignals(false);

    fontSizeCombo_->blockSignals(true);
    if (fmt.hasProperty(QTextFormat::FontPointSize)) {
        fontSizeCombo_->setCurrentText(QString::number(fmt.fontPointSize()));
    }
    fontSizeCombo_->blockSignals(false);

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
