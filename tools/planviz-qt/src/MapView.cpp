#include "MapView.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <queue>

namespace {

bool isCornerRewrite(const QStringList& plannerActions, const QStringList& stagedActions)
{
    if (plannerActions.size() < 4 || stagedActions.size() < 3) {
        return false;
    }

    const bool plannerTurnsOppose =
        (plannerActions[0] == "CR" && plannerActions[2] == "CCR") ||
        (plannerActions[0] == "CCR" && plannerActions[2] == "CR");
    return plannerTurnsOppose &&
           plannerActions[1] == "FW" &&
           plannerActions[3] == "FW" &&
           stagedActions[0] == "FW" &&
           stagedActions[1] == plannerActions[0] &&
           stagedActions[2] == "FW";
}

QColor heatColor(double value)
{
    value = std::clamp(value, 0.0, 1.0);
    struct Stop {
        double x;
        QColor color;
    };
    static const Stop stops[] = {
        {0.00, QColor(48, 80, 170)},
        {0.28, QColor(42, 165, 205)},
        {0.55, QColor(255, 224, 92)},
        {0.78, QColor(245, 135, 58)},
        {1.00, QColor(205, 46, 48)},
    };
    const Stop* left = &stops[0];
    const Stop* right = &stops[std::size(stops) - 1];
    for (size_t i = 1; i < std::size(stops); ++i) {
        if (value <= stops[i].x) {
            left = &stops[i - 1];
            right = &stops[i];
            break;
        }
    }
    const double span = std::max(0.0001, right->x - left->x);
    const double t = (value - left->x) / span;
    const QColor& a = left->color;
    const QColor& b = right->color;
    const int red = static_cast<int>(std::round(a.red() + (b.red() - a.red()) * t));
    const int green = static_cast<int>(std::round(a.green() + (b.green() - a.green()) * t));
    const int blue = static_cast<int>(std::round(a.blue() + (b.blue() - a.blue()) * t));
    const int alpha = static_cast<int>(std::round(45.0 + 130.0 * value));
    return QColor(red, green, blue, alpha);
}

QColor waitColor(double value)
{
    value = std::clamp(value, 0.0, 1.0);
    struct Stop {
        double x;
        QColor color;
    };
    static const Stop stops[] = {
        {0.00, QColor(235, 245, 255)},
        {0.25, QColor(145, 205, 255)},
        {0.55, QColor(118, 82, 220)},
        {0.78, QColor(220, 42, 170)},
        {1.00, QColor(170, 0, 55)},
    };
    const Stop* left = &stops[0];
    const Stop* right = &stops[std::size(stops) - 1];
    for (size_t i = 1; i < std::size(stops); ++i) {
        if (value <= stops[i].x) {
            left = &stops[i - 1];
            right = &stops[i];
            break;
        }
    }
    const double span = std::max(0.0001, right->x - left->x);
    const double t = (value - left->x) / span;
    const QColor& a = left->color;
    const QColor& b = right->color;
    const int red = static_cast<int>(std::round(a.red() + (b.red() - a.red()) * t));
    const int green = static_cast<int>(std::round(a.green() + (b.green() - a.green()) * t));
    const int blue = static_cast<int>(std::round(a.blue() + (b.blue() - a.blue()) * t));
    const int alpha = static_cast<int>(std::round(35.0 + 160.0 * value));
    return QColor(red, green, blue, alpha);
}

} // namespace

