//! @file PageView.cpp 分页文档视图实现

#include "PageView.hpp"

#include <QPainter>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextFrame>
#include <QTextLayout>
#include <QAbstractTextDocumentLayout>
#include <QGuiApplication>
#include <QClipboard>
#include <QStyle>
#include <QStyleOption>
#include <QApplication>

#include <utility>

namespace {

QString toTitleCase(QString text)
{
    bool nextWord = true;
    for (QChar& ch : text) {
        if (ch.isLetter()) {
            ch = nextWord ? ch.toUpper() : ch.toLower();
            nextWord = false;
        } else {
            nextWord = ch.isSpace() || ch.isPunct();
        }
    }
    return text;
}

QString transformCase(QString text, PageView::TextCaseTransform transform)
{
    switch (transform) {
    case PageView::TextCaseTransform::Upper:
        return text.toUpper();
    case PageView::TextCaseTransform::Lower:
        return text.toLower();
    case PageView::TextCaseTransform::Title:
        return toTitleCase(std::move(text));
    }
    return text;
}

} // namespace

// ═══════════════════════════════════════════════════════════════
// 构造与析构
// ═══════════════════════════════════════════════════════════════

PageView::PageView(QWidget* parent)
    : QAbstractScrollArea(parent)
    , doc_(new QTextDocument(this))
    , pageSize_(PageSizes::A4())
{
    doc_->setPageSize(pageSize_);
    doc_->setDefaultFont(QFont("等线", 12));
    doc_->setUndoRedoEnabled(true);

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFocusPolicy(Qt::StrongFocus);
    viewport()->setMouseTracking(true);

    cursorBlinkTimer_ = new QTimer(this);
    cursorBlinkTimer_->setInterval(QApplication::cursorFlashTime() / 2);
    connect(cursorBlinkTimer_, &QTimer::timeout, this, [this]() {
        cursorVisible_ = !cursorVisible_;
        viewport()->update(cursorScreenRect().adjusted(-1, -1, 1, 1).toRect());
    });

    updateLayout();
}

// ═══════════════════════════════════════════════════════════════
// 页面设置
// ═══════════════════════════════════════════════════════════════

void PageView::setPageSize(QSizeF size)
{
    if (pageSize_ == size) return;
    pageSize_ = size;
    doc_->setPageSize(pageSize_);
    updateLayout();
    viewport()->update();
}

QSizeF PageView::pageSize() const { return pageSize_; }

void PageView::setDocumentMargins(qreal left, qreal top, qreal right, qreal bottom)
{
    pageMarginLeft_ = left;
    pageMarginTop_ = top;
    pageMarginRight_ = right;
    pageMarginBottom_ = bottom;

    QTextFrameFormat rootFmt = doc_->rootFrame()->frameFormat();
    rootFmt.setLeftMargin(left);
    rootFmt.setTopMargin(top);
    rootFmt.setRightMargin(right);
    rootFmt.setBottomMargin(bottom);
    doc_->rootFrame()->setFrameFormat(rootFmt);

    updateLayout();
    viewport()->update();
}

void PageView::setZoom(qreal zoom)
{
    if (qFuzzyCompare(zoom_, zoom)) return;
    zoom_ = qBound(0.2, zoom, 4.0);
    updateLayout();
    viewport()->update();
}

qreal PageView::zoom() const { return zoom_; }

qreal PageView::displayZoom() const { return effectiveZoom(); }

qreal PageView::effectiveZoom() const
{
    if (!qFuzzyIsNull(zoom_)) return zoom_;
    // 自适应宽度：缩放页面使其适配视口宽度（减去页边留白）
    qreal viewW = viewport()->width() - pageGap_ * 4;
    if (viewW <= 0) return 1.0;
    qreal pagePxW = pageSize_.width() * logicalDpiX() / 72.0;
    if (pagePxW <= 0) return 1.0;
    return qMax(0.1, viewW / pagePxW);
}

