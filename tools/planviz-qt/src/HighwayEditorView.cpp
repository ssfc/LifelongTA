#include "HighwayEditorView.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {
QPointF unitForDir(int dir)
{
    switch (HighwayEditorData::normalizeDir(dir)) {
    case 0:
        return {1.0, 0.0};
    case 1:
        return {0.0, 1.0};
    case 2:
        return {-1.0, 0.0};
    default:
        return {0.0, -1.0};
    }
}
}

HighwayEditorView::HighwayEditorView(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(false);
}

void HighwayEditorView::setData(HighwayEditorData* editorData)
{
    m_data = editorData;
    m_fitOnResize = true;
    resetView();
}

void HighwayEditorView::setLayer(HighwayEditorData::Layer layer)
{
    m_layer = layer;
}

void HighwayEditorView::setManualMask(int mask)
{
    m_manualMask = mask;
}

void HighwayEditorView::setShowHighway(bool show)
{
    m_showHighway = show;
    update();
}

void HighwayEditorView::resetView()
{
    if (!m_data || !m_data->hasMap() || width() <= 0 || height() <= 0) {
        update();
        return;
    }
    const double sx = (width() - 40.0) / std::max(1, m_data->cols());
    const double sy = (height() - 40.0) / std::max(1, m_data->rows());
    m_scale = std::clamp(std::min(sx, sy), 0.25, 32.0);
    m_offset = {20.0, 20.0};
    update();
}

void HighwayEditorView::fitOrzTube()
{
    if (!m_data || !m_data->hasMap()) {
        return;
    }
    const QRectF tube(925.0, 535.0, 121.0, 56.0);
    const double sx = (width() - 40.0) / tube.width();
    const double sy = (height() - 40.0) / tube.height();
    m_scale = std::clamp(std::min(sx, sy), 0.25, 48.0);
    m_offset = {20.0 - tube.left() * m_scale, 20.0 - tube.top() * m_scale};
    update();
}

QPointF HighwayEditorView::worldToScreen(const QPointF& world) const
{
    return {world.x() * m_scale + m_offset.x(), world.y() * m_scale + m_offset.y()};
}

QPointF HighwayEditorView::screenToWorld(const QPointF& screen) const
{
    return {(screen.x() - m_offset.x()) / m_scale, (screen.y() - m_offset.y()) / m_scale};
}

QRectF HighwayEditorView::visibleWorldRect() const
{
    const QPointF topLeft = screenToWorld({0.0, 0.0});
    const QPointF bottomRight = screenToWorld({static_cast<double>(width()), static_cast<double>(height())});
    return QRectF(topLeft, bottomRight).normalized();
}

QPoint HighwayEditorView::cellAt(const QPointF& screen) const
{
    const QPointF world = screenToWorld(screen);
    return {static_cast<int>(std::floor(world.x())), static_cast<int>(std::floor(world.y()))};
}

void HighwayEditorView::paintCellAt(const QPointF& screen)
{
    if (!m_data) {
        return;
    }
    const QPoint cell = cellAt(screen);
    const int row = cell.y();
    const int col = cell.x();
    if (!m_data->inBounds(row, col)) {
        return;
    }
    const quint64 k = HighwayEditorData::key(row, col);
    if (m_paintedThisStroke.contains(k)) {
        return;
    }
    m_paintedThisStroke.insert(k);
    if (!m_currentUndoStroke.contains(k)) {
        m_currentUndoStroke.insert(k, m_data->cellStateAt(row, col));
    }
    m_data->paint(row, col, m_layer, m_manualMask);
    emit statsChanged();
    update();
}

