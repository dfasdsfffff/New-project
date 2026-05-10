//! @file PageView.hpp 分页文档视图，替代 QTextEdit 实现按页显示和编辑

#pragma once

#include <QAbstractScrollArea>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QTextFrame>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QInputMethodEvent>
#include <QWheelEvent>
#include <QTimer>
#include <QSizeF>
#include <QVector>
#include <QRectF>

/**
 * @brief 分页文档视图
 *
 * 基于 QAbstractScrollArea 的文档编辑器，以分页方式渲染 QTextDocument，
 * 支持设置页面尺寸、边距、缩放比例，处理鼠标/键盘输入以支持编辑操作。
 *
 * 对外暴露 QTextEdit 兼容的常用方法，可作为 QTextEdit 的直接替代。
 */
class PageView : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit PageView(QWidget* parent = nullptr);

signals:
    /// 光标位置变化时触发，供 FormatToolbar 同步格式状态
    void cursorPositionChanged();
    /// 当前字符格式变化时触发
    void currentCharFormatChanged(const QTextCharFormat& format);

public:
    // ---- 页面设置 ----
    /// @brief 设置页面尺寸（单位：磅，1pt = 1/72 inch）
    void setPageSize(QSizeF size);
    QSizeF pageSize() const;

    /// @brief 设置文档边距（单位：磅）
    void setDocumentMargins(qreal left, qreal top, qreal right, qreal bottom);

    /// @brief 设置缩放比例（1.0 = 100%）
    void setZoom(qreal zoom);
    qreal zoom() const;

    // ---- QTextEdit 兼容接口 ----
    QTextDocument* document() const;
    void setDocument(QTextDocument* doc);

    QTextCursor textCursor() const;
    void setTextCursor(const QTextCursor& cursor);

    QTextCharFormat currentCharFormat() const;
    void mergeCurrentCharFormat(const QTextCharFormat& fmt);

    QColor textColor() const;
    QColor textBackgroundColor() const;
    void setAcceptRichText(bool accept);
    void clear();

    QString toPlainText() const;

    // 编辑操作（委托给 QTextCursor/QTextDocument）
    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    void selectAll();

    // 块格式操作
    QTextBlockFormat blockFormat() const;
    void mergeBlockFormat(const QTextBlockFormat& fmt);

    // 光标矩形（用于弹出菜单等定位）
    QRect cursorRect() const;

protected:
    // 绘制
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    // 鼠标事件
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

    // 键盘事件
    void keyPressEvent(QKeyEvent* event) override;

    // 输入法
    void inputMethodEvent(QInputMethodEvent* event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

    // 滚轮
    void wheelEvent(QWheelEvent* event) override;

    // 焦点
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

    bool event(QEvent* e) override;

private:
    // ---- 布局计算 ----
    /// 重新计算页面布局（页面矩形、滚动范围），在页面尺寸/缩放变化时调用
    void updateLayout();
    /// 获取第 pageIndex 页在视口中的屏幕矩形
    QRectF pageScreenRect(int pageIndex) const;
    /// 获取指定屏幕坐标所在的页码（-1 表示不在任何页面上）
    int pageAtScreenPos(const QPoint& pos) const;
    /// 将屏幕坐标映射为文档内部坐标（QTextDocument 布局坐标）
    QPointF screenToDocument(const QPoint& screenPos) const;
    /// 将文档内部坐标映射为屏幕坐标
    QPointF documentToScreen(const QPointF& docPos) const;
    /// 根据屏幕坐标获取文档中的光标位置
    QTextCursor cursorForScreenPos(const QPoint& screenPos) const;

    // ---- 光标渲染 ----
    /// 获取光标的屏幕矩形
    QRectF cursorScreenRect() const;
    /// 绘制文本光标
    void drawCursor(QPainter& painter);
    /// 启动/停止光标闪烁定时器
    void startBlinking();
    void stopBlinking();

    // ---- 滚动 ----
    /// 根据文档位置更新滚动条
    void updateScrollBars();
    /// 确保光标在可见区域内
    void ensureCursorVisible();

    // 内部 QTextDocument
    QTextDocument* doc_;

    // 页面参数
    QSizeF pageSize_;       ///< 页面尺寸（磅）
    qreal pageMarginLeft_ = 72;    ///< 左边距（磅）
    qreal pageMarginTop_ = 72;     ///< 上边距（磅）
    qreal pageMarginRight_ = 72;   ///< 右边距（磅）
    qreal pageMarginBottom_ = 72;  ///< 下边距（磅）
    qreal zoom_ = 1.0;     ///< 缩放比例

    // 布局缓存
    QVector<QRectF> pageRects_;   ///< 每页的屏幕矩形
    qreal pageGap_ = 20;           ///< 页间间距（像素）
    qreal shadowWidth_ = 4;        ///< 页面阴影宽度（像素）

    // 光标和选择
    QTimer* cursorBlinkTimer_;
    bool cursorVisible_ = true;
    int cursorPosition_ = 0;      ///< 当前光标在文档中的字符位置
    QPoint mousePressPos_;
    QPoint mouseMovePos_;
    bool selecting_ = false;
    int selectionAnchor_ = -1;    ///< 选择起始位置（-1 表示无选择）

    // 富文本开关
    bool acceptRichText_ = true;
};

/// 常用标准页面尺寸（单位：磅）
namespace PageSizes {
    inline constexpr QSizeF A4()     { return {595.28, 841.89}; }
    inline constexpr QSizeF A3()     { return {841.89, 1190.55}; }
    inline constexpr QSizeF A5()     { return {419.53, 595.28}; }
    inline constexpr QSizeF Letter() { return {612.0, 792.0}; }
    inline constexpr QSizeF Legal()  { return {612.0, 1008.0}; }
}
