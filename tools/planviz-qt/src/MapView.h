#pragma once

#include "PlanData.h"

#include <QPaintEvent>
#include <QPointF>
#include <QWidget>

class MapView : public QWidget {
    Q_OBJECT

public:
    explicit MapView(QWidget* parent = nullptr);

    void setPlanData(PlanData* planData);
    void setTick(int tick);
    void resetView();
    void setShowAgentIds(bool show);
    void setShowAllGoalArrows(bool show);
    void setShowDebugOverlays(bool show);
    void setShowOrzBottlenecks(bool show);
    void setShowOrzParkingAreas(bool show);
    void setShowDebugDirections(bool show);
    void setShowHighwayDirections(bool show);
    void setShowDirectionMap(bool show);
    void setShowHeatMap(bool show);
    void setShowWaitMap(bool show);
    void setShowHeatOutlines(bool show);
    void setShowGuidePath(bool show);
    void setShowStagedLocs(bool show);
    void setHighlightFreeAgents(bool show);
    void setShowFreeTasks(bool show);
    void setShowDelayedAgents(bool show);
    void setShowCoordinates(bool show);
    void setShowPibtTrace(bool show);
    void setSelectedAgent(int agentId);
    void centerOnAgent(int agentId);
    void setHighlightedTask(int taskId);
    int selectedAgent() const { return m_selectedAgent; }

signals:
    void selectedAgentChanged(int agentId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QPointF worldToScreen(const QPointF& world) const;
    QPointF screenToWorld(const QPointF& screen) const;
    QRectF visibleWorldRect() const;
    int agentAtScreen(const QPointF& screen) const;
    void drawDebugOverlays(QPainter& painter, const QRectF& visible);
    void drawOrzBottlenecks(QPainter& painter, const QRectF& visible);
    void drawOrzParkingAreas(QPainter& painter, const QRectF& visible);
    void drawDebugDirections(QPainter& painter, const QRectF& visible);
    void drawHighwayDirections(QPainter& painter, const QRectF& visible);
    void drawDirectionMap(QPainter& painter, const QRectF& visible);
    void drawHeatMap(QPainter& painter, int minRow, int maxRow, int minCol, int maxCol);
    void drawWaitMap(QPainter& painter, int minRow, int maxRow, int minCol, int maxCol);
    void drawHeatClusterOutlines(QPainter& painter, const QRectF& visible);
    void drawCoordinates(QPainter& painter, int minRow, int maxRow, int minCol, int maxCol);
    void drawPibtTrace(QPainter& painter, const QRectF& visible);
    void drawFreeTasks(QPainter& painter, const QRectF& visible);
    void drawSelectedAgentGuidePath(QPainter& painter, const QRectF& visible);
    void drawSelectedAgentStagedLocs(QPainter& painter, const QRectF& visible);
    void drawSelectedAgentRewriteMarker(QPainter& painter, const QRectF& visible);
    void drawHighlightedTaskErrands(QPainter& painter, const QRectF& visible);
    void drawSelectedAgentPath(QPainter& painter, const QRectF& visible);
    void drawAllAgentGoalArrows(QPainter& painter, const QRectF& visible);
    void drawSelectedAgentGoalArrow(QPainter& painter);
    void drawAgentGoalArrow(QPainter& painter, int agent, const QColor& color, double alphaScale);
    void drawAgentHeading(QPainter& painter, const QPointF& center, double radius, double dir, bool selected);

    PlanData* m_data = nullptr;
    int m_tick = 0;
    double m_scale = 12.0;
    QPointF m_offset = {20.0, 20.0};
    QPoint m_lastMouse;
    QPoint m_pressPos;
    int m_selectedAgent = -1;
    int m_highlightedTask = -1;
    bool m_showAgentIds = false;
    bool m_showAllGoalArrows = false;
    bool m_showDebugOverlays = true;
    bool m_showOrzBottlenecks = true;
    bool m_showOrzParkingAreas = true;
    bool m_showDebugDirections = true;
    bool m_showHighwayDirections = true;
    bool m_showDirectionMap = true;
    bool m_showHeatMap = false;
    bool m_showWaitMap = false;
    bool m_showHeatOutlines = false;
    bool m_showGuidePath = true;
    bool m_showStagedLocs = true;
    bool m_highlightFreeAgents = true;
    bool m_showFreeTasks = false;
    bool m_showDelayedAgents = true;
    bool m_showCoordinates = false;
    bool m_showPibtTrace = true;
    bool m_fitOnNextResize = false;
};