void HighwayEditorView::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(233, 237, 242));
    if (!m_data || !m_data->hasMap()) {
        painter.setPen(QColor(70, 80, 95));
        painter.drawText(rect(), Qt::AlignCenter, "Load a .map file to begin.");
        return;
    }

    const QRectF visible = visibleWorldRect();
    const int minCol = std::max(0, static_cast<int>(std::floor(visible.left())) - 1);
    const int maxCol = std::min(m_data->cols() - 1, static_cast<int>(std::ceil(visible.right())) + 1);
    const int minRow = std::max(0, static_cast<int>(std::floor(visible.top())) - 1);
    const int maxRow = std::min(m_data->rows() - 1, static_cast<int>(std::ceil(visible.bottom())) + 1);

    painter.setRenderHint(QPainter::Antialiasing, m_scale >= 8.0);
    for (int row = minRow; row <= maxRow; ++row) {
        for (int col = minCol; col <= maxCol; ++col) {
            const QPointF screen = worldToScreen({static_cast<double>(col), static_cast<double>(row)});
            const QRectF cellRect(screen.x(), screen.y(), m_scale, m_scale);
            painter.fillRect(cellRect, m_data->blocked(row, col) ? QColor(35, 40, 49) : QColor(245, 248, 252));
            const int mask = m_data->manualMaskAt(row, col);

            if (mask != 0) {
                drawManualPenalty(painter, row, col, cellRect, mask);
            } else if (m_data->hasPrimary(row, col)) {
                painter.fillRect(cellRect.adjusted(1, 1, -1, -1), QColor(255, 214, 0, 115));
                painter.setPen(QPen(QColor(210, 170, 0), std::max(1.0, m_scale * 0.08)));
                painter.drawRect(cellRect.adjusted(1, 1, -1, -1));
            } else if (m_data->hasSecondary(row, col)) {
                painter.fillRect(cellRect.adjusted(1, 1, -1, -1), QColor(41, 121, 255, 80));
                painter.setPen(QPen(QColor(41, 121, 255), std::max(1.0, m_scale * 0.08)));
                painter.drawRect(cellRect.adjusted(1, 1, -1, -1));
            } else if (m_data->hasUpperParking(row, col)) {
                painter.fillRect(cellRect.adjusted(1, 1, -1, -1), QColor(0, 150, 136, 80));
                painter.setPen(QPen(QColor(0, 150, 136), std::max(1.0, m_scale * 0.08), Qt::DashLine));
                painter.drawRect(cellRect.adjusted(1, 1, -1, -1));
            } else if (m_data->hasLowerParking(row, col)) {
                painter.fillRect(cellRect.adjusted(1, 1, -1, -1), QColor(156, 39, 176, 78));
                painter.setPen(QPen(QColor(156, 39, 176), std::max(1.0, m_scale * 0.08), Qt::DashLine));
                painter.drawRect(cellRect.adjusted(1, 1, -1, -1));
            }
        }
    }

    if (m_showHighway) {
        drawHighwayDirections(painter, visible);
    }

    if (m_scale >= 5.0) {
        painter.setPen(QPen(QColor(204, 213, 224), 1));
        for (int row = minRow; row <= maxRow + 1; ++row) {
            const QPointF a = worldToScreen({static_cast<double>(minCol), static_cast<double>(row)});
            const QPointF b = worldToScreen({static_cast<double>(maxCol + 1), static_cast<double>(row)});
            painter.drawLine(a, b);
        }
        for (int col = minCol; col <= maxCol + 1; ++col) {
            const QPointF a = worldToScreen({static_cast<double>(col), static_cast<double>(minRow)});
            const QPointF b = worldToScreen({static_cast<double>(col), static_cast<double>(maxRow + 1)});
            painter.drawLine(a, b);
        }
    }

    if (m_scale >= 18.0) {
        painter.setPen(QColor(92, 108, 130));
        QFont font = painter.font();
        font.setPointSizeF(std::clamp(m_scale * 0.18, 7.0, 11.0));
        painter.setFont(font);
        for (int row = minRow; row <= maxRow; ++row) {
            for (int col = minCol; col <= maxCol; ++col) {
                const QPointF s = worldToScreen({col + 0.08, row + 0.58});
                painter.drawText(s, QString("%1,%2").arg(row).arg(col));
            }
        }
    }
}