// ═══════════════════════════════════════════════════════════════
// QTextEdit 兼容接口
// ═══════════════════════════════════════════════════════════════

QTextDocument* PageView::document() const { return doc_; }

void PageView::setDocument(QTextDocument* doc)
{
    if (doc_ == doc) return;
    doc_ = doc;
    doc_->setParent(this);
    doc_->setPageSize(pageSize_);
    doc_->setUndoRedoEnabled(true);
    cursorPosition_ = 0;
    selectionAnchor_ = -1;

    connect(doc_, &QTextDocument::contentsChanged, this, [this]() {
        updateLayout();
        viewport()->update();
    });

    updateLayout();
    viewport()->update();
}

QTextCursor PageView::textCursor() const
{
    QTextCursor c(doc_);
    c.setPosition(cursorPosition_);
    if (selectionAnchor_ >= 0 && selectionAnchor_ != cursorPosition_) {
        c.setPosition(selectionAnchor_, QTextCursor::MoveAnchor);
        c.setPosition(cursorPosition_, QTextCursor::KeepAnchor);
    }
    return c;
}

void PageView::setTextCursor(const QTextCursor& cursor)
{
    cursorPosition_ = cursor.position();
    if (cursor.hasSelection()) {
        selectionAnchor_ = cursor.anchor();
    } else {
        selectionAnchor_ = -1;
    }
    ensureCursorVisible();
    startBlinking();
    viewport()->update();
}

QTextCharFormat PageView::currentCharFormat() const
{
    QTextCursor c(doc_);
    c.setPosition(cursorPosition_);
    return c.charFormat();
}

void PageView::mergeCurrentCharFormat(const QTextCharFormat& fmt)
{
    QTextCursor c(doc_);
    c.setPosition(cursorPosition_);
    if (selectionAnchor_ >= 0 && selectionAnchor_ != cursorPosition_) {
        c.setPosition(selectionAnchor_, QTextCursor::MoveAnchor);
        c.setPosition(cursorPosition_, QTextCursor::KeepAnchor);
    }
    c.mergeCharFormat(fmt);
    viewport()->update();
}

QColor PageView::textColor() const
{
    return currentCharFormat().foreground().color();
}

QColor PageView::textBackgroundColor() const
{
    return currentCharFormat().background().color();
}

void PageView::setAcceptRichText(bool accept) { acceptRichText_ = accept; }

void PageView::clear()
{
    doc_->clear();
    doc_->setDefaultFont(QFont("等线", 12));
    cursorPosition_ = 0;
    selectionAnchor_ = -1;
    updateLayout();
    viewport()->update();
}

QString PageView::toPlainText() const { return doc_->toPlainText(); }

void PageView::undo() { doc_->undo(); viewport()->update(); }

void PageView::redo() { doc_->redo(); viewport()->update(); }

void PageView::cut()
{
    QTextCursor c(doc_);
    if (selectionAnchor_ >= 0 && selectionAnchor_ != cursorPosition_) {
        c.setPosition(selectionAnchor_, QTextCursor::MoveAnchor);
        c.setPosition(cursorPosition_, QTextCursor::KeepAnchor);
    }
    if (c.hasSelection()) {
        QGuiApplication::clipboard()->setText(c.selectedText());
        c.removeSelectedText();
        cursorPosition_ = c.position();
        selectionAnchor_ = -1;
        viewport()->update();
    }
}

void PageView::copy()
{
    QTextCursor c(doc_);
    if (selectionAnchor_ >= 0 && selectionAnchor_ != cursorPosition_) {
        c.setPosition(selectionAnchor_, QTextCursor::MoveAnchor);
        c.setPosition(cursorPosition_, QTextCursor::KeepAnchor);
    }
    if (c.hasSelection()) {
        QGuiApplication::clipboard()->setText(c.selectedText());
    }
}

