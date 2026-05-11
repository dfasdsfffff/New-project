//! @file FormatToolbar.cpp 格式工具栏实现

#include "FormatToolbar.hpp"
#include "PageView.hpp"

#include <QFontDatabase>
#include <QColorDialog>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QTextList>
#include <QActionGroup>
#include <QToolButton>
#include <QWidgetAction>
#include <QHBoxLayout>
#include <QPen>

namespace {

constexpr int HighlightColorProperty = QTextFormat::UserProperty + 101;
constexpr int BorderColorProperty = QTextFormat::UserProperty + 102;
constexpr int EnclosedCircleProperty = QTextFormat::UserProperty + 103;
constexpr int ShadingColorProperty [[maybe_unused]] = QTextFormat::UserProperty + 104;

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

    // 字体选择：使用 QFontComboBox，预设简体中文字体
    addWidget(new QLabel("字体 "));
    fontCombo_ = new QFontComboBox(this);
    fontCombo_->setMinimumWidth(160);
    fontCombo_->setWritingSystem(QFontDatabase::SimplifiedChinese);
    addWidget(fontCombo_);

    // 字号选择：可编辑下拉框，预置常用字号，默认 12pt
    addSeparator();
    addWidget(new QLabel("字号 "));
    fontSizeCombo_ = new QComboBox(this);
    fontSizeCombo_->setEditable(true);
    fontSizeCombo_->setMinimumWidth(60);
    QStringList sizes;
    sizes << "8" << "9" << "10" << "10.5" << "11" << "12" << "14" << "16"
          << "18" << "20" << "22" << "24" << "26" << "28" << "36" << "48" << "72";
    fontSizeCombo_->addItems(sizes);
    fontSizeCombo_->setCurrentText("12");
    addWidget(fontSizeCombo_);

    // 样式按钮组：粗体/斜体/下划线/删除线，均为可切换状态
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

    strikeAction_ = addAction("~~S~~");
    strikeAction_->setCheckable(true);
    strikeAction_->setToolTip("删除线");

    // 颜色按钮组：文字颜色和背景色，点击弹出颜色选择对话框
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

    borderAction_ = addAction("□");
    borderAction_->setCheckable(true);
    borderAction_->setToolTip("字符边框");

    enclosedCircleAction_ = addAction("○");
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

    // 对齐按钮组：使用 QActionGroup 保持互斥选择
    addSeparator();

    QActionGroup* alignGroup = new QActionGroup(this);
    alignGroup->setExclusive(true);

    alignLeftAction_ = addAction("≡");
    alignLeftAction_->setCheckable(true);
    alignLeftAction_->setToolTip("左对齐");
    alignLeftAction_->setChecked(true);
    alignGroup->addAction(alignLeftAction_);

    alignCenterAction_ = addAction("≡");
    alignCenterAction_->setCheckable(true);
    alignCenterAction_->setToolTip("居中");
    alignGroup->addAction(alignCenterAction_);

    alignRightAction_ = addAction("≡");
    alignRightAction_->setCheckable(true);
    alignRightAction_->setToolTip("右对齐");
    alignGroup->addAction(alignRightAction_);

    alignJustifyAction_ = addAction("≡");
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

    // 高亮、字符边框和带圈字符
    connect(highlightMenu_, &QMenu::triggered,
            this, &FormatToolbar::onHighlightSelected);
    connect(borderAction_, &QAction::toggled,
            this, &FormatToolbar::onBorderToggled);
    connect(enclosedCircleAction_, &QAction::toggled,
            this, &FormatToolbar::onEnclosedCircleToggled);

    // 大小写转换
    connect(upperCaseAction_, &QAction::triggered,
            this, [this]() { onChangeCaseSelected(upperCaseAction_); });
    connect(lowerCaseAction_, &QAction::triggered,
            this, [this]() { onChangeCaseSelected(lowerCaseAction_); });
    connect(titleCaseAction_, &QAction::triggered,
            this, [this]() { onChangeCaseSelected(titleCaseAction_); });

    // 对齐按钮：通过 lambda 中转以复用 onAlignmentChanged 统一处理
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
    editor_->setFocus();  // 操作后将焦点还给编辑器，便于继续输入
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

/**
 * 同步工具栏状态到当前编辑器光标处的格式
 * 为避免信号循环导致死循环，在更新 UI 前临时阻断控件信号
 */
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

    // 同步字符边框和带圈字符状态
    borderAction_->blockSignals(true);
    const QVariant borderProp = fmt.property(BorderColorProperty);
    borderAction_->setChecked(borderProp.isValid() && !borderProp.toString().isEmpty());
    borderAction_->blockSignals(false);

    enclosedCircleAction_->blockSignals(true);
    enclosedCircleAction_->setChecked(fmt.property(EnclosedCircleProperty).toBool());
    enclosedCircleAction_->blockSignals(false);

    // 同步对齐状态：清除各按钮信号后设置当前对齐方式
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