void HighwayEditorView::mousePressEvent(QMouseEvent* event)
{
    m_lastMouse = event->pos();
    if (event->button() == Qt::LeftButton) {
        m_draggingPaint = true;
        m_paintedThisStroke.clear();
        m_currentUndoStroke.clear();
        paintCellAt(event->position());
    } else if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        m_draggingPan = true;
        setCursor(Qt::ClosedHandCursor);
    }
}

void HighwayEditorView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_data) {
        const QPoint cell = cellAt(event->position());
        emit hoveredCellChanged(cell.y(), cell.x());
    }
    if (m_draggingPaint) {
        paintCellAt(event->position());
    } else if (m_draggingPan) {
        const QPoint delta = event->pos() - m_lastMouse;
        m_offset += QPointF(delta);
        update();
    }
    m_lastMouse = event->pos();
}

void HighwayEditorView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_draggingPaint = false;
        m_paintedThisStroke.clear();
        finishUndoStroke();
    } else if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton) {
        m_draggingPan = false;
        unsetCursor();
    }
}

void HighwayEditorView::wheelEvent(QWheelEvent* event)
{
    if (!m_data || !m_data->hasMap()) {
        return;
    }
    const QPointF before = screenToWorld(event->position());
    const double factor = event->angleDelta().y() > 0 ? 1.18 : 1.0 / 1.18;
    m_scale = std::clamp(m_scale * factor, 0.2, 80.0);
    m_offset = event->position() - QPointF(before.x() * m_scale, before.y() * m_scale);
    update();
}

void HighwayEditorView::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Undo)) {
        undoLastStroke();
        event->accept();
        return;
    }

    if (!m_data || !m_data->hasMap()) {
        QWidget::keyPressEvent(event);
        return;
    }

    double step = std::clamp(std::min(width(), height()) * 0.18, 32.0, 220.0);
    if (event->modifiers() & Qt::ShiftModifier) {
        step *= 2.5;
    }
    if (event->modifiers() & Qt::ControlModifier) {
        step *= 0.35;
    }

    QPointF delta(0.0, 0.0);
    switch (event->key()) {
    case Qt::Key_W:
        delta.setY(step);
        break;
    case Qt::Key_S:
        delta.setY(-step);
        break;
    case Qt::Key_A:
        delta.setX(step);
        break;
    case Qt::Key_D:
        delta.setX(-step);
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }

    m_offset += delta;
    event->accept();
    update();
}

void HighwayEditorView::finishUndoStroke()
{
    if (m_currentUndoStroke.isEmpty()) {
        return;
    }
    m_undoStack.push_back(m_currentUndoStroke);
    m_currentUndoStroke.clear();
    constexpr int maxUndoSteps = 200;
    if (m_undoStack.size() > maxUndoSteps) {
        m_undoStack.erase(m_undoStack.begin(), m_undoStack.begin() + (m_undoStack.size() - maxUndoSteps));
    }
}

void HighwayEditorView::undoLastStroke()
{
    if (!m_data) {
        return;
    }
    if (m_draggingPaint) {
        m_draggingPaint = false;
        m_paintedThisStroke.clear();
        finishUndoStroke();
    }
    if (m_undoStack.isEmpty()) {
        return;
    }
    const auto stroke = m_undoStack.takeLast();
    for (auto it = stroke.cbegin(); it != stroke.cend(); ++it) {
        const QPoint cell = HighwayEditorData::cellFromKey(it.key());
        m_data->restoreCellState(cell.y(), cell.x(), it.value());
    }
    emit statsChanged();
    update();
}

void HighwayEditorView::resizeEvent(QResizeEvent*)
{
    if (m_fitOnResize) {
        m_fitOnResize = false;
        resetView();
    }
}