MapView::MapView(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void MapView::setPlanData(PlanData* planData)
{
    m_data = planData;
    m_fitOnNextResize = true;
    if (m_selectedAgent != -1) {
        m_selectedAgent = -1;
        emit selectedAgentChanged(m_selectedAgent);
    }
    resetView();
}

void MapView::setTick(int tick)
{
    m_tick = tick;
    update();
}

void MapView::resetView()
{
    if (!m_data || m_data->width() <= 0 || m_data->height() <= 0) {
        return;
    }
    const double sx = width() > 0 ? (width() - 40.0) / m_data->width() : 1.0;
    const double sy = height() > 0 ? (height() - 40.0) / m_data->height() : 1.0;
    m_scale = std::clamp(std::min(sx, sy), 1.0, 24.0);
    m_offset = {20.0, 20.0};
    update();
}

void MapView::setShowAgentIds(bool show)
{
    m_showAgentIds = show;
    update();
}

void MapView::setShowAllGoalArrows(bool show)
{
    m_showAllGoalArrows = show;
    update();
}

void MapView::setShowDebugOverlays(bool show)
{
    m_showDebugOverlays = show;
    update();
}

void MapView::setShowOrzBottlenecks(bool show)
{
    m_showOrzBottlenecks = show;
    update();
}

void MapView::setShowOrzParkingAreas(bool show)
{
    m_showOrzParkingAreas = show;
    update();
}

void MapView::setShowDebugDirections(bool show)
{
    m_showDebugDirections = show;
    update();
}

void MapView::setShowHighwayDirections(bool show)
{
    m_showHighwayDirections = show;
    update();
}

void MapView::setShowDirectionMap(bool show)
{
    m_showDirectionMap = show;
    update();
}

void MapView::setShowHeatMap(bool show)
{
    m_showHeatMap = show;
    if (m_showHeatMap && m_data) {
        m_data->ensureOccupancyHeat();
    }
    update();
}

void MapView::setShowWaitMap(bool show)
{
    m_showWaitMap = show;
    if (m_showWaitMap && m_data) {
        m_data->ensureWaitHeat();
    }
    update();
}

void MapView::setShowHeatOutlines(bool show)
{
    m_showHeatOutlines = show;
    if (m_showHeatOutlines && m_data) {
        m_data->ensureOccupancyHeat();
    }
    update();
}

void MapView::setShowGuidePath(bool show)
{
    m_showGuidePath = show;
    update();
}

void MapView::setShowStagedLocs(bool show)
{
    m_showStagedLocs = show;
    update();
}

void MapView::setHighlightFreeAgents(bool show)
{
    m_highlightFreeAgents = show;
    update();
}

void MapView::setShowFreeTasks(bool show)
{
    m_showFreeTasks = show;
    update();
}

void MapView::setShowDelayedAgents(bool show)
{
    m_showDelayedAgents = show;
    update();
}

void MapView::setShowCoordinates(bool show)
{
    m_showCoordinates = show;
    update();
}

void MapView::setShowPibtTrace(bool show)
{
    m_showPibtTrace = show;
    update();
}

void MapView::setSelectedAgent(int agentId)
{
    const int nextSelected = (m_data && agentId >= 0 && agentId < m_data->teamSize()) ? agentId : -1;
    if (nextSelected == m_selectedAgent) {
        update();
        return;
    }
    m_selectedAgent = nextSelected;
    emit selectedAgentChanged(m_selectedAgent);
    update();
}

void MapView::centerOnAgent(int agentId)
{
    if (!m_data || agentId < 0 || agentId >= m_data->teamSize() || width() <= 0 || height() <= 0) {
        return;
    }

    const AgentFrame frame = m_data->frameAt(agentId, m_tick);
    const QPointF worldCenter(frame.cell.x() + 0.5, frame.cell.y() + 0.5);
    const QPointF screenCenter(width() * 0.5, height() * 0.5);
    m_offset = screenCenter - QPointF(worldCenter.x() * m_scale, worldCenter.y() * m_scale);
    update();
}

void MapView::setHighlightedTask(int taskId)
{
    m_highlightedTask = taskId;
    update();
}

void MapView::drawCoordinates(QPainter& painter, int minRow, int maxRow, int minCol, int maxCol)
{
    if (!m_showCoordinates || !m_data || m_scale < 20.0) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    QFont font = painter.font();
    font.setPointSizeF(std::clamp(m_scale * 0.18, 5.5, 9.0));
    font.setBold(false);
    painter.setFont(font);
    painter.setPen(QColor(72, 87, 105, 190));

    for (int row = minRow; row <= maxRow; ++row) {
        for (int col = minCol; col <= maxCol; ++col) {
            const QPointF topLeft = worldToScreen(QPointF(col, row));
            const QRectF cellRect(topLeft.x(), topLeft.y(), m_scale, m_scale);
            painter.drawText(cellRect.adjusted(1.0, 1.0, -1.0, -1.0),
                             Qt::AlignCenter,
                             QString("%1,%2").arg(row).arg(col));
        }
    }
    painter.restore();
}

void MapView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor(245, 247, 250));
    if (!m_data) {
        painter.drawText(rect(), Qt::AlignCenter, "No map loaded");
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing, m_scale >= 4.0);
    const QRectF visible = visibleWorldRect();
    const int minCol = std::max(0, static_cast<int>(std::floor(visible.left())));
    const int maxCol = std::min(m_data->width() - 1, static_cast<int>(std::ceil(visible.right())));
    const int minRow = std::max(0, static_cast<int>(std::floor(visible.top())));
    const int maxRow = std::min(m_data->height() - 1, static_cast<int>(std::ceil(visible.bottom())));

    const QVector<unsigned char>& cells = m_data->mapCells();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(34, 38, 46));
    for (int row = minRow; row <= maxRow; ++row) {
        for (int col = minCol; col <= maxCol; ++col) {
            if (cells[row * m_data->width() + col] != 0) {
                const QPointF p = worldToScreen(QPointF(col, row));
                painter.drawRect(QRectF(p.x(), p.y(), m_scale + 0.2, m_scale + 0.2));
            }
        }
    }

    drawHeatMap(painter, minRow, maxRow, minCol, maxCol);
    drawWaitMap(painter, minRow, maxRow, minCol, maxCol);

    if (m_scale >= 8.0) {
        painter.setPen(QPen(QColor(215, 221, 230), 1));
        for (int col = minCol; col <= maxCol + 1; ++col) {
            const QPointF a = worldToScreen(QPointF(col, minRow));
            const QPointF b = worldToScreen(QPointF(col, maxRow + 1));
            painter.drawLine(a, b);
        }
        for (int row = minRow; row <= maxRow + 1; ++row) {
            const QPointF a = worldToScreen(QPointF(minCol, row));
            const QPointF b = worldToScreen(QPointF(maxCol + 1, row));
            painter.drawLine(a, b);
        }
    }

    drawCoordinates(painter, minRow, maxRow, minCol, maxCol);
    drawDebugOverlays(painter, visible);
    drawOrzBottlenecks(painter, visible);
    drawOrzParkingAreas(painter, visible);
    drawHeatClusterOutlines(painter, visible);
    drawHighwayDirections(painter, visible);
    drawDirectionMap(painter, visible);
    drawDebugDirections(painter, visible);
    drawPibtTrace(painter, visible);
    drawFreeTasks(painter, visible);
    drawHighlightedTaskErrands(painter, visible);
    drawSelectedAgentGuidePath(painter, visible);
    drawSelectedAgentStagedLocs(painter, visible);
    drawSelectedAgentRewriteMarker(painter, visible);
    drawSelectedAgentPath(painter, visible);
    drawAllAgentGoalArrows(painter, visible);
    drawSelectedAgentGoalArrow(painter);

    if (m_selectedAgent >= 0 && m_selectedAgent < m_data->teamSize()) {
        const AgentFrame selectedFrame = m_data->frameAt(m_selectedAgent, m_tick);
        const int selectedCol = static_cast<int>(std::floor(selectedFrame.cell.x() + 0.5));
        const int selectedRow = static_cast<int>(std::floor(selectedFrame.cell.y() + 0.5));
        if (selectedRow >= 0 && selectedRow < m_data->height() &&
            selectedCol >= 0 && selectedCol < m_data->width()) {
            const int index = selectedRow * m_data->width() + selectedCol;
            const bool selectedCellFree =
                index >= 0 && index < m_data->mapCells().size() &&
                m_data->mapCells()[index] == 0;
            const QPointF topLeft = worldToScreen(QPointF(selectedCol, selectedRow));
            QRectF selectedRect(topLeft, QSizeF(m_scale, m_scale));
            QColor fill = selectedCellFree ? QColor(46, 204, 113, 64)
                                           : QColor(235, 72, 145, 88);
            QColor border = selectedCellFree ? QColor(46, 204, 113, 245)
                                             : QColor(235, 72, 145, 245);
            painter.setBrush(fill);
            painter.setPen(QPen(border, std::clamp(m_scale * 0.14, 2.0, 5.0)));
            painter.drawRect(selectedRect.adjusted(1.0, 1.0, -1.0, -1.0));
            painter.setPen(Qt::NoPen);
        }
    }

    painter.setPen(Qt::NoPen);
    const bool largeOrzMap = m_data->height() == 656 && m_data->width() == 1491;
    const double radiusFactor = largeOrzMap ? 0.30 : 0.38;
    const double radius = std::clamp(m_scale * radiusFactor, 2.0, std::min(18.0, m_scale * 0.46));
    for (int agent = 0; agent < m_data->teamSize(); ++agent) {
        const AgentFrame frame = m_data->frameAt(agent, m_tick);
        if (!visible.contains(QPointF(frame.cell.x() + 0.5, frame.cell.y() + 0.5))) {
            continue;
        }
        const QPointF center = worldToScreen(QPointF(frame.cell.x() + 0.5, frame.cell.y() + 0.5));
        const bool freeAgent = m_data->currentTaskForAgent(agent, m_tick) < 0;
        const bool delayedAgent = m_data->isAgentDelayed(agent, m_tick);
        const QColor baseColor = QColor::fromHsv((agent * 47) % 360, 80, 245);
        const QColor color = (m_highlightFreeAgents && freeAgent)
            ? QColor(0, 165, 150)
            : baseColor;
        painter.setBrush(color);
        painter.drawEllipse(center, radius, radius);
        if (m_showDelayedAgents && delayedAgent) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(235, 72, 45, 245), std::max(2.0, radius * 0.34)));
            painter.drawEllipse(center, radius + 3.0, radius + 3.0);
            painter.setPen(QPen(QColor(255, 218, 80, 235), std::max(1.0, radius * 0.16)));
            painter.drawEllipse(center, radius + 5.0, radius + 5.0);
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
        }
        if (m_highlightFreeAgents && freeAgent) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(255, 255, 255, 235), std::max(1.5, radius * 0.32)));
            painter.drawEllipse(center, radius * 0.78, radius * 0.78);
            painter.setPen(QPen(QColor(0, 95, 90, 230), std::max(1.0, radius * 0.16)));
            painter.drawEllipse(center, radius + 1.5, radius + 1.5);
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
        }
        if (agent == m_selectedAgent) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(160, 38, 190), std::max(2.0, radius * 0.28)));
            painter.drawEllipse(center, radius + 2.0, radius + 2.0);
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
        }

        if (m_data->hasAgentOrientations()) {
            drawAgentHeading(painter, center, radius, frame.dir, false);
        }

        if (m_showAgentIds && m_scale >= 10.0) {
            painter.setPen(Qt::black);
            QFont font = painter.font();
            font.setPointSizeF(std::clamp(radius * 1.15, 7.0, 24.0));
            font.setBold(radius >= 4.0);
            painter.setFont(font);
            const QString label = QString::number(agent);
            const QFontMetricsF metrics(font);
            const double textWidth = std::max(radius * 2.0, metrics.horizontalAdvance(label) + 4.0);
            const double textHeight = std::max(radius * 2.0, metrics.height());
            const QRectF textRect(center.x() - textWidth / 2.0,
                                  center.y() - textHeight / 2.0,
                                  textWidth,
                                  textHeight);
            painter.drawText(textRect, Qt::AlignCenter, label);
            painter.setPen(Qt::NoPen);
        }

        if (m_showDelayedAgents && delayedAgent && m_scale >= 10.0) {
            const QString delayLabel = "L";
            QFont font = painter.font();
            font.setPointSizeF(std::clamp(radius * 0.8, 6.0, 15.0));
            font.setBold(true);
            painter.setFont(font);
            const QFontMetricsF metrics(font);
            const double badgeW = std::max(radius * 1.45, metrics.horizontalAdvance(delayLabel) + 6.0);
            const double badgeH = std::max(radius * 1.05, metrics.height() * 0.85);
            const QRectF badge(center.x() - radius * 1.55,
                               center.y() + radius * 0.55,
                               badgeW,
                               badgeH);
            painter.setPen(QPen(Qt::white, 1.0));
            painter.setBrush(QColor(235, 72, 45, 235));
            painter.drawRoundedRect(badge, 3.0, 3.0);
            painter.setPen(Qt::white);
            painter.drawText(badge, Qt::AlignCenter, delayLabel);
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
        }

        int doorVote = 9;
        int voteGroup = 0;
        QPointF voteCell;
        if (m_showDebugDirections && m_scale >= 10.0 &&
            m_data->debugDoorVoteForAgent(agent, m_tick, &doorVote, &voteCell, &voteGroup)) {
            QString voteLabel = doorVote > 0 ? "D" : (doorVote < 0 ? "U" : "0");
            const QVector<DebugDirectionMarker>& markers = m_data->debugDirectionMarkers();
            if (!markers.isEmpty() &&
                markers.front().axis.compare("horizontal", Qt::CaseInsensitive) == 0) {
                if (voteGroup == 1 || voteGroup == 2) {
                    const QString groupLabel = voteGroup == 1 ? "W" : "E";
                    const QString directionLabel = doorVote > 0 ? ">" : (doorVote < 0 ? "<" : "0");
                    voteLabel = groupLabel + directionLabel;
                } else {
                    voteLabel = doorVote > 0 ? "E" : (doorVote < 0 ? "W" : "0");
                }
            }
            QColor voteColor = doorVote > 0 ? QColor(226, 125, 42, 235)
                                            : (doorVote < 0 ? QColor(47, 128, 237, 235)
                                                           : QColor(95, 105, 120, 220));
            const QPointF voteCenter = center;
            QFont font = painter.font();
            font.setPointSizeF(std::clamp(radius * 0.95, 7.0, 18.0));
            font.setBold(true);
            painter.setFont(font);
            const QFontMetricsF metrics(font);
            const double badgeW = std::max(radius * 1.45, metrics.horizontalAdvance(voteLabel) + 6.0);
            const double badgeH = std::max(radius * 1.15, metrics.height() * 0.9);
            const QRectF badge(voteCenter.x() + radius * 0.35,
                               voteCenter.y() - radius * 1.45,
                               badgeW,
                               badgeH);
            painter.setPen(QPen(Qt::white, 1.0));
            painter.setBrush(voteColor);
            painter.drawRoundedRect(badge, 3.0, 3.0);
            painter.setPen(Qt::white);
            painter.drawText(badge, Qt::AlignCenter, voteLabel);
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
        }

        if (agent == m_selectedAgent && m_data->hasAgentOrientations()) {
            drawAgentHeading(painter, center, radius, frame.dir, true);
        }
    }
}