void PageView::paste()
{
    QString text = QGuiApplication::clipboard()->text();
    if (!text.isEmpty()) {
        QTextCursor c(doc_);
        if (selectionAnchor_ >= 0 && selectionAnchor_ != cursorPosition_) {
            c.setPosition(selectionAnchor_, QTextCursor::MoveAnchor);
            c.setPosition(cursorPosition_, QTextCursor::KeepAnchor);
            c.removeSelectedText();
        }
        c.setPosition(cursorPosition_);
        c.insertText(text);
        cursorPosition_ = c.position();
        selectionAnchor_ = -1;
        viewport()->update();
    }
}

void PageView::selectAll()
{
    selectionAnchor_ = 0;
    cursorPosition_ = doc_->characterCount() - 1;
    viewport()->update();
}

void PageView::applyTextCase(TextCaseTransform transform)
{
    QTextCursor cursor(doc_);
    if (selectionAnchor_ >= 0 && selectionAnchor_ != cursorPosition_) {
        cursor.setPosition(selectionAnchor_, QTextCursor::MoveAnchor);
        cursor.setPosition(cursorPosition_, QTextCursor::KeepAnchor);
    } else {
        cursor.setPosition(cursorPosition_);
        cursor.select(QTextCursor::WordUnderCursor);
    }

    if (!cursor.hasSelection()) {
        return;
    }

    const QString replacement = transformCase(cursor.selectedText(), transform);
    cursor.insertText(replacement, cursor.charFormat());
    cursorPosition_ = cursor.position();
    selectionAnchor_ = -1;
    updateLayout();
    viewport()->update();
    emit cursorPositionChanged();
    emit currentCharFormatChanged(currentCharFormat());
}

QTextBlockFormat PageView::blockFormat() const
{
    QTextCursor c(doc_);
    c.setPosition(cursorPosition_);
    return c.blockFormat();
}

void PageView::mergeBlockFormat(const QTextBlockFormat& fmt)
{
    QTextCursor c(doc_);
    c.setPosition(cursorPosition_);
    if (selectionAnchor_ >= 0 && selectionAnchor_ != cursorPosition_) {
        c.setPosition(selectionAnchor_, QTextCursor::MoveAnchor);
        c.setPosition(cursorPosition_, QTextCursor::KeepAnchor);
    }
    c.mergeBlockFormat(fmt);
    viewport()->update();
}

QRect PageView::cursorRect() const
{
    QRectF r = cursorScreenRect();
    return QRect(static_cast<int>(r.x()), static_cast<int>(r.y()),
                 static_cast<int>(r.width()), static_cast<int>(r.height()));
}

// ═══════════════════════════════════════════════════════════════
// 布局计算
// ═══════════════════════════════════════════════════════════════

void PageView::updateLayout()
{
    pageRects_.clear();
    if (!doc_) return;

    qreal z = effectiveZoom();
    qreal pageWidthPx = pageSize_.width() * z * logicalDpiX() / 72.0;
    qreal pageHeightPx = pageSize_.height() * z * logicalDpiY() / 72.0;

    int pageCount = doc_->pageCount();
    if (pageCount <= 0) pageCount = 1;

    qreal y = pageGap_;
    for (int i = 0; i < pageCount; ++i) {
        pageRects_.append(QRectF(0, y, pageWidthPx, pageHeightPx));
        y += pageHeightPx + pageGap_;
    }

    updateScrollBars();
    emit zoomChanged(z);
}

QRectF PageView::pageScreenRect(int pageIndex) const
{
    if (pageIndex < 0 || pageIndex >= pageRects_.size()) return {};
    QRectF r = pageRects_[pageIndex];

    // 计算水平居中偏移
    qreal viewW = viewport()->width();
    qreal centerX = (viewW - r.width()) / 2.0;
    if (centerX < pageGap_) centerX = pageGap_;

    r.translate(centerX - horizontalScrollBar()->value(),
                -verticalScrollBar()->value());
    return r;
}

