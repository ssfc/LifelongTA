#pragma once

#include "HighwayEditorData.h"

#include <QHash>
#include <QPointF>
#include <QWidget>

class HighwayEditorView : public QWidget {
    Q_OBJECT

public:
    explicit HighwayEditorView(QWidget* parent = nullptr);

    void setData(HighwayEditorData* data);
    void setLayer(HighwayEditorData::Layer layer);
    void setManualMask(int mask);
    void setShowHighway(bool show);
    void resetView();
    void fitOrzTube();

signals:
    void hoveredCellChanged(int row, int col);
    void statsChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QPointF worldToScreen(const QPointF& world) const;
    QPointF screenToWorld(const QPointF& screen) const;
    QRectF visibleWorldRect() const;
    QPoint cellAt(const QPointF& screen) const;
    void paintCellAt(const QPointF& screen);
    void finishUndoStroke();
    void undoLastStroke();
    void drawArrow(QPainter& painter, const QPointF& center, int dir, double length, double head, const QColor& color) const;
    void drawManualPenalty(QPainter& painter, int row, int col, const QRectF& rect, int mask) const;
    void drawHighwayDirections(QPainter& painter, const QRectF& visible) const;

    HighwayEditorData* m_data = nullptr;
    HighwayEditorData::Layer m_layer = HighwayEditorData::Layer::ManualPenalty;
    int m_manualMask = HighwayEditorData::dirMask(0);
    bool m_showHighway = true;
    double m_scale = 10.0;
    QPointF m_offset = {20.0, 20.0};
    QPoint m_lastMouse;
    bool m_draggingPaint = false;
    bool m_draggingPan = false;
    bool m_fitOnResize = false;
    QSet<quint64> m_paintedThisStroke;
    QHash<quint64, HighwayEditorData::CellState> m_currentUndoStroke;
    QVector<QHash<quint64, HighwayEditorData::CellState>> m_undoStack;
};