void MapView::drawAgentHeading(QPainter& painter,
                               const QPointF& center,
                               double radius,
                               double dir,
                               bool selected)
{
    const double angle = -dir * 3.14159265358979323846 / 2.0;
    const QPointF dirDelta(std::cos(angle) * radius, -std::sin(angle) * radius);
    const QPointF start = selected ? center - dirDelta * 0.18 : center;
    const QPointF end = center + dirDelta * (selected ? 1.18 : 1.0);
    const double width = std::max(1.0, radius * (selected ? 0.34 : 0.25));
    if (selected) {
        painter.setPen(QPen(QColor(255, 255, 255, 235), width + 2.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(start, end);
    }
    painter.setPen(QPen(QColor(20, 25, 35), width, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(start, end);
    painter.setPen(Qt::NoPen);
}

void MapView::wheelEvent(QWheelEvent* event)
{
    const QPointF before = screenToWorld(event->position());
    const double factor = event->angleDelta().y() > 0 ? 1.18 : 1.0 / 1.18;
    m_scale = std::clamp(m_scale * factor, 0.5, 80.0);
    const QPointF afterScreen = worldToScreen(before);
    m_offset += event->position() - afterScreen;
    update();
}

void MapView::mousePressEvent(QMouseEvent* event)
{
    m_lastMouse = event->pos();
    m_pressPos = event->pos();
}

void MapView::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        const QPoint delta = event->pos() - m_lastMouse;
        m_offset += QPointF(delta);
        m_lastMouse = event->pos();
        update();
    }
}

void MapView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    const QPoint delta = event->pos() - m_pressPos;
    if (delta.manhattanLength() > 4) {
        return;
    }

    const int agent = agentAtScreen(event->position());
    setSelectedAgent(agent == m_selectedAgent ? -1 : agent);
}

void MapView::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    resetView();
}

void MapView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_fitOnNextResize && m_data && width() > 0 && height() > 0) {
        m_fitOnNextResize = false;
        resetView();
    }
}

QPointF MapView::worldToScreen(const QPointF& world) const
{
    return QPointF(world.x() * m_scale + m_offset.x(), world.y() * m_scale + m_offset.y());
}

QPointF MapView::screenToWorld(const QPointF& screen) const
{
    return QPointF((screen.x() - m_offset.x()) / m_scale, (screen.y() - m_offset.y()) / m_scale);
}

void MapView::drawHeatMap(QPainter& painter, int minRow, int maxRow, int minCol, int maxCol)
{
    if (!m_data || !m_showHeatMap) {
        return;
    }
    m_data->ensureOccupancyHeat();
    if (m_data->maxOccupancyHeat() <= 0) {
        return;
    }
    const QVector<int>& heat = m_data->occupancyHeat();
    if (heat.size() < m_data->width() * m_data->height()) {
        return;
    }

    const double lowLog = m_data->occupancyHeatLogLow();
    const double highLog = m_data->occupancyHeatLogHigh();
    const double spanLog = highLog - lowLog;
    if (spanLog <= 0.000001) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(Qt::NoPen);
    for (int row = minRow; row <= maxRow; ++row) {
        for (int col = minCol; col <= maxCol; ++col) {
            const int count = heat[row * m_data->width() + col];
            if (count <= 0) {
                continue;
            }
            const double value = (std::log1p(static_cast<double>(count)) - lowLog) / spanLog;
            painter.setBrush(heatColor(value));
            const QPointF p = worldToScreen(QPointF(col, row));
            painter.drawRect(QRectF(p.x(), p.y(), m_scale + 0.2, m_scale + 0.2));
        }
    }
    painter.restore();
}

void MapView::drawWaitMap(QPainter& painter, int minRow, int maxRow, int minCol, int maxCol)
{
    if (!m_data || !m_showWaitMap) {
        return;
    }
    m_data->ensureWaitHeat();
    if (m_data->maxWaitHeat() <= 0) {
        return;
    }
    const QVector<int>& heat = m_data->waitHeat();
    if (heat.size() < m_data->width() * m_data->height()) {
        return;
    }

    const double lowLog = m_data->waitHeatLogLow();
    const double highLog = m_data->waitHeatLogHigh();
    const double spanLog = highLog - lowLog;
    if (spanLog <= 0.000001) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(Qt::NoPen);
    for (int row = minRow; row <= maxRow; ++row) {
        for (int col = minCol; col <= maxCol; ++col) {
            const int count = heat[row * m_data->width() + col];
            if (count <= 0) {
                continue;
            }
            const double value = (std::log1p(static_cast<double>(count)) - lowLog) / spanLog;
            painter.setBrush(waitColor(value));
            const QPointF p = worldToScreen(QPointF(col, row));
            painter.drawRect(QRectF(p.x(), p.y(), m_scale + 0.2, m_scale + 0.2));
        }
    }
    painter.restore();
}

void MapView::drawHeatClusterOutlines(QPainter& painter, const QRectF& visible)
{
    if (!m_data || !m_showHeatOutlines) {
        return;
    }
    m_data->ensureOccupancyHeat();
    if (m_data->maxOccupancyHeat() <= 0) {
        return;
    }
    const int width = m_data->width();
    const int height = m_data->height();
    const int cellsCount = width * height;
    const QVector<int>& heat = m_data->occupancyHeat();
    const QVector<unsigned char>& map = m_data->mapCells();
    if (heat.size() < cellsCount || map.size() < cellsCount) {
        return;
    }

    const double lowLog = m_data->occupancyHeatLogLow();
    const double highLog = m_data->occupancyHeatLogHigh();
    const double spanLog = highLog - lowLog;
    if (spanLog <= 0.000001) {
        return;
    }

    static constexpr double HOT_THRESHOLD = 0.58;
    static constexpr int HEAT_BANDS = 5;
    static constexpr int SMALL_CLUSTER_MAX_SIZE = 16;
    const int topBand = HEAT_BANDS - 1;
    const double denom = std::max(1e-9, 1.0 - HOT_THRESHOLD);

    QVector<int> band(cellsCount, -1);
    for (int loc = 0; loc < cellsCount; ++loc) {
        if (map[loc] != 0 || heat[loc] <= 0) {
            continue;
        }
        const double value = (std::log1p(static_cast<double>(heat[loc])) - lowLog) / spanLog;
        if (value < HOT_THRESHOLD) {
            continue;
        }
        band[loc] = std::min(topBand, std::max(0, static_cast<int>(std::floor(((value - HOT_THRESHOLD) / denom) * HEAT_BANDS))));
    }

    QVector<int> clusterId(cellsCount, -1);
    QVector<int> clusterSize;
    std::queue<int> queue;
    for (int seed = 0; seed < cellsCount; ++seed) {
        if (band[seed] != topBand || clusterId[seed] != -1) {
            continue;
        }
        const int cid = clusterSize.size();
        int size = 0;
        clusterId[seed] = cid;
        queue.push(seed);
        while (!queue.empty()) {
            const int loc = queue.front();
            queue.pop();
            ++size;
            const int row = loc / width;
            const int col = loc % width;
            static constexpr int DRS[4] = {-1, 1, 0, 0};
            static constexpr int DCS[4] = {0, 0, -1, 1};
            for (int k = 0; k < 4; ++k) {
                const int nr = row + DRS[k];
                const int nc = col + DCS[k];
                if (nr < 0 || nr >= height || nc < 0 || nc >= width) {
                    continue;
                }
                const int nb = nr * width + nc;
                if (band[nb] != topBand || clusterId[nb] != -1) {
                    continue;
                }
                clusterId[nb] = cid;
                queue.push(nb);
            }
        }
        clusterSize.push_back(size);
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    const QColor largeColor(0, 229, 255, 245);
    const QColor smallColor(0, 255, 102, 235);
    const double largeWidth = std::clamp(m_scale * 0.24, 3.0, 9.0);
    const double smallWidth = std::clamp(m_scale * 0.16, 2.0, 7.0);
    const QRectF expandedVisible = visible.adjusted(-1.0, -1.0, 1.0, 1.0);

    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            const int loc = row * width + col;
            const int cid = clusterId[loc];
            if (cid < 0) {
                continue;
            }
            const QRectF cellRect(col, row, 1.0, 1.0);
            if (!expandedVisible.intersects(cellRect)) {
                continue;
            }
            const bool large = clusterSize[cid] > SMALL_CLUSTER_MAX_SIZE;
            painter.setPen(QPen(large ? largeColor : smallColor,
                                large ? largeWidth : smallWidth,
                                Qt::SolidLine,
                                Qt::SquareCap,
                                Qt::MiterJoin));
            const QPointF topLeft = worldToScreen(QPointF(col, row));
            const QPointF topRight = worldToScreen(QPointF(col + 1, row));
            const QPointF bottomLeft = worldToScreen(QPointF(col, row + 1));
            const QPointF bottomRight = worldToScreen(QPointF(col + 1, row + 1));
            auto differs = [&](int nr, int nc) {
                if (nr < 0 || nr >= height || nc < 0 || nc >= width) {
                    return true;
                }
                return clusterId[nr * width + nc] != cid;
            };
            if (differs(row - 1, col)) painter.drawLine(topLeft, topRight);
            if (differs(row + 1, col)) painter.drawLine(bottomLeft, bottomRight);
            if (differs(row, col - 1)) painter.drawLine(topLeft, bottomLeft);
            if (differs(row, col + 1)) painter.drawLine(topRight, bottomRight);
        }
    }
    painter.restore();
}