int PageView::pageAtScreenPos(const QPoint& pos) const
{
    int scrollX = horizontalScrollBar()->value();
    int scrollY = verticalScrollBar()->value();
    QPoint adjusted(pos.x() + scrollX, pos.y() + scrollY);

    qreal viewW = viewport()->width();
    qreal centerX = (viewW - (pageSize_.width() * effectiveZoom() * logicalDpiX() / 72.0)) / 2.0;
    if (centerX < pageGap_) centerX = pageGap_;

    for (int i = 0; i < pageRects_.size(); ++i) {
        QRectF r = pageRects_[i];
        r.translate(centerX, 0);
        if (r.contains(adjusted)) return i;
    }
    return -1;
}

QPointF PageView::screenToDocument(const QPoint& screenPos) const
{
    int page = pageAtScreenPos(screenPos);
    if (page < 0) return {0, static_cast<qreal>(cursorPosition_)};

    QRectF pageRect = pageScreenRect(page);
    qreal z = effectiveZoom();
    qreal scaleX = logicalDpiX() / (72.0 * z);
    qreal scaleY = logicalDpiY() / (72.0 * z);

    qreal docX = (screenPos.x() - pageRect.x()) * scaleX;
    qreal docY = page * pageSize_.height() + (screenPos.y() - pageRect.y()) * scaleY;
    return {docX, docY};
}

QPointF PageView::documentToScreen(const QPointF& docPos) const
{
    int page = static_cast<int>(docPos.y() / pageSize_.height());
    if (page < 0 || page >= pageRects_.size()) return {0, 0};

    QRectF pageRect = pageScreenRect(page);
    qreal z = effectiveZoom();
    qreal scaleX = z * logicalDpiX() / 72.0;
    qreal scaleY = z * logicalDpiY() / 72.0;

    qreal screenX = pageRect.x() + docPos.x() * scaleX;
    qreal screenY = pageRect.y() + (docPos.y() - page * pageSize_.height()) * scaleY;
    return {screenX, screenY};
}

QTextCursor PageView::cursorForScreenPos(const QPoint& screenPos) const
{
    int page = pageAtScreenPos(screenPos);
    if (page < 0) return QTextCursor(const_cast<QTextDocument*>(doc_));

    QPointF docPos = screenToDocument(screenPos);
    int pos = doc_->documentLayout()->hitTest(docPos, Qt::FuzzyHit);
    QTextCursor cursor(const_cast<QTextDocument*>(doc_));
    cursor.setPosition(pos);
    return cursor;
}

QRectF PageView::cursorScreenRect() const
{
    QTextCursor cursor(doc_);
    cursor.setPosition(cursorPosition_);

    QTextBlock block = cursor.block();
    if (!block.isValid()) return {};

    QTextLayout* layout = block.layout();
    if (!layout) return {};

    int posInBlock = cursorPosition_ - block.position();
    QTextLine line = layout->lineForTextPosition(posInBlock);
    if (!line.isValid()) return {};

    QRectF blockRect = doc_->documentLayout()->blockBoundingRect(block);
    qreal cursorX = blockRect.x() + line.cursorToX(posInBlock);
    qreal cursorY = blockRect.y() + line.y();

    QPointF screenPos = documentToScreen({cursorX, cursorY});
    return QRectF(screenPos.x(), screenPos.y(), 1.5, line.height());
}

void PageView::updateScrollBars()
{
    if (pageRects_.isEmpty()) {
        verticalScrollBar()->setRange(0, 0);
        horizontalScrollBar()->setRange(0, 0);
        return;
    }

    qreal totalWidth = pageRects_.last().width() + pageGap_ * 4;
    qreal totalHeight = pageRects_.last().bottom() + pageGap_;
    QSize vs = viewport()->size();

    verticalScrollBar()->setRange(0, qMax(0, static_cast<int>(totalHeight - vs.height())));
    verticalScrollBar()->setPageStep(vs.height());
    verticalScrollBar()->setSingleStep(30);

    int hRange = static_cast<int>(totalWidth - vs.width());
    horizontalScrollBar()->setRange(0, qMax(0, hRange));
    horizontalScrollBar()->setPageStep(vs.width());
    horizontalScrollBar()->setSingleStep(30);
}