void HighwayEditorView::drawArrow(QPainter& painter, const QPointF& center, int dir, double length, double head, const QColor& color) const
{
    painter.save();
    const QPointF u = unitForDir(dir);
    const QPointF n(-u.y(), u.x());
    const QPointF a = center - u * length * 0.45;
    const QPointF b = center + u * length * 0.45;
    const double stroke = std::clamp(m_scale * 0.10, 1.0, 3.0);
    painter.setPen(QPen(color, stroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(color);
    painter.drawLine(a, b);

    if (m_scale >= 4.0) {
        QPolygonF arrowHead;
        arrowHead << b
                  << (b - u * head + n * head * 0.55)
                  << (b - u * head - n * head * 0.55);
        painter.drawPolygon(arrowHead);
    }
    painter.restore();
}

void HighwayEditorView::drawManualPenalty(QPainter& painter, int, int, const QRectF& rect, int mask) const
{
    painter.fillRect(rect.adjusted(1, 1, -1, -1), QColor(245, 248, 252));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 73, 45, 225), std::max(1.5, m_scale * 0.075), Qt::DashLine));
    painter.drawRect(rect.adjusted(1, 1, -1, -1));
    const QPointF center = rect.center();
    int count = 0;
    for (int dir = 0; dir < 4; ++dir) {
        if (mask & HighwayEditorData::dirMask(dir)) {
            ++count;
        }
    }
    const QColor arrowColor(0, 126, 145, 120);
    const double length = count == 1 ? std::clamp(m_scale * 0.55, 5.0, 18.0)
                                     : std::clamp(m_scale * 0.42, 4.0, 13.0);
    const double head = count == 1 ? std::clamp(m_scale * 0.18, 2.5, 6.0)
                                   : std::clamp(m_scale * 0.13, 2.0, 4.5);
    const double offset = std::clamp(m_scale * 0.20, 2.0, 7.0);
    for (int dir = 0; dir < 4; ++dir) {
        if (!(mask & HighwayEditorData::dirMask(dir))) {
            continue;
        }
        const int displayDir = HighwayEditorData::normalizeDir(dir + 2);
        const QPointF unit = unitForDir(displayDir);
        const QPointF shifted = count == 1 ? center : center + unit * offset;
        drawArrow(painter, shifted, displayDir, length, head, arrowColor);
    }
}

void HighwayEditorView::drawHighwayDirections(QPainter& painter, const QRectF& visible) const
{
    if (!m_data) {
        return;
    }
    const QColor color(0, 126, 145, 130);
    QVector<const HighwayArrowMarker*> visibleDirections;
    visibleDirections.reserve(m_data->highwayDirections().size());
    for (const HighwayArrowMarker& h : m_data->highwayDirections()) {
        if (h.col < visible.left() - 1 || h.col > visible.right() + 1 ||
            h.row < visible.top() - 1 || h.row > visible.bottom() + 1) {
            continue;
        }
        if (m_data->manualMaskAt(h.row, h.col) != 0) {
            continue;
        }
        visibleDirections.push_back(&h);
    }
    std::sort(visibleDirections.begin(), visibleDirections.end(), [](const HighwayArrowMarker* a, const HighwayArrowMarker* b) {
        if (a->row != b->row) {
            return a->row < b->row;
        }
        if (a->col != b->col) {
            return a->col < b->col;
        }
        return a->dir < b->dir;
    });

    int first = 0;
    while (first < visibleDirections.size()) {
        int last = first + 1;
        const int row = visibleDirections[first]->row;
        const int col = visibleDirections[first]->col;
        while (last < visibleDirections.size() &&
               visibleDirections[last]->row == row &&
               visibleDirections[last]->col == col) {
            ++last;
        }

        const int count = last - first;
        const QPointF baseCenter = worldToScreen({col + 0.5, row + 0.5});
        if (count == 1) {
            drawArrow(painter, baseCenter, visibleDirections[first]->dir,
                      std::clamp(m_scale * 0.55, 5.0, 18.0),
                      std::clamp(m_scale * 0.18, 2.5, 6.0), color);
        } else {
            const double length = std::clamp(m_scale * 0.42, 4.0, 13.0);
            const double head = std::clamp(m_scale * 0.13, 2.0, 4.5);
            const double offset = std::clamp(m_scale * 0.20, 2.0, 7.0);
            for (int i = first; i < last; ++i) {
                const QPointF unit = unitForDir(visibleDirections[i]->dir);
                drawArrow(painter, baseCenter + unit * offset, visibleDirections[i]->dir,
                          length, head, color);
            }
        }
        first = last;
    }
}
