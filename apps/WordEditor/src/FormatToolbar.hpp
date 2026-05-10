#pragma once

#include <QToolBar>
#include <QFontComboBox>
#include <QComboBox>
#include <QAction>
#include <QTextEdit>
#include <QColor>
#include <QLabel>

class FormatToolbar : public QToolBar {
    Q_OBJECT
public:
    explicit FormatToolbar(QWidget* parent = nullptr);

    void setEditor(QTextEdit* editor);

private slots:
    void onFontFamilyChanged(const QString& family);
    void onFontSizeChanged(const QString& sizeText);
    void onBoldToggled(bool checked);
    void onItalicToggled(bool checked);
    void onUnderlineToggled(bool checked);
    void onStrikeToggled(bool checked);
    void onTextColorClicked();
    void onBackgroundColorClicked();
    void onAlignmentChanged(QAction* action);
    void updateStateFromEditor();

private:
    void setupUi();
    void setupConnections();

    QFontComboBox* fontCombo_;
    QComboBox* fontSizeCombo_;
    QAction* boldAction_;
    QAction* italicAction_;
    QAction* underlineAction_;
    QAction* strikeAction_;
    QAction* textColorAction_;
    QAction* backgroundColorAction_;
    QAction* alignLeftAction_;
    QAction* alignCenterAction_;
    QAction* alignRightAction_;
    QAction* alignJustifyAction_;

    QTextEdit* editor_ = nullptr;
};