void PageView::ensureCursorVisible()
{
    QRectF cursorR = cursorScreenRect();
    if (cursorR.isNull()) return;

    QRect viewRect = viewport()->rect();
    int sx = horizontalScrollBar()->value();
    int sy = verticalScrollBar()->value();

    if (cursorR.right() > viewRect.right())
        sx += static_cast<int>(cursorR.right() - viewRect.right()) + 20;
    else if (cursorR.left() < viewRect.left())
        sx -= static_cast<int>(viewRect.left() - cursorR.left()) + 20;
    if (cursorR.bottom() > viewRect.bottom())
        sy += static_cast<int>(cursorR.bottom() - viewRect.bottom()) + 20;
    else if (cursorR.top() < viewRect.top())
        sy -= static_cast<int>(viewRect.top() - cursorR.top()) + 20;

    horizontalScrollBar()->setValue(sx);
    verticalScrollBar()->setValue(sy);
}

// ═══════════════════════════════════════════════════════════════
// 绘制
// ═══════════════════════════════════════════════════════════════

void PageView::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);

    // 灰色背景
    painter.fillRect(viewport()->rect(), QColor(180, 180, 180));

    int scrollX = horizontalScrollBar()->value();
    int scrollY = verticalScrollBar()->value();

    // 水平居中偏移
    qreal viewW = viewport()->width();
    qreal z = effectiveZoom();
    qreal pageW = pageSize_.width() * z * logicalDpiX() / 72.0;
    qreal centerX = (viewW - pageW) / 2.0;
    if (centerX < pageGap_) centerX = pageGap_;

    for (int i = 0; i < pageRects_.size(); ++i) {
        QRectF pageRect = pageRects_[i];
        QRectF screenRect = pageRect.translated(centerX - scrollX, -scrollY);

        if (!viewport()->rect().intersects(
                screenRect.toRect().adjusted(-20, -20, 20, 20)))
            continue;

        // 阴影
        painter.fillRect(screenRect.translated(shadowWidth_, shadowWidth_),
                        QColor(120, 120, 120, 100));

        // 白色页面
        painter.fillRect(screenRect, Qt::white);

        // 页面内容
        qreal scaleX = z * logicalDpiX() / 72.0;
        qreal scaleY = z * logicalDpiY() / 72.0;

        painter.save();
        painter.translate(screenRect.topLeft());
        painter.scale(scaleX, scaleY);

        QRectF clipRect(0, 0, pageSize_.width(), pageSize_.height());
        painter.setClipRect(clipRect);

        QAbstractTextDocumentLayout::PaintContext ctx;
        ctx.clip = clipRect;
        ctx.cursorPosition = cursorPosition_;

        // 选择区域高亮
        if (selectionAnchor_ >= 0 && selectionAnchor_ != cursorPosition_) {
            QTextCursor selCursor(doc_);
            selCursor.setPosition(selectionAnchor_, QTextCursor::MoveAnchor);
            selCursor.setPosition(cursorPosition_, QTextCursor::KeepAnchor);

            QTextCharFormat selFmt;
            selFmt.setBackground(QColor(51, 153, 255, 80));
            QAbstractTextDocumentLayout::Selection sel;
            sel.cursor = selCursor;
            sel.format = selFmt;
            ctx.selections.append(sel);
        }

        // 文档坐标 Y 轴：第 i 页起始于 y=i*pageHeight
        painter.translate(0, -i * pageSize_.height());
        doc_->documentLayout()->draw(&painter, ctx);
        painter.restore();

        // 页面边框
        painter.setPen(QPen(QColor(200, 200, 200), 1));
        painter.drawRect(screenRect);
    }

    drawCursor(painter);
}