QRectF MapView::visibleWorldRect() const
{
    const QPointF topLeft = screenToWorld(QPointF(0, 0));
    const QPointF bottomRight = screenToWorld(QPointF(width(), height()));
    return QRectF(topLeft, bottomRight).normalized().adjusted(-1, -1, 1, 1);
}

int MapView::agentAtScreen(const QPointF& screen) const
{
    if (!m_data) {
        return -1;
    }

    const bool largeOrzMap = m_data->height() == 656 && m_data->width() == 1491;
    const double radiusFactor = largeOrzMap ? 0.30 : 0.38;
    const double radius = std::clamp(m_scale * radiusFactor, 2.0, std::min(18.0, m_scale * 0.46));
    const double hitRadius = std::max(radius + 4.0, 7.0);
    int bestAgent = -1;
    double bestDist2 = hitRadius * hitRadius;

    for (int agent = 0; agent < m_data->teamSize(); ++agent) {
        const AgentFrame frame = m_data->frameAt(agent, m_tick);
        const QPointF center = worldToScreen(QPointF(frame.cell.x() + 0.5, frame.cell.y() + 0.5));
        const QPointF delta = center - screen;
        const double dist2 = delta.x() * delta.x() + delta.y() * delta.y();
        if (dist2 <= bestDist2) {
            bestDist2 = dist2;
            bestAgent = agent;
        }
    }
    return bestAgent;
}

void MapView::drawDebugOverlays(QPainter& painter, const QRectF& visible)
{
    if (!m_data || !m_showDebugOverlays) {
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing, false);
    for (const DebugOverlay& overlay : m_data->debugOverlays()) {
        if (!visible.intersects(overlay.rect)) {
            continue;
        }

        const QPointF topLeft = worldToScreen(overlay.rect.topLeft());
        const QRectF screenRect(topLeft, QSizeF(overlay.rect.width() * m_scale,
                                                overlay.rect.height() * m_scale));
        QColor fill = overlay.color;
        fill.setAlpha(62);
        QColor border = overlay.color;
        border.setAlpha(190);
        painter.setBrush(fill);
        painter.setPen(QPen(border, std::clamp(m_scale * 0.08, 1.0, 3.0)));
        painter.drawRect(screenRect.adjusted(1.0, 1.0, -1.0, -1.0));

        if (m_scale >= 10.0 && screenRect.width() >= 36.0 && screenRect.height() >= 16.0) {
            QFont font = painter.font();
            font.setPointSizeF(std::clamp(m_scale * 0.28, 7.0, 11.0));
            font.setBold(true);
            painter.setFont(font);
            painter.setPen(QPen(border, 1.0));
            painter.drawText(screenRect.adjusted(3.0, 1.0, -3.0, -1.0),
                             Qt::AlignLeft | Qt::AlignTop, overlay.name);
        }
    }
    painter.setPen(Qt::NoPen);
}

void MapView::drawOrzBottlenecks(QPainter& painter, const QRectF& visible)
{
    if (!m_data || !m_showOrzBottlenecks || m_data->height() < 576 || m_data->width() < 1120) {
        return;
    }

    struct Bottleneck {
        int minRow;
        int maxRow;
        int minCol;
        int maxCol;
    };
    static constexpr Bottleneck bottlenecks[] = {
        {192, 223, 672, 735},
        {96, 127, 736, 767},
        {416, 447, 576, 607},
        {544, 575, 960, 1023},
        {480, 511, 608, 639},
        {480, 511, 1088, 1119},
    };

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    const QColor color(225, 96, 35);
    for (int index = 0; index < std::size(bottlenecks); ++index) {
        const Bottleneck& bottleneck = bottlenecks[index];
        const QRectF rect(bottleneck.minCol, bottleneck.minRow,
                          bottleneck.maxCol - bottleneck.minCol + 1,
                          bottleneck.maxRow - bottleneck.minRow + 1);
        if (!visible.intersects(rect)) {
            continue;
        }

        const QPointF topLeft = worldToScreen(rect.topLeft());
        const QRectF screenRect(topLeft, QSizeF(rect.width() * m_scale, rect.height() * m_scale));
        QColor fill = color;
        fill.setAlpha(38);
        QColor border = color;
        border.setAlpha(230);
        painter.setBrush(fill);
        painter.setPen(QPen(border, std::clamp(m_scale * 0.12, 1.0, 4.0), Qt::DashLine));
        painter.drawRect(screenRect.adjusted(1.0, 1.0, -1.0, -1.0));

        if (m_scale >= 9.0 && screenRect.width() >= 54.0 && screenRect.height() >= 18.0) {
            QFont font = painter.font();
            font.setPointSizeF(std::clamp(m_scale * 0.30, 7.0, 12.0));
            font.setBold(true);
            painter.setFont(font);
            painter.setPen(QPen(border, 1.0));
            painter.drawText(screenRect.adjusted(3.0, 2.0, -3.0, -2.0),
                             Qt::AlignLeft | Qt::AlignTop,
                             QString("ORZ bottleneck %1").arg(index + 1));
        }
    }
    painter.restore();
}

void MapView::drawOrzParkingAreas(QPainter& painter, const QRectF& visible)
{
    if (!m_data || !m_showOrzParkingAreas || m_data->height() != 656 || m_data->width() != 1491) {
        return;
    }

    struct ParkingRect {
        int minRow;
        int maxRow;
        int minCol;
        int maxCol;
        const char* label;
        QColor color;
    };
    const ParkingRect parkingAreas[] = {
        {552, 554, 999, 1002, "upper parking", QColor(30, 136, 229)},
        {555, 557, 998, 1001, "upper parking", QColor(30, 136, 229)},
        {558, 559, 997, 999, "upper parking", QColor(30, 136, 229)},
        {560, 562, 997, 998, "upper parking", QColor(30, 136, 229)},
        {563, 563, 997, 999, "upper parking", QColor(30, 136, 229)},
        {564, 564, 997, 1001, "upper parking", QColor(30, 136, 229)},
        {565, 569, 997, 1002, "upper parking", QColor(30, 136, 229)},
        {579, 579, 966, 982, "lower parking", QColor(0, 150, 136)},
        {580, 580, 956, 981, "lower parking", QColor(0, 150, 136)},
        {581, 581, 956, 966, "lower parking", QColor(0, 150, 136)},
        {581, 581, 973, 980, "lower parking", QColor(0, 150, 136)},
        {582, 582, 956, 964, "lower parking", QColor(0, 150, 136)},
        {582, 582, 980, 980, "lower parking", QColor(0, 150, 136)},
        {583, 583, 956, 960, "lower parking", QColor(0, 150, 136)},
        {584, 584, 956, 956, "lower parking", QColor(0, 150, 136)},
    };
    const ParkingRect gateCells[] = {
        {571, 571, 987, 987, "gate", QColor(255, 112, 67)},
        {572, 572, 988, 988, "gate", QColor(255, 112, 67)},
    };

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const auto draw_cell_rect = [&](const ParkingRect& area, bool drawCells) {
        const QRectF bounds(area.minCol, area.minRow,
                            area.maxCol - area.minCol + 1,
                            area.maxRow - area.minRow + 1);
        if (!visible.intersects(bounds)) {
            return;
        }

        QColor fill = area.color;
        fill.setAlpha(drawCells ? 54 : 82);
        QColor border = area.color;
        border.setAlpha(245);

        if (drawCells) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            const auto& cells = m_data->mapCells();
            for (int row = area.minRow; row <= area.maxRow; row++) {
                for (int col = area.minCol; col <= area.maxCol; col++) {
                    const int index = row * m_data->width() + col;
                    if (index < 0 || index >= cells.size() || cells[index] != 0) {
                        continue;
                    }
                    painter.drawRect(QRectF(worldToScreen(QPointF(col, row)),
                                            QSizeF(m_scale, m_scale)));
                }
            }
        }

        const QPointF topLeft = worldToScreen(QPointF(area.minCol, area.minRow));
        const QPointF bottomRight = worldToScreen(QPointF(area.maxCol + 1, area.maxRow + 1));
        QRectF screenRect(topLeft, bottomRight);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(border, std::clamp(m_scale * 0.10, 1.0, 4.0), Qt::DashLine));
        painter.drawRect(screenRect.normalized());

        if (m_scale >= 7.0) {
            QFont font = painter.font();
            font.setPointSizeF(std::clamp(m_scale * 0.28, 7.0, 12.0));
            font.setBold(true);
            painter.setFont(font);
            painter.setPen(QPen(border, 1.0));
            painter.drawText(screenRect.adjusted(3.0, 2.0, -3.0, -2.0),
                             Qt::AlignLeft | Qt::AlignTop,
                             QString::fromUtf8(area.label));
        }
    };

    for (const ParkingRect& area : parkingAreas) {
        draw_cell_rect(area, true);
    }
    for (const ParkingRect& gate : gateCells) {
        draw_cell_rect(gate, false);
    }
    painter.restore();
}