void PageView::drawCursor(QPainter& painter)
{
    if (!cursorVisible_ || !hasFocus()) return;
    if (selectionAnchor_ >= 0 && selectionAnchor_ != cursorPosition_) return;

    QRectF sr = cursorScreenRect();
    if (sr.isNull()) return;

    painter.setPen(QPen(Qt::black, 1.5));
    painter.drawLine(QPointF(sr.x(), sr.y()),
                     QPointF(sr.x(), sr.y() + sr.height()));
}

// ═══════════════════════════════════════════════════════════════
// 事件处理
// ═══════════════════════════════════════════════════════════════

void PageView::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateLayout();
}

void PageView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QAbstractScrollArea::mousePressEvent(event);
        return;
    }

    setFocus();
    mousePressPos_ = event->pos();

    QTextCursor hitCursor = cursorForScreenPos(event->pos());
    int pos = hitCursor.position();

    if (event->modifiers() & Qt::ShiftModifier) {
        // Shift+点击扩展选择
        selectionAnchor_ = (selectionAnchor_ >= 0) ? selectionAnchor_ : cursorPosition_;
        cursorPosition_ = pos;
    } else {
        cursorPosition_ = pos;
        selectionAnchor_ = -1;
    }

    selecting_ = true;
    startBlinking();
    emit cursorPositionChanged();
    emit currentCharFormatChanged(currentCharFormat());
    viewport()->update();
}

void PageView::mouseMoveEvent(QMouseEvent* event)
{
    mouseMovePos_ = event->pos();

    if (selecting_ && (event->buttons() & Qt::LeftButton)) {
        QTextCursor hitCursor = cursorForScreenPos(event->pos());
        int pos = hitCursor.position();
        if (selectionAnchor_ < 0) selectionAnchor_ = cursorPosition_;
        cursorPosition_ = pos;
        emit cursorPositionChanged();
        viewport()->update();
    }

    if (pageAtScreenPos(event->pos()) >= 0)
        viewport()->setCursor(Qt::IBeamCursor);
    else
        viewport()->setCursor(Qt::ArrowCursor);
}

void PageView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        selecting_ = false;
    QAbstractScrollArea::mouseReleaseEvent(event);
}

void PageView::mouseDoubleClickEvent(QMouseEvent* event)
{
    QTextCursor wc = cursorForScreenPos(event->pos());
    wc.select(QTextCursor::WordUnderCursor);
    cursorPosition_ = wc.position();
    selectionAnchor_ = wc.anchor();
    emit cursorPositionChanged();
    viewport()->update();
}

void PageView::keyPressEvent(QKeyEvent* event)
{
    QTextCursor cursor(doc_);
    // 先确保光标反映当前选择和位置
    if (selectionAnchor_ >= 0 && selectionAnchor_ != cursorPosition_) {
        cursor.setPosition(selectionAnchor_, QTextCursor::MoveAnchor);
        cursor.setPosition(cursorPosition_, QTextCursor::KeepAnchor);
    } else {
        cursor.setPosition(cursorPosition_);
    }

    bool shift = event->modifiers() & Qt::ShiftModifier;
    QTextCursor::MoveMode mode = shift ? QTextCursor::KeepAnchor
                                       : QTextCursor::MoveAnchor;

    switch (event->key()) {
    case Qt::Key_Left:
        cursor.movePosition(QTextCursor::Left, mode);
        break;
    case Qt::Key_Right:
        cursor.movePosition(QTextCursor::Right, mode);
        break;
    case Qt::Key_Up:
        cursor.movePosition(QTextCursor::Up, mode);
        break;
    case Qt::Key_Down:
        cursor.movePosition(QTextCursor::Down, mode);
        break;
    case Qt::Key_Home:
        cursor.movePosition(
            (event->modifiers() & Qt::ControlModifier) ? QTextCursor::Start
                                                        : QTextCursor::StartOfLine,
            mode);
        break;
    case Qt::Key_End:
        cursor.movePosition(
            (event->modifiers() & Qt::ControlModifier) ? QTextCursor::End
                                                        : QTextCursor::EndOfLine,
            mode);
        break;
    case Qt::Key_PageUp:
        cursor.movePosition(QTextCursor::Up, QTextCursor::MoveAnchor, 20);
        break;
    case Qt::Key_PageDown:
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, 20);
        break;
    case Qt::Key_Backspace:
    case Qt::Key_Delete:
        if (cursor.hasSelection()) {
            cursor.removeSelectedText();
        } else if (event->key() == Qt::Key_Backspace) {
            cursor.deletePreviousChar();
        } else {
            cursor.deleteChar();
        }
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        cursor.insertBlock();
        break;
    case Qt::Key_Tab:
        cursor.insertText("\t");
        break;
    default:
        if (!event->text().isEmpty() && event->text().at(0).isPrint()) {
            cursor.insertText(event->text());
        } else {
            QAbstractScrollArea::keyPressEvent(event);
            return;
        }
        break;
    }

    cursorPosition_ = cursor.position();
    selectionAnchor_ = cursor.hasSelection() ? cursor.anchor() : -1;

    startBlinking();
    ensureCursorVisible();
    updateScrollBars();
    viewport()->update();

    emit cursorPositionChanged();
    emit currentCharFormatChanged(currentCharFormat());
    event->accept();
}

void PageView::inputMethodEvent(QInputMethodEvent* event)
{
    if (!event->commitString().isEmpty()) {
        QTextCursor cursor(doc_);
        cursor.setPosition(cursorPosition_);
        cursor.insertText(event->commitString());
        cursorPosition_ = cursor.position();
        selectionAnchor_ = -1;
        viewport()->update();
    }
    event->accept();
}

QVariant PageView::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch (query) {
    case Qt::ImCursorRectangle:
        return cursorScreenRect();
    case Qt::ImCursorPosition:
        return cursorPosition_;
    case Qt::ImSurroundingText:
        return doc_->toPlainText();
    case Qt::ImCurrentSelection: {
        QTextCursor c(doc_);
        if (selectionAnchor_ >= 0 && selectionAnchor_ != cursorPosition_) {
            c.setPosition(selectionAnchor_, QTextCursor::MoveAnchor);
            c.setPosition(cursorPosition_, QTextCursor::KeepAnchor);
        }
        return c.selectedText();
    }
    default:
        break;
    }
    return QVariant();
}

void PageView::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        qreal delta = event->angleDelta().y() > 0 ? 0.1 : -0.1;
        setZoom(zoom_ + delta);
        return;
    }
    QAbstractScrollArea::wheelEvent(event);
}

void PageView::focusInEvent(QFocusEvent* event)
{
    QAbstractScrollArea::focusInEvent(event);
    startBlinking();
    viewport()->update();
}

void PageView::focusOutEvent(QFocusEvent* event)
{
    QAbstractScrollArea::focusOutEvent(event);
    stopBlinking();
    viewport()->update();
}

bool PageView::event(QEvent* e)
{
    if (e->type() == QEvent::KeyPress) {
        QKeyEvent* ke = static_cast<QKeyEvent*>(e);
        if (ke->modifiers() & Qt::ControlModifier) {
            switch (ke->key()) {
            case Qt::Key_Z: undo(); return true;
            case Qt::Key_Y: redo(); return true;
            case Qt::Key_C: copy(); return true;
            case Qt::Key_V: paste(); return true;
            case Qt::Key_X: cut(); return true;
            case Qt::Key_A: selectAll(); return true;
            default: break;
            }
        }
    }
    return QAbstractScrollArea::event(e);
}

// ═══════════════════════════════════════════════════════════════
// 光标闪烁
// ═══════════════════════════════════════════════════════════════

void PageView::startBlinking()
{
    cursorVisible_ = true;
    if (!cursorBlinkTimer_->isActive())
        cursorBlinkTimer_->start();
}

void PageView::stopBlinking()
{
    cursorBlinkTimer_->stop();
    cursorVisible_ = false;
}