void MapView::drawDebugDirections(QPainter& painter, const QRectF& visible)
{
    if (!m_data || !m_showDebugDirections) {
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const DebugDirectionMarker& marker : m_data->debugDirectionMarkers()) {
        QPointF drawCell = marker.cell;
        const int markerCol = static_cast<int>(std::floor(marker.cell.x()));
        const int markerRow = static_cast<int>(std::floor(marker.cell.y()));
        const QVector<QPoint> wallOffsets = {
            QPoint(1, 0), QPoint(-1, 0), QPoint(0, -1), QPoint(0, 1)
        };
        for (const QPoint& offset : wallOffsets) {
            const int row = markerRow + offset.y();
            const int col = markerCol + offset.x();
            if (row < 0 || row >= m_data->height() || col < 0 || col >= m_data->width()) {
                continue;
            }
            if (m_data->mapCells()[row * m_data->width() + col] != 0) {
                drawCell = QPointF(col, row);
                break;
            }
        }

        const QPointF centerCell(drawCell.x() + 0.5, drawCell.y() + 0.5);
        if (!visible.adjusted(-1.0, -1.0, 1.0, 1.0).contains(centerCell)) {
            continue;
        }
        if (marker.directions.isEmpty()) {
            continue;
        }

        const int idx = std::clamp(m_tick,
                                   0, static_cast<int>(marker.directions.size()) - 1);
        const int direction = marker.directions[idx];
        const int downVotes = idx < marker.downVotes.size() ? marker.downVotes[idx] : 0;
        const int upVotes = idx < marker.upVotes.size() ? marker.upVotes[idx] : 0;
        const int pendingDirection = idx < marker.pendingDirections.size() ? marker.pendingDirections[idx] : 0;
        const bool yellowActive = idx < marker.yellowActive.size() && marker.yellowActive[idx] != 0;
        const bool zeroVoteBlocked = idx < marker.zeroVoteBlocked.size() && marker.zeroVoteBlocked[idx] != 0;
        const QPointF center = worldToScreen(centerCell);
        const double length = std::clamp(m_scale * 1.65, 18.0, 42.0);
        const double head = std::clamp(m_scale * 0.45, 5.0, 12.0);
        const QColor color = yellowActive ? QColor(236, 185, 35, 245) : QColor(210, 45, 35, 235);
        painter.setPen(QPen(color, std::clamp(m_scale * 0.18, 2.0, 5.0),
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(color);

        const bool horizontal = marker.axis.compare("horizontal", Qt::CaseInsensitive) == 0;
        if (direction == 0) {
            const QPointF left(center.x() - length * 0.32, center.y());
            const QPointF right(center.x() + length * 0.32, center.y());
            painter.drawLine(left, right);
            painter.drawEllipse(center, std::clamp(m_scale * 0.18, 2.0, 5.0),
                                std::clamp(m_scale * 0.18, 2.0, 5.0));
        } else {
            const double sign = direction > 0 ? 1.0 : -1.0;
            const QPointF start = horizontal
                ? QPointF(center.x() - sign * length * 0.45, center.y())
                : QPointF(center.x(), center.y() - sign * length * 0.45);
            const QPointF end = horizontal
                ? QPointF(center.x() + sign * length * 0.45, center.y())
                : QPointF(center.x(), center.y() + sign * length * 0.45);
            painter.drawLine(start, end);
            QPolygonF arrowHead;
            if (horizontal) {
                arrowHead << end
                          << QPointF(end.x() - sign * head, end.y() - head * 0.65)
                          << QPointF(end.x() - sign * head, end.y() + head * 0.65);
            } else {
                arrowHead << end
                          << QPointF(end.x() - head * 0.65, end.y() - sign * head)
                          << QPointF(end.x() + head * 0.65, end.y() - sign * head);
            }
            painter.drawPolygon(arrowHead);
        }

        if (m_scale >= 8.0) {
            QFont font = painter.font();
            font.setPointSizeF(std::clamp(m_scale * 0.34, 8.0, 13.0));
            font.setBold(true);
            painter.setFont(font);
            painter.setPen(color);
            const QString positiveLabel = horizontal ? "east"
                                                     : (marker.positiveLabel.isEmpty() ? "down" : marker.positiveLabel);
            const QString negativeLabel = horizontal ? "west"
                                                     : (marker.negativeLabel.isEmpty() ? "up" : marker.negativeLabel);
            const QString positiveInitial = horizontal ? "E" : positiveLabel.left(1).toUpper();
            const QString negativeInitial = horizontal ? "W" : negativeLabel.left(1).toUpper();
            const QString directionText = direction > 0 ? positiveLabel : (direction < 0 ? negativeLabel : "tie");
            const QString pendingText = pendingDirection > 0 ? positiveLabel : (pendingDirection < 0 ? negativeLabel : "tie");
            const QString title = yellowActive
                ? QString("yellow -> %1").arg(pendingText)
                : directionText;
            const QString label = QString("%1\n%2:%3 %4:%5%6")
                                      .arg(title)
                                      .arg(positiveInitial)
                                      .arg(downVotes)
                                      .arg(negativeInitial)
                                      .arg(upVotes)
                                      .arg(zeroVoteBlocked ? " Z:block" : "");
            painter.drawText(QRectF(center.x() + head, center.y() - length * 0.55,
                                    length * 3.0, length * 1.25),
                             Qt::AlignVCenter | Qt::AlignLeft, label);
        }
    }
    painter.setPen(Qt::NoPen);
}

void MapView::drawHighwayDirections(QPainter& painter, const QRectF& visible)
{
    if (!m_data || !m_showHighwayDirections) {
        return;
    }

    const QVector<HighwayDirection>& directions = m_data->highwayDirections();
    if (directions.isEmpty()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, m_scale >= 5.0);
    const QRectF expandedVisible = visible.adjusted(-1.0, -1.0, 1.0, 1.0);
    const QColor color(0, 126, 145, 130);
    const double lineWidth = std::clamp(m_scale * 0.10, 1.0, 3.0);
    const double length = std::clamp(m_scale * 0.55, 5.0, 18.0);
    const double head = std::clamp(m_scale * 0.18, 2.5, 6.0);
    painter.setPen(QPen(color, lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(color);

    QVector<const HighwayDirection*> visibleDirections;
    visibleDirections.reserve(directions.size());
    for (const HighwayDirection& highway : directions) {
        const QPointF centerCell(highway.cell.x() + 0.5, highway.cell.y() + 0.5);
        if (expandedVisible.contains(centerCell)) {
            visibleDirections.push_back(&highway);
        }
    }
    std::sort(visibleDirections.begin(), visibleDirections.end(),
              [](const HighwayDirection* lhs, const HighwayDirection* rhs) {
                  if (lhs->cell.y() != rhs->cell.y()) return lhs->cell.y() < rhs->cell.y();
                  if (lhs->cell.x() != rhs->cell.x()) return lhs->cell.x() < rhs->cell.x();
                  return lhs->dir < rhs->dir;
              });

    const auto unitForDir = [](int dir) -> QPointF {
        QPointF unit(0.0, 0.0);
        if (dir == 0) unit = QPointF(1.0, 0.0);
        if (dir == 1) unit = QPointF(0.0, 1.0);
        if (dir == 2) unit = QPointF(-1.0, 0.0);
        if (dir == 3) unit = QPointF(0.0, -1.0);
        return unit;
    };

    const auto drawArrow = [&](const QPointF& center, const QPointF& unit, double arrowLength, double arrowHeadSize) {
        if (unit == QPointF(0.0, 0.0)) {
            return;
        }

        const QPointF start(center.x() - unit.x() * arrowLength * 0.45,
                            center.y() - unit.y() * arrowLength * 0.45);
        const QPointF end(center.x() + unit.x() * arrowLength * 0.45,
                          center.y() + unit.y() * arrowLength * 0.45);
        painter.drawLine(start, end);
        if (m_scale >= 4.0) {
            const QPointF perp(-unit.y(), unit.x());
            QPolygonF arrowHead;
            arrowHead << end
                      << QPointF(end.x() - unit.x() * arrowHeadSize + perp.x() * arrowHeadSize * 0.55,
                                 end.y() - unit.y() * arrowHeadSize + perp.y() * arrowHeadSize * 0.55)
                      << QPointF(end.x() - unit.x() * arrowHeadSize - perp.x() * arrowHeadSize * 0.55,
                                 end.y() - unit.y() * arrowHeadSize - perp.y() * arrowHeadSize * 0.55);
            painter.drawPolygon(arrowHead);
        }
    };

    int first = 0;
    while (first < visibleDirections.size()) {
        int last = first + 1;
        const QPointF cell = visibleDirections[first]->cell;
        while (last < visibleDirections.size() && visibleDirections[last]->cell == cell) {
            ++last;
        }

        const int count = last - first;
        const QPointF baseCenter = worldToScreen(QPointF(cell.x() + 0.5, cell.y() + 0.5));
        if (count == 1) {
            drawArrow(baseCenter, unitForDir(visibleDirections[first]->dir), length, head);
        } else {
            const double smallLength = std::clamp(m_scale * 0.42, 4.0, 13.0);
            const double smallHead = std::clamp(m_scale * 0.13, 2.0, 4.5);
            const double offset = std::clamp(m_scale * 0.20, 2.0, 7.0);
            for (int i = first; i < last; ++i) {
                const QPointF unit = unitForDir(visibleDirections[i]->dir);
                const QPointF shiftedCenter(baseCenter.x() + unit.x() * offset,
                                            baseCenter.y() + unit.y() * offset);
                drawArrow(shiftedCenter, unit, smallLength, smallHead);
            }
        }
        first = last;
    }

    painter.restore();
}

void MapView::drawDirectionMap(QPainter& painter, const QRectF& visible)
{
    if (!m_data || !m_showDirectionMap) {
        return;
    }

    const DirectionMapSnapshot* snapshot = m_data->directionMapAtTick(m_tick);
    if (snapshot == nullptr || snapshot->cells.isEmpty()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, m_scale >= 4.0);
    const QRectF expandedVisible = visible.adjusted(-1.0, -1.0, 1.0, 1.0);
    const double baseLength = std::clamp(m_scale * 0.60, 4.0, 18.0);
    const double head = std::clamp(m_scale * 0.14, 2.0, 5.0);
    const double lineWidth = std::clamp(m_scale * 0.075, 0.8, 2.5);

    for (const DirectionMapCell& cell : snapshot->cells) {
        const QPointF centerCell(cell.cell.x() + 0.5, cell.cell.y() + 0.5);
        if (!expandedVisible.contains(centerCell)) {
            continue;
        }

        const double mag = std::clamp(cell.magnitude, 0.0, 1.0);
        if (mag <= 0.0) {
            continue;
        }
        const QPointF unit(cell.x / cell.magnitude, cell.y / cell.magnitude);
        const QPointF center = worldToScreen(centerCell);
        const double length = baseLength * std::clamp(0.55 + mag * 0.65, 0.45, 1.2);
        QColor color(30, 99, 215);
        color.setAlpha(std::clamp(static_cast<int>(55 + mag * 155), 55, 210));
        painter.setPen(QPen(color, lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(color);

        const QPointF start(center.x() - unit.x() * length * 0.42,
                            center.y() - unit.y() * length * 0.42);
        const QPointF end(center.x() + unit.x() * length * 0.42,
                          center.y() + unit.y() * length * 0.42);
        painter.drawLine(start, end);

        if (m_scale >= 4.0) {
            const QPointF perp(-unit.y(), unit.x());
            QPolygonF arrowHead;
            arrowHead << end
                      << QPointF(end.x() - unit.x() * head + perp.x() * head * 0.55,
                                 end.y() - unit.y() * head + perp.y() * head * 0.55)
                      << QPointF(end.x() - unit.x() * head - perp.x() * head * 0.55,
                                 end.y() - unit.y() * head - perp.y() * head * 0.55);
            painter.drawPolygon(arrowHead);
        }
    }

    painter.restore();
}

void MapView::drawPibtTrace(QPainter& painter, const QRectF& visible)
{
    if (!m_showPibtTrace || !m_data) {
        return;
    }

    const QVector<PibtTraceEvent> events = m_data->pibtTraceForTick(m_tick);
    if (events.isEmpty()) {
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF expandedVisible = visible.adjusted(-2.0, -2.0, 2.0, 2.0);
    for (const PibtTraceEvent& event : events) {
        if (event.fromAgent < 0 || event.toAgent < 0 ||
            event.fromAgent >= m_data->teamSize() || event.toAgent >= m_data->teamSize()) {
            continue;
        }

        const AgentFrame fromFrame = m_data->frameAt(event.fromAgent, m_tick);
        const AgentFrame toFrame = m_data->frameAt(event.toAgent, m_tick);
        const QPointF fromCell(fromFrame.cell.x() + 0.5, fromFrame.cell.y() + 0.5);
        const QPointF toCell(toFrame.cell.x() + 0.5, toFrame.cell.y() + 0.5);
        const QRectF bounds(fromCell, toCell);
        if (!expandedVisible.intersects(bounds.normalized().adjusted(-1.0, -1.0, 1.0, 1.0))) {
            continue;
        }

        const bool isBacktrack = event.type.compare("backtrack", Qt::CaseInsensitive) == 0;
        const QColor color = isBacktrack ? QColor(220, 45, 45, 235)
                                         : QColor(145, 62, 210, 235);
        QPointF start = worldToScreen(fromCell);
        QPointF end = worldToScreen(toCell);
        QPointF delta = end - start;
        const double length = std::hypot(delta.x(), delta.y());
        if (length < 1.0) {
            continue;
        }

        const QPointF unit(delta.x() / length, delta.y() / length);
        const double inset = std::clamp(m_scale * 0.46, 5.0, 20.0);
        start += unit * inset;
        end -= unit * inset;

        QPen pen(color, std::clamp(m_scale * 0.08, 1.2, 3.0),
                 isBacktrack ? Qt::DashLine : Qt::SolidLine,
                 Qt::RoundCap, Qt::RoundJoin);
        if (isBacktrack) {
            pen.setDashPattern({4.0, 4.0});
        }
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(start, end);

        const QPointF normal(-unit.y(), unit.x());
        const double headLen = std::clamp(m_scale * 0.42, 6.0, 16.0);
        const double headWidth = std::clamp(m_scale * 0.25, 4.0, 10.0);
        QPolygonF head;
        head << end
             << (end - unit * headLen + normal * headWidth)
             << (end - unit * headLen - normal * headWidth);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPolygon(head);

        if (m_scale >= 24.0) {
            const QString label = isBacktrack ? "B" : "I";
            const QPointF mid = (start + end) * 0.5;
            QFont font = painter.font();
            font.setPointSizeF(std::clamp(m_scale * 0.18, 6.0, 10.0));
            font.setBold(true);
            painter.setFont(font);
            const double labelSize = std::clamp(m_scale * 0.34, 8.0, 14.0);
            const QRectF labelRect(mid.x() - labelSize / 2.0,
                                   mid.y() - labelSize / 2.0,
                                   labelSize,
                                   labelSize);
            QColor labelFill = color;
            labelFill.setAlpha(150);
            painter.setPen(QPen(Qt::white, 0.8));
            painter.setBrush(labelFill);
            painter.drawEllipse(labelRect);
            painter.setPen(Qt::white);
            painter.drawText(labelRect, Qt::AlignCenter, label);
        }
    }
    painter.setPen(Qt::NoPen);
}

void MapView::drawFreeTasks(QPainter& painter, const QRectF& visible)
{
    if (!m_showFreeTasks || !m_data) {
        return;
    }

    AssignmentSnapshot snapshot;
    if (!m_data->assignmentSnapshotAt(m_tick, &snapshot)) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    QFont font = painter.font();
    font.setPointSizeF(std::clamp(m_scale * 0.22, 6.0, 11.0));
    font.setBold(true);
    painter.setFont(font);
    const double radius = std::clamp(m_scale * 0.24, 3.0, 9.0);
    const QColor taskColor(250, 196, 40, 220);
    const QColor taskBorder(115, 86, 0, 230);

    for (int taskId : snapshot.freeTasks) {
        const QVector<QPointF>* errands = m_data->taskErrands(taskId);
        if (errands == nullptr || errands->isEmpty()) {
            continue;
        }
        const QPointF first = errands->front();
        const QPointF centerCell(first.x() + 0.5, first.y() + 0.5);
        const QRectF expandedVisible = visible.adjusted(-1.0, -1.0, 1.0, 1.0);
        if (!expandedVisible.contains(centerCell)) {
            continue;
        }
        const QPointF center = worldToScreen(centerCell);
        if (errands->size() >= 2) {
            const QPointF second = errands->at(1);
            const QPointF secondCenterCell(second.x() + 0.5, second.y() + 0.5);
            const QPointF secondCenter = worldToScreen(secondCenterCell);
            painter.setPen(QPen(QColor(235, 140, 20, 185),
                                std::clamp(m_scale * 0.08, 1.3, 3.4),
                                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(Qt::NoBrush);
            painter.drawLine(center, secondCenter);

            const QPointF delta = secondCenter - center;
            const double length = std::hypot(delta.x(), delta.y());
            if (length > 1.0) {
                const QPointF unit(delta.x() / length, delta.y() / length);
                const QPointF normal(-unit.y(), unit.x());
                const double headLen = std::clamp(m_scale * 0.28, 4.0, 10.0);
                const double headWidth = std::clamp(m_scale * 0.18, 3.0, 7.0);
                const QPointF tip = secondCenter - unit * std::clamp(m_scale * 0.16, 2.0, 7.0);
                QPolygonF head;
                head << tip
                     << (tip - unit * headLen + normal * headWidth)
                     << (tip - unit * headLen - normal * headWidth);
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(235, 140, 20, 205));
                painter.drawPolygon(head);
            }

            if (expandedVisible.contains(secondCenterCell)) {
                painter.setBrush(QColor(255, 235, 145, 185));
                painter.setPen(QPen(QColor(160, 105, 0, 190), std::max(1.0, radius * 0.14)));
                painter.drawEllipse(secondCenter, radius * 0.68, radius * 0.68);
            }
        }

        painter.setBrush(taskColor);
        painter.setPen(QPen(taskBorder, std::max(1.0, radius * 0.18)));
        painter.drawRect(QRectF(center.x() - radius, center.y() - radius,
                                radius * 2.0, radius * 2.0));

        if (m_scale >= 20.0) {
            const QString label = QString::number(taskId);
            const QFontMetricsF metrics(font);
            const double textW = metrics.horizontalAdvance(label) + 6.0;
            const double textH = metrics.height() + 2.0;
            const QRectF labelRect(center.x() + radius + 2.0,
                                   center.y() - textH * 0.5,
                                   textW,
                                   textH);
            painter.setBrush(QColor(255, 255, 255, 220));
            painter.setPen(QPen(QColor(115, 86, 0, 190), 1.0));
            painter.drawRoundedRect(labelRect, 2.0, 2.0);
            painter.setPen(QColor(70, 54, 0));
            painter.drawText(labelRect, Qt::AlignCenter, label);
        }
    }

    painter.restore();
}

void MapView::drawSelectedAgentGuidePath(QPainter& painter, const QRectF& visible)
{
    if (!m_showGuidePath || !m_data || m_selectedAgent < 0 || m_selectedAgent >= m_data->teamSize()) {
        return;
    }

    QVector<QPointF> cells;
    if (!m_data->guidePathForAgent(m_selectedAgent, m_tick, &cells) || cells.size() < 2) {
        return;
    }

    const AgentFrame currentFrame = m_data->frameAt(m_selectedAgent, m_tick);
    int startIndex = 0;
    double bestDist = std::numeric_limits<double>::infinity();
    for (int i = 0; i < cells.size(); ++i) {
        const double dx = cells[i].x() - currentFrame.cell.x();
        const double dy = cells[i].y() - currentFrame.cell.y();
        const double dist = dx * dx + dy * dy;
        if (dist < bestDist) {
            bestDist = dist;
            startIndex = i;
            if (dist < 1e-9) {
                break;
            }
        }
    }
    if (startIndex >= cells.size() - 1) {
        return;
    }

    const QRectF expandedVisible = visible.adjusted(-3, -3, 3, 3);
    QPainterPath path;
    bool hasPoint = false;
    QPointF lastCell(-1.0, -1.0);
    for (int i = startIndex; i < cells.size(); ++i) {
        const QPointF& cell = cells[i];
        if (std::abs(cell.x() - lastCell.x()) < 1e-6 &&
            std::abs(cell.y() - lastCell.y()) < 1e-6) {
            continue;
        }
        const QPointF prevCell = lastCell;
        const QPointF center(cell.x() + 0.5, cell.y() + 0.5);
        if (!expandedVisible.contains(center) && hasPoint) {
            continue;
        }
        const QPointF screen = worldToScreen(center);
        const bool contiguous = !hasPoint ||
                                std::abs(cell.x() - prevCell.x()) + std::abs(cell.y() - prevCell.y()) <= 1.0 + 1e-6;
        lastCell = cell;
        if (!hasPoint || !contiguous) {
            path.moveTo(screen);
            hasPoint = true;
        } else {
            path.lineTo(screen);
        }
    }

    if (!hasPoint) {
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor(30, 115, 215, 220), std::clamp(m_scale * 0.14, 1.4, 4.0), Qt::DashDotLine, Qt::RoundCap, Qt::RoundJoin);
    pen.setDashPattern({5.0, 3.0, 1.5, 3.0});
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    const QPointF start = worldToScreen(QPointF(cells[startIndex].x() + 0.5, cells[startIndex].y() + 0.5));
    const QPointF end = worldToScreen(QPointF(cells.back().x() + 0.5, cells.back().y() + 0.5));
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(30, 115, 215, 230));
    painter.drawEllipse(start, std::clamp(m_scale * 0.13, 2.0, 5.0), std::clamp(m_scale * 0.13, 2.0, 5.0));
    painter.setBrush(QColor(30, 115, 215, 170));
    painter.drawRect(QRectF(end.x() - std::clamp(m_scale * 0.14, 2.0, 5.0),
                            end.y() - std::clamp(m_scale * 0.14, 2.0, 5.0),
                            std::clamp(m_scale * 0.28, 4.0, 10.0),
                            std::clamp(m_scale * 0.28, 4.0, 10.0)));
}

void MapView::drawSelectedAgentStagedLocs(QPainter& painter, const QRectF& visible)
{
    if (!m_showStagedLocs || !m_data || m_selectedAgent < 0 || m_selectedAgent >= m_data->teamSize()) {
        return;
    }

    StagedLocSnapshot snapshot;
    if (!m_data->stagedLocsForAgent(m_selectedAgent, m_tick, &snapshot) || snapshot.cells.isEmpty()) {
        return;
    }

    const QRectF expandedVisible = visible.adjusted(-2.0, -2.0, 2.0, 2.0);
    QPainterPath path;
    bool hasPoint = false;
    QPointF lastCell(-1.0, -1.0);
    for (const QPointF& cell : snapshot.cells) {
        if (std::abs(cell.x() - lastCell.x()) < 1e-6 &&
            std::abs(cell.y() - lastCell.y()) < 1e-6) {
            continue;
        }
        const QPointF centerCell(cell.x() + 0.5, cell.y() + 0.5);
        if (!expandedVisible.contains(centerCell) && hasPoint) {
            lastCell = cell;
            continue;
        }
        const QPointF screen = worldToScreen(centerCell);
        if (!hasPoint) {
            path.moveTo(screen);
            hasPoint = true;
        } else {
            path.lineTo(screen);
        }
        lastCell = cell;
    }

    if (!hasPoint) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(0, 155, 190, 235),
                        std::clamp(m_scale * 0.16, 1.8, 5.0),
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    const double radius = std::clamp(m_scale * 0.13, 2.0, 5.5);
    painter.setPen(QPen(QColor(255, 255, 255, 230), std::max(1.0, radius * 0.35)));
    painter.setBrush(QColor(0, 155, 190, 210));
    for (int i = 0; i < snapshot.cells.size(); ++i) {
        const QPointF cell = snapshot.cells[i];
        const QPointF centerCell(cell.x() + 0.5, cell.y() + 0.5);
        if (!expandedVisible.contains(centerCell)) {
            continue;
        }
        const QPointF screen = worldToScreen(centerCell);
        const double markerRadius = (i == 0) ? radius * 1.25 : radius;
        painter.drawEllipse(screen, markerRadius, markerRadius);
    }
    painter.restore();
}

void MapView::drawSelectedAgentRewriteMarker(QPainter& painter, const QRectF& visible)
{
    if (!m_data || m_selectedAgent < 0 || m_selectedAgent >= m_data->teamSize()) {
        return;
    }

    ActionQueueSnapshot plannerSnapshot;
    ActionQueueSnapshot stagedSnapshot;
    if (!m_data->plannerActionsForAgent(m_selectedAgent, m_tick, &plannerSnapshot) ||
        !m_data->stagedActionsForAgent(m_selectedAgent, m_tick, &stagedSnapshot) ||
        !isCornerRewrite(plannerSnapshot.actions, stagedSnapshot.actions)) {
        return;
    }

    QPointF markerCell;
    StagedLocSnapshot locSnapshot;
    if (m_data->stagedLocsForAgent(m_selectedAgent, m_tick, &locSnapshot) &&
        !locSnapshot.cells.isEmpty()) {
        markerCell = locSnapshot.cells.front();
    } else {
        markerCell = m_data->frameAt(m_selectedAgent, m_tick).cell;
    }

    const QPointF centerCell(markerCell.x() + 0.5, markerCell.y() + 0.5);
    if (!visible.adjusted(-1.0, -1.0, 1.0, 1.0).contains(centerCell)) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QPointF center = worldToScreen(centerCell);
    const double w = std::clamp(m_scale * 0.92, 17.0, 28.0);
    const double h = std::clamp(m_scale * 0.52, 12.0, 18.0);
    const QRectF badge(center.x() + std::clamp(m_scale * 0.18, 4.0, 9.0),
                       center.y() - h - std::clamp(m_scale * 0.18, 4.0, 9.0),
                       w,
                       h);

    painter.setPen(QPen(QColor(255, 255, 255, 245), std::max(1.0, h * 0.13)));
    painter.setBrush(QColor(242, 140, 40, 238));
    painter.drawRoundedRect(badge, 4.0, 4.0);

    QFont font = painter.font();
    font.setBold(true);
    font.setPointSizeF(std::clamp(h * 0.46, 7.0, 10.5));
    painter.setFont(font);
    painter.setPen(QColor(35, 25, 12));
    painter.drawText(badge, Qt::AlignCenter, "RW");
    painter.restore();
}

void MapView::drawHighlightedTaskErrands(QPainter& painter, const QRectF& visible)
{
    if (!m_data || m_highlightedTask < 0) {
        return;
    }

    const QVector<QPointF>* errands = m_data->taskErrands(m_highlightedTask);
    if (errands == nullptr || errands->isEmpty()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QColor lineColor(235, 90, 45, 215);
    const QColor firstColor(250, 210, 65, 245);
    const QColor pointColor(235, 90, 45, 235);
    const double pointRadius = std::clamp(m_scale * 0.34, 4.0, 14.0);

    if (errands->size() >= 2) {
        QPainterPath path;
        bool started = false;
        for (const QPointF& cell : *errands) {
            const QPointF screen = worldToScreen(QPointF(cell.x() + 0.5, cell.y() + 0.5));
            if (!started) {
                path.moveTo(screen);
                started = true;
            } else {
                path.lineTo(screen);
            }
        }
        painter.setPen(QPen(lineColor, std::clamp(m_scale * 0.12, 2.0, 5.0),
                            Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }

    QFont font = painter.font();
    font.setPointSizeF(std::clamp(m_scale * 0.28, 7.0, 13.0));
    font.setBold(true);
    painter.setFont(font);
    for (int i = 0; i < errands->size(); ++i) {
        const QPointF cell = errands->at(i);
        const QPointF centerCell(cell.x() + 0.5, cell.y() + 0.5);
        if (!visible.adjusted(-1.0, -1.0, 1.0, 1.0).contains(centerCell)) {
            continue;
        }
        const QPointF center = worldToScreen(centerCell);
        painter.setPen(QPen(Qt::white, std::max(1.0, pointRadius * 0.18)));
        painter.setBrush(i == 0 ? firstColor : pointColor);
        painter.drawEllipse(center, pointRadius, pointRadius);
        painter.setPen(QPen(QColor(55, 45, 35), 1.0));
        painter.drawText(QRectF(center.x() - pointRadius, center.y() - pointRadius,
                                pointRadius * 2.0, pointRadius * 2.0),
                         Qt::AlignCenter, QString::number(i + 1));
    }

    painter.restore();
}

void MapView::drawSelectedAgentPath(QPainter& painter, const QRectF& visible)
{
    Q_UNUSED(painter);
    Q_UNUSED(visible);
    return;

    if (!m_data || m_selectedAgent < 0 || m_selectedAgent >= m_data->teamSize()) {
        return;
    }

    const AgentTrack& track = m_data->tracks()[m_selectedAgent];
    if (track.frames.size() < 2) {
        return;
    }

    const int first = std::clamp(m_tick, 0, static_cast<int>(track.frames.size()) - 1);
    const int last = static_cast<int>(track.frames.size()) - 1;
    QPainterPath path;
    bool hasPoint = false;
    QPointF lastCell(-1.0, -1.0);

    for (int tick = first; tick <= last; ++tick) {
        const QPointF cell = track.frames[tick].cell;
        if (tick != first &&
            std::abs(cell.x() - lastCell.x()) < 1e-6 &&
            std::abs(cell.y() - lastCell.y()) < 1e-6) {
            continue;
        }
        lastCell = cell;
        if (!visible.adjusted(-2, -2, 2, 2).contains(QPointF(cell.x() + 0.5, cell.y() + 0.5)) && hasPoint) {
            continue;
        }
        const QPointF screen = worldToScreen(QPointF(cell.x() + 0.5, cell.y() + 0.5));
        if (!hasPoint) {
            path.moveTo(screen);
            hasPoint = true;
        } else {
            path.lineTo(screen);
        }
    }

    if (!hasPoint) {
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(155, 65, 190, 210), std::clamp(m_scale * 0.18, 1.5, 5.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);

    const AgentFrame current = m_data->frameAt(m_selectedAgent, m_tick);
    const QPointF currentCenter = worldToScreen(QPointF(current.cell.x() + 0.5, current.cell.y() + 0.5));
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(155, 65, 190, 210));
    painter.drawEllipse(currentCenter, std::clamp(m_scale * 0.18, 2.0, 6.0), std::clamp(m_scale * 0.18, 2.0, 6.0));
}

void MapView::drawAllAgentGoalArrows(QPainter& painter, const QRectF& visible)
{
    if (!m_showAllGoalArrows || !m_data) {
        return;
    }

    const QRectF expandedVisible = visible.adjusted(-4, -4, 4, 4);
    for (int agent = 0; agent < m_data->teamSize(); ++agent) {
        QPointF goal;
        if (!m_data->nextGoalForAgent(agent, m_tick, &goal)) {
            continue;
        }
        const AgentFrame frame = m_data->frameAt(agent, m_tick);
        const QPointF current(frame.cell.x() + 0.5, frame.cell.y() + 0.5);
        const QPointF target(goal.x() + 0.5, goal.y() + 0.5);
        const QRectF lineBounds(current, target);
        if (!expandedVisible.intersects(lineBounds.normalized().adjusted(-1, -1, 1, 1))) {
            continue;
        }
        const QColor color = QColor::fromHsv((agent * 47) % 360, 85, 210);
        drawAgentGoalArrow(painter, agent, color, agent == m_selectedAgent ? 0.65 : 0.38);
    }
}

void MapView::drawSelectedAgentGoalArrow(QPainter& painter)
{
    if (!m_data || m_selectedAgent < 0 || m_selectedAgent >= m_data->teamSize()) {
        return;
    }

    drawAgentGoalArrow(painter, m_selectedAgent, QColor(18, 135, 150), 1.0);
}

void MapView::drawAgentGoalArrow(QPainter& painter, int agent, const QColor& color, double alphaScale)
{
    if (!m_data || agent < 0 || agent >= m_data->teamSize()) {
        return;
    }

    QPointF goal;
    if (!m_data->nextGoalForAgent(agent, m_tick, &goal)) {
        return;
    }

    const AgentFrame frame = m_data->frameAt(agent, m_tick);
    const QPointF start = worldToScreen(QPointF(frame.cell.x() + 0.5, frame.cell.y() + 0.5));
    const QPointF end = worldToScreen(QPointF(goal.x() + 0.5, goal.y() + 0.5));
    const QPointF delta = end - start;
    const double length = std::hypot(delta.x(), delta.y());
    if (length < 1.0) {
        return;
    }

    const QPointF unit(delta.x() / length, delta.y() / length);
    const double agentRadius = std::clamp(m_scale * 0.38, 2.0, std::min(18.0, m_scale * 0.46));
    const double goalInset = std::clamp(m_scale * 0.18, 3.0, 10.0);
    const QPointF lineStart = start + unit * (agentRadius + 2.0);
    const QPointF lineEnd = end - unit * goalInset;

    painter.setRenderHint(QPainter::Antialiasing, true);
    QColor lineColor = color;
    lineColor.setAlpha(std::clamp(static_cast<int>(230 * alphaScale), 35, 230));
    QPen pen(lineColor, std::clamp(m_scale * 0.13, 1.0, 4.0));
    pen.setStyle(Qt::DashLine);
    pen.setDashPattern({6.0, 5.0});
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(lineStart, lineEnd);

    const QPointF normal(-unit.y(), unit.x());
    const double headLen = std::clamp(m_scale * 0.45, 7.0, 18.0);
    const double headWidth = std::clamp(m_scale * 0.25, 4.0, 11.0);
    QPolygonF head;
    head << lineEnd
         << (lineEnd - unit * headLen + normal * headWidth)
         << (lineEnd - unit * headLen - normal * headWidth);
    painter.setPen(Qt::NoPen);
    painter.setBrush(lineColor);
    painter.drawPolygon(head);

    QColor markerFill = color;
    markerFill.setAlpha(std::clamp(static_cast<int>(70 * alphaScale), 16, 120));
    painter.setBrush(markerFill);
    painter.setPen(QPen(lineColor, std::max(1.0, m_scale * 0.1)));
    const double marker = std::clamp(m_scale * 0.35, 4.0, 12.0);
    painter.drawEllipse(end, marker, marker);
}
