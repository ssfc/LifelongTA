#include "PlanData.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPoint>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <limits>

bool PlanData::loadMap(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *error = "Cannot open map file: " + path;
        return false;
    }

    QTextStream in(&file);
    in.readLine();
    const auto heightParts = in.readLine().split(' ', Qt::SkipEmptyParts);
    const auto widthParts = in.readLine().split(' ', Qt::SkipEmptyParts);
    if (heightParts.size() < 2 || widthParts.size() < 2) {
        *error = "Invalid map header.";
        return false;
    }
    m_height = heightParts[1].toInt();
    m_width = widthParts[1].toInt();
    in.readLine();

    m_map.clear();
    m_map.reserve(m_width * m_height);
    for (int row = 0; row < m_height; ++row) {
        const QString line = in.readLine().trimmed();
        if (line.size() < m_width) {
            *error = QString("Map row %1 is too short.").arg(row);
            return false;
        }
        for (int col = 0; col < m_width; ++col) {
            const QChar ch = line[col];
            m_map.push_back((ch == '.' || ch == 'S' || ch == 'E') ? 0 : 1);
        }
    }

    m_mapPath = QFileInfo(path).absoluteFilePath();
    return true;
}

bool PlanData::loadLifelongTrace(const QString& path, int agentLimit, int startTick, int endTick, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = "Cannot open LifelongTA trace: " + path;
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        *error = "Invalid LifelongTA trace: " + parseError.errorString();
        return false;
    }
    const QJsonObject root = doc.object();
    if (root.value("format").toString() != "lifelongta-debug-trace-v1") {
        *error = "Not a lifelongta-debug-trace-v1 file.";
        return false;
    }

    m_height = root.value("rows").toInt();
    m_width = root.value("cols").toInt();
    const QJsonArray map = root.value("map").toArray();
    const QJsonArray frames = root.value("frames").toArray();
    if (m_width <= 0 || m_height <= 0 || map.size() != m_width * m_height || frames.isEmpty()) {
        *error = "Trace must contain a complete map and at least one frame.";
        return false;
    }

    m_map.clear();
    m_map.reserve(map.size());
    for (const QJsonValue& value : map) {
        m_map.push_back(static_cast<unsigned char>(value.toInt() == 0 ? 0 : 1));
    }
    m_mapPath = QFileInfo(path).absoluteFilePath();
    m_tracks.clear();
    m_tasks.clear();
    m_agentAssignments.clear();
    m_agentTaskTimeline.clear();
    m_taskProgressTimeline.clear();
    m_sandboxGoalTimeline.clear();
    m_plannerPriorityTimeline.clear();
    m_guidePathTimeline.clear();
    m_assignmentSnapshots.clear();
    m_plannerActionTimeline.clear();
    m_stagedActionBeforeTimeline.clear();
    m_stagedActionTimeline.clear();
    m_stagedLocTimeline.clear();
    m_tpgSnapshots.clear();
    m_pibtTraceTimeline.clear();
    m_delayIntervals.clear();
    m_occupancyHeat.clear();
    m_occupancyHeatBuilt = false;
    m_maxOccupancyHeat = 0;
    m_waitHeat.clear();
    m_waitHeatBuilt = false;
    m_maxWaitHeat = 0;
    m_debugOverlays.clear();
    m_debugDirectionMarkers.clear();
    m_highwayDirections.clear();
    m_directionMapSnapshots.clear();
    m_finishedTaskTicks.clear();
    m_finishedTasks = 0;
    m_isSandboxPlan = false;
    m_hasAgentOrientations = true;
    m_ticksPerTimestep = 1;

    int maxAgentId = -1;
    int maxTaskId = -1;
    for (const QJsonValue& frameValue : frames) {
        const QJsonObject frame = frameValue.toObject();
        for (const QJsonValue& agentValue : frame.value("agents").toArray()) {
            const int id = agentValue.toObject().value("id").toInt(-1);
            if (id >= 0 && (agentLimit <= 0 || id < agentLimit)) {
                maxAgentId = std::max(maxAgentId, id);
            }
        }
        for (const QJsonValue& taskValue : frame.value("tasks").toArray()) {
            maxTaskId = std::max(maxTaskId, taskValue.toObject().value("id").toInt(-1));
        }
    }
    if (maxAgentId < 0) {
        *error = "Trace contains no captured agents.";
        return false;
    }

    const int frameCount = frames.size();
    m_displayTickOffset = frames.at(0).toObject().value("timestep").toInt();
    m_maxTick = std::max(0, frameCount - 1);
    m_tracks.resize(maxAgentId + 1);
    m_agentTaskTimeline.resize(maxAgentId + 1);
    m_taskProgressTimeline.resize(maxAgentId + 1);
    m_tasks.resize(maxTaskId + 1);

    auto cellForLocation = [this](int location) {
        return QPointF(location % m_width, location / m_width);
    };
    auto validLocation = [this](int location) {
        return location >= 0 && location < m_width * m_height;
    };
    auto setTask = [&](const QJsonObject& value) {
        const int taskId = value.value("id").toInt(-1);
        if (taskId < 0 || taskId >= m_tasks.size()) {
            return;
        }
        TaskInfo& task = m_tasks[taskId];
        task.id = taskId;
        if (!task.errands.isEmpty()) {
            return;
        }
        for (const QJsonValue& locationValue : value.value("locations").toArray()) {
            const int location = locationValue.toInt(-1);
            if (validLocation(location)) {
                task.errands.push_back(cellForLocation(location));
            }
        }
    };

    QVector<int> currentTasks(maxAgentId + 1, -2);
    QVector<int> currentLocations(maxAgentId + 1, -1);
    QSet<int> finishedTasks;
    for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
        const QJsonObject frame = frames[frameIndex].toObject();
        const int tick = frame.value("timestep").toInt(m_displayTickOffset + frameIndex);
        const int relativeTick = tick - m_displayTickOffset;
        QHash<int, QJsonObject> tasksAtFrame;
        for (const QJsonValue& taskValue : frame.value("tasks").toArray()) {
            const QJsonObject taskObject = taskValue.toObject();
            setTask(taskObject);
            tasksAtFrame.insert(taskObject.value("id").toInt(-1), taskObject);
        }

        QVector<int> freeAgents;
        for (const QJsonValue& agentValue : frame.value("agents").toArray()) {
            const QJsonObject agent = agentValue.toObject();
            const int id = agent.value("id").toInt(-1);
            const int location = agent.value("location").toInt(-1);
            if (id < 0 || id >= m_tracks.size() || !validLocation(location)) {
                continue;
            }
            AgentTrack& track = m_tracks[id];
            const QPointF cell = cellForLocation(location);
            if (frameIndex == 0) {
                track.start = cell;
                track.startDir = 0.0;
                track.frames.push_back(AgentFrame{cell, 0.0});
            } else {
                while (track.frames.size() < frameIndex) {
                    track.frames.push_back(track.frames.isEmpty() ? AgentFrame{cell, 0.0} : track.frames.back());
                }
                const QPointF previous = track.frames.isEmpty() ? cell : track.frames.back().cell;
                double direction = track.frames.isEmpty() ? 0.0 : track.frames.back().dir;
                const int delta = location - currentLocations[id];
                if (delta == 1) direction = 0.0;
                else if (delta == m_width) direction = 1.0;
                else if (delta == -1) direction = 2.0;
                else if (delta == -m_width) direction = 3.0;
                Q_UNUSED(previous);
                track.frames.push_back(AgentFrame{cell, direction});
                if (delta == 1) track.actions.push_back('E');
                else if (delta == m_width) track.actions.push_back('S');
                else if (delta == -1) track.actions.push_back('A');
                else if (delta == -m_width) track.actions.push_back('N');
                else track.actions.push_back('W');
            }
            currentLocations[id] = location;

            const int task = agent.value("task").toInt(-1);
            if (task != currentTasks[id]) {
                m_agentTaskTimeline[id].push_back(qMakePair(relativeTick, task));
                if (currentTasks[id] >= 0 && task < 0) {
                    finishedTasks.insert(currentTasks[id]);
                    m_finishedTaskTicks.push_back(relativeTick);
                }
                currentTasks[id] = task;
            }
            if (task < 0) {
                freeAgents.push_back(id);
            }
            const auto taskIt = tasksAtFrame.constFind(task);
            if (taskIt != tasksAtFrame.constEnd()) {
                m_taskProgressTimeline[id].push_back(TaskProgressEvent{
                    relativeTick, task, taskIt.value().value("nextLocation").toInt(0)});
            }
        }
        for (int id = 0; id < m_tracks.size(); ++id) {
            AgentTrack& track = m_tracks[id];
            if (track.frames.isEmpty()) {
                continue;
            }
            while (track.frames.size() <= frameIndex) {
                track.frames.push_back(track.frames.back());
                track.actions.push_back('W');
            }
        }

        AssignmentSnapshot snapshot;
        snapshot.tick = tick;
        snapshot.freeAgents = std::move(freeAgents);
        for (auto it = tasksAtFrame.constBegin(); it != tasksAtFrame.constEnd(); ++it) {
            const QJsonObject& task = it.value();
            if (task.value("assignedAgent").toInt(-1) < 0) {
                snapshot.freeTasks.push_back(it.key());
            }
        }
        std::sort(snapshot.freeTasks.begin(), snapshot.freeTasks.end());
        m_assignmentSnapshots.push_back(std::move(snapshot));
    }
    for (AgentTrack& track : m_tracks) {
        if (track.frames.isEmpty()) {
            track.start = QPointF();
            track.frames.fill(AgentFrame{}, frameCount);
        }
        while (track.frames.size() < frameCount) {
            track.frames.push_back(track.frames.back());
        }
        while (track.actions.size() < m_maxTick) {
            track.actions.push_back('W');
        }
    }
    m_finishedTasks = finishedTasks.size();
    std::sort(m_finishedTaskTicks.begin(), m_finishedTaskTicks.end());
    m_startTick = startTick > 0 ? std::max(startTick, m_displayTickOffset) : m_displayTickOffset;
    const int displayMaxTick = m_displayTickOffset + m_maxTick;
    m_endTick = endTick > 0 ? std::min(endTick, displayMaxTick) : displayMaxTick;
    if (m_endTick < m_startTick) {
        m_endTick = m_startTick;
    }
    return true;
}

bool PlanData::loadPlan(const QString& path, int agentLimit, int startTick, int endTick, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = "Cannot open plan file: " + path;
        return false;
    }

    const QJsonParseError parseError{};
    QJsonParseError actualParseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &actualParseError);
    Q_UNUSED(parseError);
    if (actualParseError.error != QJsonParseError::NoError || !doc.isObject()) {
        *error = "Invalid JSON plan: " + actualParseError.errorString();
        return false;
    }

    const QJsonObject root = doc.object();
    const QJsonArray starts = root.value("start").toArray();
    const QJsonArray actualPaths = root.value("actualPaths").toArray();
    if (starts.isEmpty() || actualPaths.isEmpty()) {
        *error = "Plan must contain start and actualPaths arrays.";
        return false;
    }

    const int startsSize = static_cast<int>(starts.size());
    const int actualPathsSize = static_cast<int>(actualPaths.size());
    const int requestedAgents = agentLimit > 0 ? agentLimit : startsSize;
    const int count = std::min({requestedAgents, startsSize, actualPathsSize});
    m_tracks.clear();
    m_tracks.reserve(count);
    m_tasks.clear();
    m_agentAssignments.clear();
    m_agentTaskTimeline.clear();
    m_taskProgressTimeline.clear();
    m_sandboxGoalTimeline.clear();
    m_plannerPriorityTimeline.clear();
    m_guidePathTimeline.clear();
    m_assignmentSnapshots.clear();
    m_plannerActionTimeline.clear();
    m_stagedActionBeforeTimeline.clear();
    m_stagedActionTimeline.clear();
    m_stagedLocTimeline.clear();
    m_pibtTraceTimeline.clear();
    m_delayIntervals.clear();
    m_occupancyHeat.clear();
    m_occupancyHeatBuilt = false;
    m_maxOccupancyHeat = 0;
    m_occupancyHeatLogLow = 0.0;
    m_occupancyHeatLogHigh = 0.0;
    m_waitHeat.clear();
    m_waitHeatBuilt = false;
    m_maxWaitHeat = 0;
    m_waitHeatLogLow = 0.0;
    m_waitHeatLogHigh = 0.0;
    m_debugOverlays.clear();
    m_debugDirectionMarkers.clear();
    m_highwayDirections.clear();
    m_directionMapSnapshots.clear();
    m_maxTick = 0;
    m_displayTickOffset = root.value("resumeStartTick").toInt(0);
    m_finishedTasks = root.value("numTaskFinished").toInt();
    m_isSandboxPlan = root.value("version").toString().contains("sandbox", Qt::CaseInsensitive) ||
                      root.contains("sandbox");
    const QString actionModel = root.value("actionModel").toString();
    m_hasAgentOrientations = !actionModel.isEmpty() &&
                             !actionModel.contains("DIRECT", Qt::CaseInsensitive);
    m_ticksPerTimestep = root.value("agentMaxCounter").toInt(1);
    if (m_ticksPerTimestep <= 0) {
        m_ticksPerTimestep = 1;
    }

    for (int i = 0; i < count; ++i) {
        const QJsonArray start = starts[i].toArray();
        if (start.size() < 3) {
            *error = QString("Invalid start entry for agent %1.").arg(i);
            return false;
        }
        AgentTrack track;
        track.start = QPointF(start[1].toDouble(), start[0].toDouble());
        track.startDir = start.size() >= 4 ? start[3].toDouble(directionFromString(start[2].toString()))
                                           : directionFromString(start[2].toString());
        if (!loadTrackPath(actualPaths[i].toString(), &track, error)) {
            *error = QString("Agent %1: %2").arg(i).arg(*error);
            return false;
        }
        repairObstacleFrames(&track);
        if (actionModel.isEmpty()) {
            for (unsigned char action : track.actions) {
                if (action == 'F' || action == 'R' || action == 'C') {
                    m_hasAgentOrientations = true;
                    break;
                }
            }
        }
        m_maxTick = std::max(m_maxTick, static_cast<int>(track.actions.size()));
        m_tracks.push_back(std::move(track));
    }

    const int declaredMakespan = root.value("makespan").toInt(0);
    if (declaredMakespan > 0) {
        const int relativeMakespan = m_displayTickOffset > 0 && declaredMakespan > m_displayTickOffset
            ? declaredMakespan - m_displayTickOffset
            : declaredMakespan;
        m_maxTick = std::max(m_maxTick, relativeMakespan);
        for (AgentTrack& track : m_tracks) {
            if (track.frames.isEmpty()) {
                track.frames.push_back(AgentFrame{track.start, track.startDir});
            }
            while (track.frames.size() <= m_maxTick) {
                track.frames.push_back(track.frames.back());
            }
        }
    }

    loadTasksAndAssignments(root);
    loadScheduleTimeline(root);
    loadSandboxGoalTimeline(root);
    loadPlannerPriorityTimeline(root, count);
    loadGuidePathTimeline(root, count);
    loadAssignmentSnapshots(root);
    loadActionSnapshotTimeline(root, "plannerActionSnapshots", count, &m_plannerActionTimeline);
    loadActionSnapshotTimeline(root, "stagedActionBeforeSnapshots", count, &m_stagedActionBeforeTimeline);
    loadActionSnapshotTimeline(root, "stagedActionSnapshots", count, &m_stagedActionTimeline);
    loadStagedLocSnapshots(root, count);
    // Executor-state anchoring is only a debug refinement. On large ORZ views it
    // repeatedly expands executorStateLocs and can delay the first window by minutes.
    if (count <= 1000 && m_maxTick <= 5000) {
        anchorFramesToExecutorStateLocs(root, count);
    }
    loadTpgSnapshots(root);
    loadPibtTraceTimeline(root);
    loadDelayIntervals(root, count);
    loadDebugOverlays(root);
    loadDebugDirections(root);
    loadHighwayDirections(root);
    loadDirectionMap(root);

    const int displayMaxTick = m_displayTickOffset + m_maxTick;
    m_startTick = startTick > 0 ? std::max(m_displayTickOffset, startTick) : m_displayTickOffset;
    m_endTick = endTick > 0 ? std::min(endTick, displayMaxTick) : displayMaxTick;
    if (m_endTick < m_startTick) {
        m_endTick = m_startTick;
    }

    return true;
}

void PlanData::ensureOccupancyHeat()
{
    if (!m_occupancyHeatBuilt) {
        buildOccupancyHeat();
        m_occupancyHeatBuilt = true;
    }
}

void PlanData::ensureWaitHeat()
{
    if (!m_waitHeatBuilt) {
        buildWaitHeat();
        m_waitHeatBuilt = true;
    }
}

int PlanData::directionFromString(const QString& value)
{
    if (value == "E") return 0;
    if (value == "S") return 1;
    if (value == "W") return 2;
    if (value == "N") return 3;
    return 0;
}

QVector<unsigned char> PlanData::decodePath(const QString& path, QString* error)
{
    error->clear();
    QVector<unsigned char> actions;

    if (path.trimmed().startsWith('[')) {
        static const QRegularExpression chunkRe(R"(\[\(([^)]*)\):\(([^)]*)\)\])");
        auto it = chunkRe.globalMatch(path);
        int parsed = 0;
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            const QString body = match.captured(2);
            const auto runs = body.split(',', Qt::SkipEmptyParts);
            for (const QString& rawRun : runs) {
                const auto parts = rawRun.trimmed().split(' ', Qt::SkipEmptyParts);
                if (parts.isEmpty()) {
                    continue;
                }
                const QChar action = parts[0][0];
                const int length = parts.size() >= 2 ? parts[1].toInt() : 1;
                if (length < 0) {
                    *error = "Negative run length in path.";
                    return {};
                }
                for (int i = 0; i < length; ++i) {
                    actions.push_back(action.toLatin1());
                }
            }
            ++parsed;
        }
        if (parsed == 0) {
            *error = "Segmented path was not parsed.";
        }
        return actions;
    }

    const auto parts = path.split(',', Qt::SkipEmptyParts);
    actions.reserve(parts.size());
    for (const QString& part : parts) {
        const QString motion = part.trimmed();
        if (!motion.isEmpty()) {
            actions.push_back(motion[0].toLatin1());
        }
    }
    return actions;
}

bool PlanData::loadTrackPath(const QString& path, AgentTrack* track, QString* error) const
{
    error->clear();
    track->actions.clear();
    track->frames.clear();

    if (path.trimmed().startsWith('[')) {
        static const QRegularExpression chunkRe(R"(\[\(([^)]*)\):\(([^)]*)\)\])");
        auto it = chunkRe.globalMatch(path);
        int parsed = 0;
        AgentFrame firstHeaderFrame;
        int firstHeaderCounter = 0;
        bool hasFirstHeader = false;
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            const QStringList head = match.captured(1).split(',', Qt::SkipEmptyParts);
            if (head.size() < 4) {
                *error = "Segmented path chunk header is incomplete.";
                return false;
            }

            bool okTick = false;
            bool okRow = false;
            bool okCol = false;
            bool okDir = false;
            bool okCounter = true;
            const int chunkTick = head[0].trimmed().toInt(&okTick);
            const double row = head[1].trimmed().toDouble(&okRow);
            const double col = head[2].trimmed().toDouble(&okCol);
            const double dir = head[3].trimmed().toDouble(&okDir);
            const int counter = head.size() >= 5 ? head[4].trimmed().toInt(&okCounter) : 0;
            const QString currentActionToken = head.size() >= 6 ? head[5].trimmed() : QString();
            if (!okTick || !okRow || !okCol || !okDir || !okCounter || chunkTick < 0) {
                *error = "Invalid segmented path chunk header.";
                return false;
            }

            AgentFrame headerFrame{QPointF(col, row), dir};
            const auto runs = match.captured(2).split(',', Qt::SkipEmptyParts);
            if (counter > 0 && m_ticksPerTimestep > 0) {
                QChar inProgressAction;
                if (!currentActionToken.isEmpty()) {
                    inProgressAction = currentActionToken[0];
                } else if (!runs.isEmpty()) {
                    const auto firstParts = runs[0].trimmed().split(' ', Qt::SkipEmptyParts);
                    if (!firstParts.isEmpty()) {
                        inProgressAction = firstParts[0][0];
                    }
                }
                if (!inProgressAction.isNull()) {
                    const double progress = std::clamp(
                        static_cast<double>(counter) / static_cast<double>(m_ticksPerTimestep),
                        0.0,
                        1.0);
                    if (inProgressAction == 'F') {
                        switch (static_cast<int>(std::round(headerFrame.dir)) & 3) {
                        case 0:
                            headerFrame.cell.rx() += progress;
                            break;
                        case 1:
                            headerFrame.cell.ry() += progress;
                            break;
                        case 2:
                            headerFrame.cell.rx() -= progress;
                            break;
                        case 3:
                            headerFrame.cell.ry() -= progress;
                            break;
                        }
                    } else if (inProgressAction == 'R') {
                        headerFrame.dir += progress;
                    } else if (inProgressAction == 'C') {
                        headerFrame.dir -= progress;
                    }
                }
            }

            const bool headerRepeatsFirst =
                hasFirstHeader &&
                std::abs(headerFrame.cell.x() - firstHeaderFrame.cell.x()) < 1e-9 &&
                std::abs(headerFrame.cell.y() - firstHeaderFrame.cell.y()) < 1e-9 &&
                std::abs(headerFrame.dir - firstHeaderFrame.dir) < 1e-9 &&
                counter == firstHeaderCounter;

            AgentFrame frame = headerFrame;
            if (track->frames.isEmpty()) {
                track->frames.resize(chunkTick + 1);
                for (int i = 0; i <= chunkTick; ++i) {
                    track->frames[i] = frame;
                }
                firstHeaderFrame = headerFrame;
                firstHeaderCounter = counter;
                hasFirstHeader = true;
            } else {
                while (track->frames.size() <= chunkTick) {
                    track->frames.push_back(track->frames.back());
                }
                const AgentFrame predictedFrame = track->frames[chunkTick];
                const double dx = predictedFrame.cell.x() - headerFrame.cell.x();
                const double dy = predictedFrame.cell.y() - headerFrame.cell.y();
                const double distance = std::hypot(dx, dy);
                constexpr double kReliableChunkHeaderDistance = 1.5;
                const bool staleRepeatedHeader =
                    headerRepeatsFirst && chunkTick > 0 && distance > kReliableChunkHeaderDistance;
                if (staleRepeatedHeader) {
                    frame = predictedFrame;
                } else if (distance > kReliableChunkHeaderDistance) {
                    frame = headerFrame;
                } else {
                    frame = predictedFrame;
                }
                track->frames[chunkTick] = frame;
            }

            for (const QString& rawRun : runs) {
                const auto parts = rawRun.trimmed().split(' ', Qt::SkipEmptyParts);
                if (parts.isEmpty()) {
                    continue;
                }
                const QChar action = parts[0][0];
                const int length = parts.size() >= 2 ? parts[1].toInt() : 1;
                if (length < 0) {
                    *error = "Negative run length in path.";
                    return false;
                }
                for (int i = 0; i < length; ++i) {
                    const unsigned char encodedAction = action.toLatin1();
                    track->actions.push_back(encodedAction);
                    applyAction(frame, encodedAction);
                    track->frames.push_back(frame);
                }
            }
            ++parsed;
        }
        if (parsed == 0) {
            *error = "Segmented path was not parsed.";
            return false;
        }
        return true;
    }

    track->actions = decodePath(path, error);
    if (!error->isEmpty()) {
        return false;
    }

    track->frames.reserve(track->actions.size() + 1);
    AgentFrame frame{track->start, track->startDir};
    track->frames.push_back(frame);
    for (unsigned char action : track->actions) {
        applyAction(frame, action);
        track->frames.push_back(frame);
    }
    return true;
}

void PlanData::repairObstacleFrames(AgentTrack* track) const
{
    if (track == nullptr || m_width <= 0 || m_height <= 0 || m_map.isEmpty()) {
        return;
    }

    const auto isFree = [&](int row, int col) {
        return row >= 0 && row < m_height &&
               col >= 0 && col < m_width &&
               m_map[row * m_width + col] == 0;
    };

    for (AgentFrame& frame : track->frames) {
        const int floorCol = static_cast<int>(std::floor(frame.cell.x() + 1e-9));
        const int floorRow = static_cast<int>(std::floor(frame.cell.y() + 1e-9));
        if (isFree(floorRow, floorCol)) {
            continue;
        }

        const int roundCol = static_cast<int>(std::floor(frame.cell.x() + 0.5));
        const int roundRow = static_cast<int>(std::floor(frame.cell.y() + 0.5));
        double bestDistance = std::numeric_limits<double>::infinity();
        QPoint best;
        bool found = false;

        constexpr int kMaxRepairRadius = 40;
        for (int radius = 0; radius <= kMaxRepairRadius && !found; radius++) {
            for (int row = roundRow - radius; row <= roundRow + radius; row++) {
                for (int col = roundCol - radius; col <= roundCol + radius; col++) {
                    if (std::max(std::abs(row - roundRow), std::abs(col - roundCol)) != radius) {
                        continue;
                    }
                    if (!isFree(row, col)) {
                        continue;
                    }
                    const double dx = frame.cell.x() - static_cast<double>(col);
                    const double dy = frame.cell.y() - static_cast<double>(row);
                    const double distance = dx * dx + dy * dy;
                    if (!found || distance < bestDistance) {
                        found = true;
                        bestDistance = distance;
                        best = QPoint(col, row);
                    }
                }
            }
        }

        if (found) {
            frame.cell = QPointF(best.x(), best.y());
        }
    }
}

void PlanData::applyAction(AgentFrame& frame, unsigned char action) const
{
    const double frac = 1.0 / static_cast<double>(m_ticksPerTimestep);
    auto inBounds = [&](const QPointF& cell) {
        constexpr double eps = 1e-6;
        return cell.x() >= -eps && cell.y() >= -eps &&
               cell.x() <= static_cast<double>(m_width - 1) + eps &&
               cell.y() <= static_cast<double>(m_height - 1) + eps;
    };
    switch (action) {
    case 'F':
    {
        QPointF nextCell = frame.cell;
        switch (static_cast<int>(std::round(frame.dir)) & 3) {
        case 0:
            nextCell.rx() += frac;
            break;
        case 1:
            nextCell.ry() += frac;
            break;
        case 2:
            nextCell.rx() -= frac;
            break;
        case 3:
            nextCell.ry() -= frac;
            break;
        }
        if (inBounds(nextCell)) {
            frame.cell = nextCell;
        }
        break;
    }
    case 'E':
        frame.dir = 0.0;
        frame.cell.rx() += 1.0;
        break;
    case 'S':
        frame.dir = 1.0;
        frame.cell.ry() += 1.0;
        break;
    case 'A':
        frame.dir = 2.0;
        frame.cell.rx() -= 1.0;
        break;
    case 'N':
        frame.dir = 3.0;
        frame.cell.ry() -= 1.0;
        break;
    case 'R':
        frame.dir += frac;
        while (frame.dir >= 4.0) frame.dir -= 4.0;
        break;
    case 'C':
        frame.dir -= frac;
        while (frame.dir < 0.0) frame.dir += 4.0;
        break;
    case 'W':
    case 'T':
    default:
        break;
    }
}

void PlanData::anchorFramesToExecutorStateLocs(const QJsonObject& root, int agentCount)
{
    const QJsonArray stateLocs = root.value("executorStateLocs").toArray();
    if (stateLocs.isEmpty() || m_width <= 0) {
        return;
    }

    const int tickCount = static_cast<int>(stateLocs.size());
    auto locForAgentAtTick = [&](int tick, int agent) -> int {
        if (tick < 0 || tick >= tickCount) {
            return -1;
        }
        const QJsonArray locs = stateLocs[tick].toArray();
        if (agent >= locs.size()) {
            return -1;
        }
        return locs[agent].toInt(-1);
    };
    auto nearestLocForAgentAtTick = [&](int tick, int agent) -> int {
        const int exact = locForAgentAtTick(tick, agent);
        if (exact >= 0) {
            return exact;
        }

        int prevLoc = -1;
        for (int prevTick = tick - 1; prevTick >= 0; --prevTick) {
            prevLoc = locForAgentAtTick(prevTick, agent);
            if (prevLoc >= 0) {
                break;
            }
        }

        int nextLoc = -1;
        for (int nextTick = tick + 1; nextTick < tickCount; ++nextTick) {
            nextLoc = locForAgentAtTick(nextTick, agent);
            if (nextLoc >= 0) {
                break;
            }
        }

        if (prevLoc >= 0 && prevLoc == nextLoc) {
            return prevLoc;
        }
        if (prevLoc >= 0) {
            return prevLoc;
        }
        return nextLoc;
    };
    auto dirForAdjacentDelta = [&](int delta) -> int {
        if (delta == 1) {
            return 0;
        }
        if (delta == m_width) {
            return 1;
        }
        if (delta == -1) {
            return 2;
        }
        if (delta == -m_width) {
            return 3;
        }
        return -1;
    };
    auto unitForDir = [](int dir) -> QPointF {
        switch (dir & 3) {
        case 0:
            return QPointF(1.0, 0.0);
        case 1:
            return QPointF(0.0, 1.0);
        case 2:
            return QPointF(-1.0, 0.0);
        case 3:
            return QPointF(0.0, -1.0);
        default:
            return QPointF(0.0, 0.0);
        }
    };

    for (int agent = 0; agent < agentCount && agent < m_tracks.size(); ++agent) {
        AgentTrack& track = m_tracks[agent];
        for (int relTick = 0; relTick < track.frames.size(); ++relTick) {
            const int tick = relTick + m_displayTickOffset;
            if (tick < 0 || tick >= tickCount) {
                continue;
            }

            const int loc = nearestLocForAgentAtTick(tick, agent);
            if (loc < 0) {
                continue;
            }

            const int row = loc / m_width;
            const int col = loc % m_width;

            const bool currentActionIsForward =
                (relTick < track.actions.size() && track.actions[relTick] == 'F') ||
                (relTick > 0 && relTick - 1 < track.actions.size() && track.actions[relTick - 1] == 'F');
            if (currentActionIsForward && m_ticksPerTimestep > 1) {
                int forwardDir = -1;
                int ticksUntilArrival = -1;
                for (int lookahead = 1; lookahead <= m_ticksPerTimestep && tick + lookahead < tickCount; ++lookahead) {
                    const int nextLoc = nearestLocForAgentAtTick(tick + lookahead, agent);
                    if (nextLoc >= 0 && nextLoc != loc) {
                        forwardDir = dirForAdjacentDelta(nextLoc - loc);
                        if (forwardDir >= 0) {
                            ticksUntilArrival = lookahead;
                        }
                        break;
                    }
                }

                if (forwardDir >= 0 && ticksUntilArrival >= 0) {
                    const double progress = std::clamp(
                        static_cast<double>(m_ticksPerTimestep - ticksUntilArrival) /
                            static_cast<double>(m_ticksPerTimestep),
                        0.0,
                        1.0);
                    const QPointF unit = unitForDir(forwardDir);
                    track.frames[relTick].cell = QPointF(static_cast<double>(col), static_cast<double>(row)) +
                                                 unit * progress;
                    track.frames[relTick].dir = static_cast<double>(forwardDir);
                    continue;
                }

                for (int lookbehind = 1; lookbehind <= m_ticksPerTimestep && tick - lookbehind >= 0; ++lookbehind) {
                    const int prevLoc = nearestLocForAgentAtTick(tick - lookbehind, agent);
                    if (prevLoc >= 0 && prevLoc != loc) {
                        forwardDir = dirForAdjacentDelta(loc - prevLoc);
                        if (forwardDir >= 0) {
                            track.frames[relTick].dir = static_cast<double>(forwardDir);
                        }
                        break;
                    }
                }
            }

            const double distanceToExecutorLoc = std::hypot(
                track.frames[relTick].cell.x() - static_cast<double>(col),
                track.frames[relTick].cell.y() - static_cast<double>(row));
            constexpr double kReliableExecutorFrameDistance = 1.5;
            if (distanceToExecutorLoc > kReliableExecutorFrameDistance) {
                track.frames[relTick].cell = QPointF(col, row);
                continue;
            }

            bool movingHorizontal = false;
            bool movingVertical = false;
            const double fractionalX = std::abs(track.frames[relTick].cell.x() - static_cast<double>(col));
            const double fractionalY = std::abs(track.frames[relTick].cell.y() - static_cast<double>(row));
            constexpr double kFractionalEpsilon = 1e-6;
            movingHorizontal = movingHorizontal || fractionalX > kFractionalEpsilon;
            movingVertical = movingVertical || fractionalY > kFractionalEpsilon;

            if (tick + 1 < tickCount) {
                const int nextLoc = nearestLocForAgentAtTick(tick + 1, agent);
                if (nextLoc >= 0) {
                    const int delta = nextLoc - loc;
                    movingHorizontal = movingHorizontal || std::abs(delta) == 1;
                    movingVertical = movingVertical || std::abs(delta) == m_width;
                }
            }
            if (tick > 0) {
                const int prevLoc = nearestLocForAgentAtTick(tick - 1, agent);
                if (prevLoc >= 0) {
                    const int delta = loc - prevLoc;
                    movingHorizontal = movingHorizontal || std::abs(delta) == 1;
                    movingVertical = movingVertical || std::abs(delta) == m_width;
                }
            }

            const int prevLoc = tick > 0 ? nearestLocForAgentAtTick(tick - 1, agent) : loc;
            const int nextLoc = tick + 1 < tickCount ? nearestLocForAgentAtTick(tick + 1, agent) : loc;
            if (prevLoc == loc && nextLoc == loc && distanceToExecutorLoc > kFractionalEpsilon) {
                track.frames[relTick].cell = QPointF(col, row);
                continue;
            }

            if (!movingHorizontal && !movingVertical &&
                relTick > 0 &&
                relTick - 1 < track.actions.size() &&
                track.actions[relTick - 1] == 'F') {
                const int dir = static_cast<int>(std::round(track.frames[relTick].dir)) & 3;
                movingHorizontal = (dir == 0 || dir == 2);
                movingVertical = (dir == 1 || dir == 3);
            }

            if (movingVertical && !movingHorizontal) {
                track.frames[relTick].cell.setX(col);
            } else if (movingHorizontal && !movingVertical) {
                track.frames[relTick].cell.setY(row);
            } else {
                track.frames[relTick].cell = QPointF(col, row);
            }
        }
    }
}

AgentFrame PlanData::frameAt(int agentId, int tick) const
{
    const AgentTrack& track = m_tracks[agentId];
    if (track.frames.isEmpty()) {
        return AgentFrame{track.start, track.startDir};
    }
    const int index = std::clamp(relativeTick(tick), 0, static_cast<int>(track.frames.size()) - 1);
    return track.frames[index];
}

const QVector<QPointF>* PlanData::taskErrands(int taskId) const
{
    if (taskId < 0 || taskId >= m_tasks.size() || m_tasks[taskId].errands.isEmpty()) {
        return nullptr;
    }
    return &m_tasks[taskId].errands;
}

bool PlanData::nextGoalForAgent(int agentId, int tick, QPointF* goal) const
{
    if (goal == nullptr || agentId < 0 || agentId >= m_tracks.size()) {
        return false;
    }

    if (agentId < m_sandboxGoalTimeline.size() && !m_sandboxGoalTimeline[agentId].isEmpty()) {
        const int timestep = std::clamp(tick, 0,
                                        static_cast<int>(m_sandboxGoalTimeline[agentId].size()) - 1);
        const QPointF sandboxGoal = m_sandboxGoalTimeline[agentId][timestep];
        if (sandboxGoal.x() >= 0.0 && sandboxGoal.y() >= 0.0) {
            *goal = sandboxGoal;
            return true;
        }
    }

    int activeTask = -1;
    int assignedTick = 0;
    if (!scheduledTaskAt(agentId, tick, &activeTask, &assignedTick)) {
        return false;
    }
    if (activeTask < 0 || activeTask >= m_tasks.size()) {
        return false;
    }

    const TaskInfo& task = m_tasks[activeTask];
    const int nextErrand = taskProgressForAgent(agentId, activeTask, tick);
    if (nextErrand >= 0 && nextErrand < task.errands.size()) {
        *goal = task.errands[nextErrand];
        return true;
    }

    const int relTick = relativeTick(tick);
    for (const QPointF& errand : task.errands) {
        if (!trackReachedGoal(agentId, assignedTick, relTick, errand)) {
            *goal = errand;
            return true;
        }
    }
    return false;
}

int PlanData::currentTaskForAgent(int agentId, int tick) const
{
    int task = -1;
    int assignedTick = 0;
    if (!scheduledTaskAt(agentId, tick, &task, &assignedTick)) {
        return -1;
    }
    Q_UNUSED(assignedTick);
    return task;
}

bool PlanData::currentTaskProgressForAgent(int agentId, int tick, int* task, int* currentErrand, int* totalErrands) const
{
    if (!task || !currentErrand || !totalErrands) {
        return false;
    }

    int activeTask = -1;
    int assignedTick = 0;
    if (!scheduledTaskAt(agentId, tick, &activeTask, &assignedTick) ||
        activeTask < 0 || activeTask >= m_tasks.size()) {
        return false;
    }
    Q_UNUSED(assignedTick);

    const int total = m_tasks[activeTask].errands.size();
    if (total <= 0) {
        return false;
    }

    const int nextErrand = std::clamp(taskProgressForAgent(agentId, activeTask, tick), 0, total - 1);
    *task = activeTask;
    *currentErrand = nextErrand + 1;
    *totalErrands = total;
    return true;
}

bool PlanData::priorityForAgent(int agentId, int tick, double* priority) const
{
    const int timestep = tick;
    if (!priority || agentId < 0 || timestep < 0 || timestep >= m_plannerPriorityTimeline.size()) {
        return false;
    }
    const QVector<double>& priorities = m_plannerPriorityTimeline[timestep];
    if (agentId >= priorities.size()) {
        return false;
    }
    *priority = priorities[agentId];
    return true;
}

bool PlanData::isAgentDelayed(int agentId, int tick) const
{
    if (agentId < 0 || agentId >= m_delayIntervals.size()) {
        return false;
    }
    const int timestep = std::max(0, tick);
    for (const auto& interval : m_delayIntervals[agentId]) {
        if (timestep < interval.first) {
            return false;
        }
        if (timestep >= interval.first && timestep < interval.second) {
            return true;
        }
    }
    return false;
}

bool PlanData::debugDoorVoteForAgent(int agentId, int tick, int* vote, QPointF* voteCell, int* voteGroup) const
{
    if (!vote || !voteCell || agentId < 0 || m_debugDirectionMarkers.isEmpty()) {
        return false;
    }
    const int timestep = tick;
    if (timestep < 0) {
        return false;
    }
    for (const DebugDirectionMarker& marker : m_debugDirectionMarkers) {
        if (timestep >= marker.agentVotes.size()) {
            continue;
        }
        const QVector<int>& votes = marker.agentVotes[timestep];
        if (agentId >= votes.size() || votes[agentId] == 9) {
            continue;
        }
        if (timestep >= marker.agentVoteLocs.size()) {
            continue;
        }
        const QVector<int>& voteLocs = marker.agentVoteLocs[timestep];
        if (agentId >= voteLocs.size() || voteLocs[agentId] < 0 || m_width <= 0) {
            continue;
        }
        const int loc = voteLocs[agentId];
        *vote = votes[agentId];
        *voteCell = QPointF(loc % m_width, loc / m_width);
        if (voteGroup != nullptr) {
            *voteGroup = 0;
            if (timestep < marker.agentVoteGroups.size()) {
                const QVector<int>& voteGroups = marker.agentVoteGroups[timestep];
                if (agentId < voteGroups.size()) {
                    *voteGroup = voteGroups[agentId];
                }
            }
        }
        return true;
    }
    return false;
}

bool PlanData::guidePathForAgent(int agentId, int tick, QVector<QPointF>* path) const
{
    if (path == nullptr || agentId < 0 || agentId >= m_guidePathTimeline.size()) {
        return false;
    }
    const QVector<GuidePathSnapshot>& timeline = m_guidePathTimeline[agentId];
    if (timeline.isEmpty()) {
        return false;
    }
    const int timestep = std::max(0, tick);
    const GuidePathSnapshot* best = nullptr;
    for (const GuidePathSnapshot& snapshot : timeline) {
        if (snapshot.tick > timestep) {
            break;
        }
        best = &snapshot;
    }
    if (best == nullptr || best->cells.isEmpty()) {
        return false;
    }
    if (agentId < m_taskProgressTimeline.size()) {
        for (const TaskProgressEvent& event : m_taskProgressTimeline[agentId]) {
            if (event.tick > timestep) {
                break;
            }
            if (event.tick > best->tick) {
                return false;
            }
        }
    }
    *path = best->cells;
    return true;
}

bool PlanData::assignmentSnapshotAt(int tick, AssignmentSnapshot* snapshot) const
{
    if (snapshot == nullptr || m_assignmentSnapshots.isEmpty()) {
        return false;
    }

    const AssignmentSnapshot* best = nullptr;
    for (const AssignmentSnapshot& item : m_assignmentSnapshots) {
        if (item.tick > tick) {
            break;
        }
        best = &item;
    }
    if (best == nullptr) {
        return false;
    }

    *snapshot = *best;
    return true;
}

bool PlanData::scheduledTaskAt(int agentId, int tick, int* task, int* assignedTick) const
{
    if (task == nullptr || assignedTick == nullptr || agentId < 0 || agentId >= m_tracks.size()) {
        return false;
    }
    const int timestep = relativeTick(tick);

    if (agentId < m_agentTaskTimeline.size() && !m_agentTaskTimeline[agentId].isEmpty()) {
        *task = -1;
        *assignedTick = 0;
        for (const auto& entry : m_agentTaskTimeline[agentId]) {
            if (entry.first > timestep) {
                break;
            }
            *assignedTick = entry.first;
            *task = entry.second;
        }
        return true;
    }

    if (agentId >= m_agentAssignments.size()) {
        return false;
    }

    for (const AgentTaskAssignment& assignment : m_agentAssignments[agentId]) {
        if (assignment.assignedTick > timestep) {
            break;
        }
        if (assignment.finishedTick < 0 || timestep < assignment.finishedTick) {
            *task = assignment.task;
            *assignedTick = assignment.assignedTick;
            return true;
        }
    }
    *task = -1;
    *assignedTick = 0;
    return true;
}

bool actionSnapshotAt(const QVector<ActionQueueSnapshot>& timeline, int tick, ActionQueueSnapshot* snapshot)
{
    if (snapshot == nullptr || timeline.isEmpty()) {
        return false;
    }
    const int displayTick = tick;
    auto it = std::upper_bound(timeline.begin(), timeline.end(), displayTick,
                               [](int value, const ActionQueueSnapshot& item) {
                                   return value < item.tick;
                               });
    if (it == timeline.begin()) {
        return false;
    }
    --it;
    *snapshot = *it;
    return true;
}

bool PlanData::plannerActionsForAgent(int agentId, int tick, ActionQueueSnapshot* snapshot) const
{
    if (agentId < 0 || agentId >= m_plannerActionTimeline.size()) {
        return false;
    }
    return actionSnapshotAt(m_plannerActionTimeline[agentId], tick, snapshot);
}

bool PlanData::stagedActionsBeforeForAgent(int agentId, int tick, ActionQueueSnapshot* snapshot) const
{
    if (agentId < 0 || agentId >= m_stagedActionBeforeTimeline.size()) {
        return false;
    }
    return actionSnapshotAt(m_stagedActionBeforeTimeline[agentId], tick, snapshot);
}

bool PlanData::stagedActionsForAgent(int agentId, int tick, ActionQueueSnapshot* snapshot) const
{
    if (agentId < 0 || agentId >= m_stagedActionTimeline.size()) {
        return false;
    }
    return actionSnapshotAt(m_stagedActionTimeline[agentId], tick, snapshot);
}

bool PlanData::stagedLocsForAgent(int agentId, int tick, StagedLocSnapshot* snapshot) const
{
    if (snapshot == nullptr || agentId < 0 || agentId >= m_stagedLocTimeline.size()) {
        return false;
    }
    const QVector<StagedLocSnapshot>& timeline = m_stagedLocTimeline[agentId];
    if (timeline.isEmpty()) {
        return false;
    }
    auto it = std::upper_bound(timeline.begin(), timeline.end(), tick,
                               [](int value, const StagedLocSnapshot& item) {
                                   return value < item.tick;
                               });
    if (it == timeline.begin()) {
        return false;
    }
    --it;
    *snapshot = *it;
    return true;
}

bool PlanData::tpgQueuesForAgent(int agentId, int tick, TpgSnapshot* snapshot) const
{
    if (snapshot == nullptr || agentId < 0 || m_tpgSnapshots.isEmpty()) {
        return false;
    }

    const TpgSnapshot* best = nullptr;
    auto it = std::upper_bound(m_tpgSnapshots.begin(), m_tpgSnapshots.end(), tick,
                               [](int value, const TpgSnapshot& item) {
                                   return value < item.tick;
                               });
    if (it == m_tpgSnapshots.begin()) {
        return false;
    }
    --it;
    best = &(*it);

    snapshot->tick = best->tick;
    snapshot->queues.clear();
    for (const TpgQueue& queue : best->queues) {
        if (queue.agents.contains(agentId)) {
            snapshot->queues.push_back(queue);
        }
    }
    return true;
}

QVector<PibtTraceEvent> PlanData::pibtTraceForTick(int tick) const
{
    const int rel = relativeTick(tick);
    if (rel < 0 || rel >= m_pibtTraceTimeline.size()) {
        return {};
    }
    return m_pibtTraceTimeline[rel];
}

int PlanData::finishedTasksAtTick(int tick) const
{
    const int relTick = relativeTick(tick);
    return static_cast<int>(std::upper_bound(m_finishedTaskTicks.begin(), m_finishedTaskTicks.end(), relTick) -
                            m_finishedTaskTicks.begin());
}

int PlanData::relativeTick(int tick) const
{
    return std::max(0, tick - m_displayTickOffset);
}

void PlanData::loadTasksAndAssignments(const QJsonObject& root)
{
    const QJsonArray tasks = root.value("tasks").toArray();
    int maxTaskId = -1;
    for (const QJsonValue& value : tasks) {
        const QJsonArray taskArray = value.toArray();
        if (taskArray.size() < 3) {
            continue;
        }
        maxTaskId = std::max(maxTaskId, taskArray[0].toInt());
    }
    m_tasks.assign(maxTaskId + 1, TaskInfo{});
    for (const QJsonValue& value : tasks) {
        const QJsonArray taskArray = value.toArray();
        if (taskArray.size() < 3) {
            continue;
        }
        const int taskId = taskArray[0].toInt();
        if (taskId < 0 || taskId >= m_tasks.size()) {
            continue;
        }
        TaskInfo task;
        task.id = taskId;
        const QJsonArray locs = taskArray[2].toArray();
        for (int i = 0; i + 1 < locs.size(); i += 2) {
            const double row = locs[i].toDouble();
            const double col = locs[i + 1].toDouble();
            task.errands.push_back(QPointF(col, row));
        }
        m_tasks[taskId] = std::move(task);
    }

    m_agentAssignments.assign(m_tracks.size(), {});
    m_taskProgressTimeline.assign(m_tracks.size(), {});
    m_finishedTaskTicks.clear();
    const QJsonArray events = root.value("events").toArray();
    auto toRelativeEventTick = [this](int tick) {
        if (m_displayTickOffset <= 0) {
            return tick;
        }
        return std::max(0, tick - m_displayTickOffset);
    };
    QVector<AgentTaskAssignment> assignments;
    for (const QJsonValue& value : events) {
        const QJsonArray event = value.toArray();
        if (event.size() < 4) {
            continue;
        }
        const int eventTick = toRelativeEventTick(event[0].toInt());
        if (eventTick > m_maxTick) {
            continue;
        }
        const int eventAgent = event[1].toInt();
        const int eventTask = event[2].toInt();
        const int nextErrand = event[3].toInt();
        if (eventAgent >= 0 && eventAgent < m_taskProgressTimeline.size()) {
            m_taskProgressTimeline[eventAgent].push_back(TaskProgressEvent{eventTick, eventTask, nextErrand});
        }
        if (nextErrand != 1) {
            continue;
        }
        AgentTaskAssignment assignment;
        assignment.assignedTick = eventTick;
        assignment.agent = eventAgent;
        assignment.task = eventTask;
        if (assignment.agent >= 0 && assignment.agent < m_tracks.size()) {
            assignments.push_back(assignment);
        }
    }

    std::sort(assignments.begin(), assignments.end(), [](const AgentTaskAssignment& a, const AgentTaskAssignment& b) {
        if (a.agent != b.agent) return a.agent < b.agent;
        if (a.assignedTick != b.assignedTick) return a.assignedTick < b.assignedTick;
        return a.task < b.task;
    });

    for (AgentTaskAssignment& assignment : assignments) {
        int finishTick = -1;
        const int taskErrandCount =
            (assignment.task >= 0 && assignment.task < m_tasks.size())
                ? static_cast<int>(m_tasks[assignment.task].errands.size())
                : -1;
        for (const QJsonValue& value : events) {
            const QJsonArray event = value.toArray();
            if (event.size() < 4 || taskErrandCount <= 0 || event[3].toInt() < taskErrandCount) {
                continue;
            }
            if (event[1].toInt() == assignment.agent &&
                event[2].toInt() == assignment.task &&
                toRelativeEventTick(event[0].toInt()) >= assignment.assignedTick) {
                const int candidate = toRelativeEventTick(event[0].toInt());
                if (finishTick < 0 || candidate < finishTick) {
                    finishTick = candidate;
                }
            }
        }
        assignment.finishedTick = finishTick;
        m_agentAssignments[assignment.agent].push_back(assignment);
        if (finishTick >= 0) {
            m_finishedTaskTicks.push_back(finishTick);
        }
    }

    std::sort(m_finishedTaskTicks.begin(), m_finishedTaskTicks.end());

    for (auto& timeline : m_taskProgressTimeline) {
        std::sort(timeline.begin(), timeline.end(), [](const TaskProgressEvent& a, const TaskProgressEvent& b) {
            if (a.tick != b.tick) return a.tick < b.tick;
            if (a.task != b.task) return a.task < b.task;
            return a.nextErrand < b.nextErrand;
        });
    }
}

void PlanData::loadScheduleTimeline(const QJsonObject& root)
{
    m_agentTaskTimeline.assign(m_tracks.size(), {});

    QJsonArray schedules = root.value("actualSchedule").toArray();
    if (schedules.isEmpty()) {
        schedules = root.value("plannerSchedule").toArray();
    }
    if (schedules.isEmpty()) {
        return;
    }

    const int count = std::min(static_cast<int>(schedules.size()), static_cast<int>(m_tracks.size()));
    for (int agent = 0; agent < count; ++agent) {
        const QString schedule = schedules[agent].toString().trimmed();
        if (schedule.isEmpty()) {
            continue;
        }

        QVector<QPair<int, int>> timeline;
        const auto entries = schedule.split(',', Qt::SkipEmptyParts);
        timeline.reserve(entries.size());
        int entryIndex = 0;
        for (const QString& rawEntry : entries) {
            const auto parts = rawEntry.trimmed().split(':', Qt::SkipEmptyParts);
            if (parts.size() != 2) {
                entryIndex++;
                continue;
            }
            bool okTick = false;
            bool okTask = false;
            const int tick = parts[0].toInt(&okTick);
            const int task = parts[1].toInt(&okTask);
            if (okTick && okTask) {
                int relTick = tick;
                bool forcedInitialTick = false;
                if (entryIndex == 0 && relTick > 0 && task >= 0) {
                    relTick = 0;
                    forcedInitialTick = true;
                }
                if (m_displayTickOffset > 0 && !forcedInitialTick) {
                    relTick = std::max(0, tick - m_displayTickOffset);
                }
                if (relTick < 0 || relTick > m_maxTick) {
                    entryIndex++;
                    continue;
                }
                timeline.push_back(qMakePair(relTick, task));
            }
            entryIndex++;
        }

        std::stable_sort(timeline.begin(), timeline.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        if (!timeline.isEmpty() && timeline.front().first > 0 && timeline.front().second >= 0) {
            timeline.front().first = 0;
        }
        m_agentTaskTimeline[agent] = std::move(timeline);
    }
}

void PlanData::loadSandboxGoalTimeline(const QJsonObject& root)
{
    m_sandboxGoalTimeline.assign(m_tracks.size(), {});

    const QJsonObject sandbox = root.value("sandbox").toObject();
    const QJsonArray timeline = sandbox.value("goalTimeline").toArray();
    if (timeline.isEmpty()) {
        return;
    }

    const int count = std::min(static_cast<int>(timeline.size()), static_cast<int>(m_tracks.size()));
    for (int agent = 0; agent < count; ++agent) {
        const QJsonArray agentTimeline = timeline[agent].toArray();
        m_sandboxGoalTimeline[agent].reserve(agentTimeline.size());
        for (const QJsonValue& value : agentTimeline) {
            const QJsonObject item = value.toObject();
            const int row = item.value("row").toInt(-1);
            const int col = item.value("col").toInt(-1);
            if (row < 0 || col < 0) {
                m_sandboxGoalTimeline[agent].push_back(QPointF(-1.0, -1.0));
            } else {
                m_sandboxGoalTimeline[agent].push_back(QPointF(col, row));
            }
        }
    }
}

void PlanData::loadPlannerPriorityTimeline(const QJsonObject& root, int agentCount)
{
    m_plannerPriorityTimeline.clear();
    const QJsonArray timeline = root.value("plannerPriority").toArray();
    if (timeline.isEmpty() || agentCount <= 0) {
        return;
    }

    m_plannerPriorityTimeline.reserve(timeline.size());
    for (const QJsonValue& tickValue : timeline) {
        const QJsonArray row = tickValue.toArray();
        QVector<double> priorities;
        priorities.reserve(std::min(agentCount, static_cast<int>(row.size())));
        for (int agent = 0; agent < agentCount && agent < row.size(); ++agent) {
            priorities.push_back(row[agent].toDouble());
        }
        m_plannerPriorityTimeline.push_back(std::move(priorities));
    }
}

void PlanData::loadGuidePathTimeline(const QJsonObject& root, int agentCount)
{
    m_guidePathTimeline.assign(agentCount, {});
    const QJsonArray agents = root.value("guidePaths").toArray();
    if (agents.isEmpty() || m_width <= 0) {
        return;
    }

    const int count = std::min(agentCount, static_cast<int>(agents.size()));
    for (int agent = 0; agent < count; agent++) {
        const QJsonArray timelineValues = agents[agent].toArray();
        QVector<GuidePathSnapshot> timeline;
        timeline.reserve(timelineValues.size());
        for (const QJsonValue& value : timelineValues) {
            const QJsonObject item = value.toObject();
            GuidePathSnapshot snapshot;
            snapshot.tick = item.value("tick").toInt(-1);
            if (snapshot.tick < 0) {
                continue;
            }
            const QJsonArray pathValues = item.value("path").toArray();
            snapshot.cells.reserve(pathValues.size());
            for (const QJsonValue& locValue : pathValues) {
                const int loc = locValue.toInt(-1);
                if (loc < 0) {
                    continue;
                }
                snapshot.cells.push_back(QPointF(loc % m_width, loc / m_width));
            }
            if (!snapshot.cells.isEmpty()) {
                timeline.push_back(std::move(snapshot));
            }
        }
        std::sort(timeline.begin(), timeline.end(), [](const GuidePathSnapshot& a, const GuidePathSnapshot& b) {
            return a.tick < b.tick;
        });
        m_guidePathTimeline[agent] = std::move(timeline);
    }
}

void PlanData::loadAssignmentSnapshots(const QJsonObject& root)
{
    m_assignmentSnapshots.clear();
    const QJsonArray snapshots = root.value("assignmentSnapshots").toArray();
    if (snapshots.isEmpty()) {
        return;
    }

    m_assignmentSnapshots.reserve(snapshots.size());
    for (const QJsonValue& value : snapshots) {
        const QJsonObject item = value.toObject();
        AssignmentSnapshot snapshot;
        snapshot.tick = item.value("tick").toInt(-1);
        if (snapshot.tick < 0) {
            continue;
        }

        const QJsonArray freeAgentValues = item.value("freeAgents").toArray();
        snapshot.freeAgents.reserve(freeAgentValues.size());
        for (const QJsonValue& agentValue : freeAgentValues) {
            snapshot.freeAgents.push_back(agentValue.toInt(-1));
        }

        const QJsonArray freeTaskValues = item.value("freeTasks").toArray();
        snapshot.freeTasks.reserve(freeTaskValues.size());
        for (const QJsonValue& taskValue : freeTaskValues) {
            snapshot.freeTasks.push_back(taskValue.toInt(-1));
        }

        m_assignmentSnapshots.push_back(std::move(snapshot));
    }

    std::sort(m_assignmentSnapshots.begin(), m_assignmentSnapshots.end(),
              [](const AssignmentSnapshot& a, const AssignmentSnapshot& b) {
                  return a.tick < b.tick;
              });
}

void PlanData::loadActionSnapshotTimeline(const QJsonObject& root,
                                          const QString& fieldName,
                                          int agentCount,
                                          QVector<QVector<ActionQueueSnapshot>>* destination)
{
    if (destination == nullptr) {
        return;
    }
    destination->assign(agentCount, {});
    const QJsonArray snapshots = root.value(fieldName).toArray();
    if (snapshots.isEmpty() || agentCount <= 0) {
        return;
    }

    for (const QJsonValue& snapshotValue : snapshots) {
        const QJsonObject item = snapshotValue.toObject();
        const int tick = item.value("tick").toInt(-1);
        const int sourceTick = item.value("sourceTick").toInt(tick);
        if (tick < 0) {
            continue;
        }
        const QJsonArray agents = item.value("agents").toArray();
        const QJsonArray baseStates = item.value("baseStates").toArray();
        const int count = std::min(agentCount, static_cast<int>(agents.size()));
        for (int agent = 0; agent < count; agent++) {
            const QJsonArray actions = agents[agent].toArray();
            ActionQueueSnapshot snapshot;
            snapshot.tick = tick;
            snapshot.sourceTick = sourceTick;
            if (agent < baseStates.size()) {
                const QJsonArray baseState = baseStates[agent].toArray();
                if (baseState.size() >= 2) {
                    snapshot.baseCell = QPointF(baseState[1].toDouble(), baseState[0].toDouble());
                    snapshot.hasBaseCell = true;
                }
            }
            snapshot.actions.reserve(actions.size());
            for (const QJsonValue& actionValue : actions) {
                const QString action = actionValue.toString();
                if (!action.isEmpty()) {
                    snapshot.actions.push_back(action);
                }
            }
            (*destination)[agent].push_back(std::move(snapshot));
        }
    }

    for (auto& timeline : *destination) {
        std::sort(timeline.begin(), timeline.end(),
                  [](const ActionQueueSnapshot& a, const ActionQueueSnapshot& b) {
                      return a.tick < b.tick;
                  });
    }
}

void PlanData::loadStagedLocSnapshots(const QJsonObject& root, int agentCount)
{
    m_stagedLocTimeline.assign(agentCount, {});
    const QJsonArray snapshots = root.value("stagedLocSnapshots").toArray();
    if (snapshots.isEmpty() || agentCount <= 0) {
        return;
    }

    for (const QJsonValue& snapshotValue : snapshots) {
        const QJsonObject item = snapshotValue.toObject();
        const int tick = item.value("tick").toInt(-1);
        if (tick < 0) {
            continue;
        }
        const QJsonArray agents = item.value("agents").toArray();
        const int count = std::min(agentCount, static_cast<int>(agents.size()));
        for (int agent = 0; agent < count; agent++) {
            const QJsonArray locs = agents[agent].toArray();
            StagedLocSnapshot snapshot;
            snapshot.tick = tick;
            snapshot.cells.reserve(locs.size());
            for (const QJsonValue& locValue : locs) {
                const QJsonArray cell = locValue.toArray();
                if (cell.size() >= 2) {
                    snapshot.cells.push_back(QPointF(cell[1].toDouble(), cell[0].toDouble()));
                }
            }
            if (!snapshot.cells.isEmpty()) {
                m_stagedLocTimeline[agent].push_back(std::move(snapshot));
            }
        }
    }

    for (auto& timeline : m_stagedLocTimeline) {
        std::sort(timeline.begin(), timeline.end(),
                  [](const StagedLocSnapshot& a, const StagedLocSnapshot& b) {
                      return a.tick < b.tick;
                  });
    }
}

void PlanData::loadTpgSnapshots(const QJsonObject& root)
{
    m_tpgSnapshots.clear();
    const QJsonArray snapshots = root.value("tpgSnapshots").toArray();
    if (snapshots.isEmpty()) {
        return;
    }

    m_tpgSnapshots.reserve(snapshots.size());
    for (const QJsonValue& snapshotValue : snapshots) {
        const QJsonObject item = snapshotValue.toObject();
        TpgSnapshot snapshot;
        snapshot.tick = item.value("tick").toInt(-1);
        if (snapshot.tick < 0) {
            continue;
        }
        const QJsonArray queues = item.value("queues").toArray();
        snapshot.queues.reserve(queues.size());
        for (const QJsonValue& queueValue : queues) {
            const QJsonArray queueArray = queueValue.toArray();
            if (queueArray.size() < 3) {
                continue;
            }
            TpgQueue queue;
            queue.cell = QPointF(queueArray[1].toDouble(), queueArray[0].toDouble());
            const QJsonArray agents = queueArray[2].toArray();
            queue.agents.reserve(agents.size());
            for (const QJsonValue& agentValue : agents) {
                const int agent = agentValue.toInt(-1);
                if (agent >= 0) {
                    queue.agents.push_back(agent);
                }
            }
            if (!queue.agents.isEmpty()) {
                snapshot.queues.push_back(std::move(queue));
            }
        }
        m_tpgSnapshots.push_back(std::move(snapshot));
    }

    std::sort(m_tpgSnapshots.begin(), m_tpgSnapshots.end(),
              [](const TpgSnapshot& a, const TpgSnapshot& b) {
                  return a.tick < b.tick;
              });
}

void PlanData::loadPibtTraceTimeline(const QJsonObject& root)
{
    m_pibtTraceTimeline.clear();
    const QJsonArray timeline = root.value("pibtTrace").toArray();
    if (timeline.isEmpty()) {
        return;
    }

    m_pibtTraceTimeline.reserve(timeline.size());
    for (const QJsonValue& tickValue : timeline) {
        const QJsonArray eventValues = tickValue.toArray();
        QVector<PibtTraceEvent> events;
        events.reserve(eventValues.size());
        for (const QJsonValue& value : eventValues) {
            const QJsonObject item = value.toObject();
            PibtTraceEvent event;
            event.type = item.value("type").toString();
            event.fromAgent = item.value("from").toInt(-1);
            event.toAgent = item.value("to").toInt(-1);
            const int col = item.value("col").toInt(-1);
            const int row = item.value("row").toInt(-1);
            if (row >= 0 && col >= 0) {
                event.cell = QPointF(col, row);
            } else {
                const int loc = item.value("loc").toInt(-1);
                event.cell = loc >= 0 && m_width > 0
                    ? QPointF(loc % m_width, loc / m_width)
                    : QPointF(-1.0, -1.0);
            }
            if (!event.type.isEmpty() && event.fromAgent >= 0 && event.toAgent >= 0) {
                events.push_back(std::move(event));
            }
        }
        m_pibtTraceTimeline.push_back(std::move(events));
    }
}

void PlanData::loadDelayIntervals(const QJsonObject& root, int agentCount)
{
    m_delayIntervals.assign(agentCount, {});
    const QJsonArray agents = root.value("delayIntervals").toArray();
    if (agents.isEmpty()) {
        return;
    }

    const int count = std::min(agentCount, static_cast<int>(agents.size()));
    for (int agent = 0; agent < count; agent++) {
        const QJsonArray intervals = agents[agent].toArray();
        QVector<QPair<int, int>> parsedIntervals;
        parsedIntervals.reserve(intervals.size());
        for (const QJsonValue& value : intervals) {
            const QJsonArray interval = value.toArray();
            if (interval.size() < 2) {
                continue;
            }
            const int begin = interval[0].toInt(-1);
            const int end = interval[1].toInt(-1);
            if (begin >= 0 && end > begin) {
                parsedIntervals.push_back(qMakePair(begin, end));
            }
        }
        std::sort(parsedIntervals.begin(), parsedIntervals.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        m_delayIntervals[agent] = std::move(parsedIntervals);
    }
}

void PlanData::buildOccupancyHeat()
{
    m_occupancyHeat.assign(m_width * m_height, 0);
    m_maxOccupancyHeat = 0;
    m_occupancyHeatLogLow = 0.0;
    m_occupancyHeatLogHigh = 0.0;
    if (m_width <= 0 || m_height <= 0 || m_map.size() < m_width * m_height) {
        return;
    }

    for (const AgentTrack& track : m_tracks) {
        for (const AgentFrame& frame : track.frames) {
            const int col = static_cast<int>(std::floor(frame.cell.x() + 0.5));
            const int row = static_cast<int>(std::floor(frame.cell.y() + 0.5));
            if (row < 0 || row >= m_height || col < 0 || col >= m_width) {
                continue;
            }
            const int index = row * m_width + col;
            if (m_map[index] != 0) {
                continue;
            }
            const int next = ++m_occupancyHeat[index];
            if (next > m_maxOccupancyHeat) {
                m_maxOccupancyHeat = next;
            }
        }
    }

    QVector<double> logs;
    logs.reserve(m_occupancyHeat.size());
    for (int count : m_occupancyHeat) {
        if (count > 0) {
            logs.push_back(std::log1p(static_cast<double>(count)));
        }
    }
    if (logs.isEmpty()) {
        return;
    }
    std::sort(logs.begin(), logs.end());
    const auto quantile = [&logs](double q) {
        const int last = static_cast<int>(logs.size()) - 1;
        const int index = std::clamp(
            static_cast<int>(std::round(q * static_cast<double>(last))),
            0,
            last);
        return logs[index];
    };
    m_occupancyHeatLogLow = quantile(0.10);
    m_occupancyHeatLogHigh = quantile(0.97);
    if (m_occupancyHeatLogHigh <= m_occupancyHeatLogLow) {
        m_occupancyHeatLogLow = logs.front();
        m_occupancyHeatLogHigh = logs.back();
    }
}

void PlanData::buildWaitHeat()
{
    m_waitHeat.assign(m_width * m_height, 0);
    m_maxWaitHeat = 0;
    m_waitHeatLogLow = 0.0;
    m_waitHeatLogHigh = 0.0;
    if (m_width <= 0 || m_height <= 0 || m_map.size() < m_width * m_height) {
        return;
    }

    for (const AgentTrack& track : m_tracks) {
        const int count = std::min(static_cast<int>(track.actions.size()), static_cast<int>(track.frames.size()));
        for (int tick = 0; tick < count; ++tick) {
            if (track.actions[tick] != 'W') {
                continue;
            }
            const AgentFrame& frame = track.frames[tick];
            const int col = static_cast<int>(std::floor(frame.cell.x() + 0.5));
            const int row = static_cast<int>(std::floor(frame.cell.y() + 0.5));
            if (row < 0 || row >= m_height || col < 0 || col >= m_width) {
                continue;
            }
            const int index = row * m_width + col;
            if (m_map[index] != 0) {
                continue;
            }
            const int next = ++m_waitHeat[index];
            if (next > m_maxWaitHeat) {
                m_maxWaitHeat = next;
            }
        }
    }

    QVector<double> logs;
    logs.reserve(m_waitHeat.size());
    for (int count : m_waitHeat) {
        if (count > 0) {
            logs.push_back(std::log1p(static_cast<double>(count)));
        }
    }
    if (logs.isEmpty()) {
        return;
    }
    std::sort(logs.begin(), logs.end());
    const auto quantile = [&logs](double q) {
        const int last = static_cast<int>(logs.size()) - 1;
        const int index = std::clamp(
            static_cast<int>(std::round(q * static_cast<double>(last))),
            0,
            last);
        return logs[index];
    };
    m_waitHeatLogLow = quantile(0.05);
    m_waitHeatLogHigh = quantile(0.98);
    if (m_waitHeatLogHigh <= m_waitHeatLogLow) {
        m_waitHeatLogLow = logs.front();
        m_waitHeatLogHigh = logs.back();
    }
}

void PlanData::loadDebugOverlays(const QJsonObject& root)
{
    m_debugOverlays.clear();
    const QJsonArray overlays = root.value("debugOverlays").toArray();
    for (const QJsonValue& value : overlays) {
        const QJsonObject item = value.toObject();
        if (item.value("kind").toString() != "rect") {
            continue;
        }
        const int row0 = item.value("row0").toInt(-1);
        const int row1 = item.value("row1").toInt(-1);
        const int col0 = item.value("col0").toInt(-1);
        const int col1 = item.value("col1").toInt(-1);
        if (row0 < 0 || col0 < 0 || row1 < row0 || col1 < col0) {
            continue;
        }

        DebugOverlay overlay;
        overlay.name = item.value("name").toString();
        overlay.rect = QRectF(col0, row0, col1 - col0 + 1, row1 - row0 + 1);
        overlay.color = QColor(item.value("color").toString("#2F80ED"));
        if (!overlay.color.isValid()) {
            overlay.color = QColor("#2F80ED");
        }
        m_debugOverlays.push_back(overlay);
    }

    if (isRoomMap() && m_width == 64 && m_height == 64) {
        DebugOverlay topLeftWait;
        topLeftWait.name = "6-10 up wait top";
        topLeftWait.rect = QRectF(42, 24, 3, 8);
        topLeftWait.color = QColor("#2F80ED");
        m_debugOverlays.push_back(topLeftWait);

        DebugOverlay topRightWait;
        topRightWait.name = "6-10 down wait top";
        topRightWait.rect = QRectF(45, 24, 3, 8);
        topRightWait.color = QColor("#E27D2A");
        m_debugOverlays.push_back(topRightWait);

        DebugOverlay bottomLeftWait;
        bottomLeftWait.name = "6-10 up wait bottom";
        bottomLeftWait.rect = QRectF(42, 33, 3, 8);
        bottomLeftWait.color = QColor("#2F80ED");
        m_debugOverlays.push_back(bottomLeftWait);

        DebugOverlay bottomRightWait;
        bottomRightWait.name = "6-10 down wait bottom";
        bottomRightWait.rect = QRectF(45, 33, 3, 8);
        bottomRightWait.color = QColor("#E27D2A");
        m_debugOverlays.push_back(bottomRightWait);
    }
}

void PlanData::loadDebugDirections(const QJsonObject& root)
{
    m_debugDirectionMarkers.clear();
    const QJsonArray markers = root.value("debugDirections").toArray();
    for (const QJsonValue& value : markers) {
        const QJsonObject item = value.toObject();
        const int row = item.value("row").toInt(-1);
        const int col = item.value("col").toInt(-1);
        const QJsonArray directionValues = item.value("directions").toArray();
        const QJsonArray downVoteValues = item.value("downVotes").toArray();
        const QJsonArray upVoteValues = item.value("upVotes").toArray();
        const QJsonArray pendingDirectionValues = item.value("pendingDirections").toArray();
        const QJsonArray yellowActiveValues = item.value("yellowActive").toArray();
        const QJsonArray zeroVoteBlockedValues = item.value("zeroVoteBlocked").toArray();
        const QJsonArray agentVoteValues = item.value("agentVotes").toArray();
        const QJsonArray agentVoteLocValues = item.value("agentVoteLocs").toArray();
        const QJsonArray agentVoteGroupValues = item.value("agentVoteGroups").toArray();
        if (row < 0 || col < 0 || directionValues.isEmpty()) {
            continue;
        }

        DebugDirectionMarker marker;
        marker.name = item.value("name").toString();
        marker.cell = QPointF(col, row);
        marker.axis = item.value("axis").toString("vertical");
        marker.positiveLabel = item.value("positiveLabel").toString("down");
        marker.negativeLabel = item.value("negativeLabel").toString("up");
        marker.directions.reserve(directionValues.size());
        marker.downVotes.reserve(directionValues.size());
        marker.upVotes.reserve(directionValues.size());
        marker.pendingDirections.reserve(directionValues.size());
        marker.yellowActive.reserve(directionValues.size());
        marker.zeroVoteBlocked.reserve(directionValues.size());
        marker.agentVotes.reserve(directionValues.size());
        marker.agentVoteLocs.reserve(directionValues.size());
        marker.agentVoteGroups.reserve(directionValues.size());
        for (const QJsonValue& directionValue : directionValues) {
            marker.directions.push_back(directionValue.toInt(0));
        }
        for (int i = 0; i < directionValues.size(); i++) {
            marker.downVotes.push_back(i < downVoteValues.size() ? downVoteValues[i].toInt(0) : 0);
            marker.upVotes.push_back(i < upVoteValues.size() ? upVoteValues[i].toInt(0) : 0);
            marker.pendingDirections.push_back(i < pendingDirectionValues.size() ? pendingDirectionValues[i].toInt(0) : 0);
            marker.yellowActive.push_back(i < yellowActiveValues.size() ? yellowActiveValues[i].toInt(0) : 0);
            marker.zeroVoteBlocked.push_back(i < zeroVoteBlockedValues.size() ? zeroVoteBlockedValues[i].toInt(0) : 0);
            QVector<int> votes;
            const QJsonArray voteValues = i < agentVoteValues.size() ? agentVoteValues[i].toArray() : QJsonArray();
            votes.reserve(voteValues.size());
            for (const QJsonValue& voteValue : voteValues) {
                votes.push_back(voteValue.toInt(9));
            }
            marker.agentVotes.push_back(std::move(votes));

            QVector<int> voteLocs;
            const QJsonArray voteLocValues =
                i < agentVoteLocValues.size() ? agentVoteLocValues[i].toArray() : QJsonArray();
            voteLocs.reserve(voteLocValues.size());
            for (const QJsonValue& voteLocValue : voteLocValues) {
                voteLocs.push_back(voteLocValue.toInt(-1));
            }
            marker.agentVoteLocs.push_back(std::move(voteLocs));

            QVector<int> voteGroups;
            const QJsonArray voteGroupValues =
                i < agentVoteGroupValues.size() ? agentVoteGroupValues[i].toArray() : QJsonArray();
            voteGroups.reserve(voteGroupValues.size());
            for (const QJsonValue& voteGroupValue : voteGroupValues) {
                voteGroups.push_back(voteGroupValue.toInt(0));
            }
            marker.agentVoteGroups.push_back(std::move(voteGroups));
        }
        m_debugDirectionMarkers.push_back(std::move(marker));
    }
}

void PlanData::loadHighwayDirections(const QJsonObject& root)
{
    m_highwayDirections.clear();
    const QJsonArray directions = root.value("highwayDirections").toArray();
    m_highwayDirections.reserve(directions.size());
    for (const QJsonValue& value : directions) {
        const QJsonObject item = value.toObject();
        const int row = item.value("row").toInt(-1);
        const int col = item.value("col").toInt(-1);
        const int dir = item.value("dir").toInt(-1);
        const int blockedDir = item.value("blockedDir").toInt(-1);
        const int weight = item.value("weight").toInt(0);
        if (row < 0 || col < 0 || row >= m_height || col >= m_width ||
            dir < 0 || dir > 3 || weight <= 0) {
            continue;
        }

        HighwayDirection highway;
        highway.cell = QPointF(col, row);
        highway.dir = dir;
        highway.blockedDir = blockedDir;
        highway.weight = weight;
        m_highwayDirections.push_back(highway);
    }

    bool hasRoom01Highway = false;
    for (const HighwayDirection& highway : m_highwayDirections) {
        const int col = static_cast<int>(highway.cell.x());
        const int row = static_cast<int>(highway.cell.y());
        if (row >= 8 && row <= 13 && col >= 13 && col <= 19) {
            hasRoom01Highway = true;
            break;
        }
    }

    auto hasOverlay = [&](const QString& name) {
        for (const DebugOverlay& overlay : m_debugOverlays) {
            if (overlay.name == name) {
                return true;
            }
        }
        return false;
    };
    auto addOverlay = [&](const QString& name, const QRectF& rect, const QColor& color) {
        if (hasOverlay(name)) {
            return;
        }
        DebugOverlay overlay;
        overlay.name = name;
        overlay.rect = rect;
        overlay.color = color;
        m_debugOverlays.push_back(overlay);
    };

    if (isRoomMap() && m_width == 64 && m_height == 64 && hasRoom01Highway) {
        addOverlay("room 0-1 leftward top highway", QRectF(13, 8, 7, 3), QColor("#2F80ED"));
        addOverlay("room 0-1 rightward bottom highway", QRectF(13, 11, 7, 3), QColor("#F2994A"));
    }
}

void PlanData::loadDirectionMap(const QJsonObject& root)
{
    m_directionMapSnapshots.clear();
    const QJsonObject directionMap = root.value("directionMap").toObject();
    if (!directionMap.value("enabled").toBool(false)) {
        return;
    }

    const QJsonArray snapshots = directionMap.value("snapshots").toArray();
    m_directionMapSnapshots.reserve(snapshots.size());
    for (const QJsonValue& snapshotValue : snapshots) {
        const QJsonObject snapshotObject = snapshotValue.toObject();
        DirectionMapSnapshot snapshot;
        snapshot.tick = snapshotObject.value("tick").toInt(-1);
        if (snapshot.tick < 0) {
            continue;
        }

        const QJsonArray cells = snapshotObject.value("cells").toArray();
        snapshot.cells.reserve(cells.size());
        for (const QJsonValue& cellValue : cells) {
            const QJsonObject cellObject = cellValue.toObject();
            const int loc = cellObject.value("loc").toInt(-1);
            if (loc < 0 || m_width <= 0) {
                continue;
            }
            const int row = loc / m_width;
            const int col = loc % m_width;
            if (row < 0 || row >= m_height || col < 0 || col >= m_width) {
                continue;
            }

            DirectionMapCell cell;
            cell.cell = QPointF(col, row);
            cell.x = cellObject.value("x").toDouble(0.0);
            cell.y = cellObject.value("y").toDouble(0.0);
            cell.magnitude = std::sqrt(cell.x * cell.x + cell.y * cell.y);
            if (cell.magnitude <= 0.0) {
                continue;
            }
            snapshot.cells.push_back(cell);
        }
        m_directionMapSnapshots.push_back(std::move(snapshot));
    }

    std::sort(m_directionMapSnapshots.begin(), m_directionMapSnapshots.end(),
              [](const DirectionMapSnapshot& a, const DirectionMapSnapshot& b) {
                  return a.tick < b.tick;
              });
}

const DirectionMapSnapshot* PlanData::directionMapAtTick(int tick) const
{
    if (m_directionMapSnapshots.isEmpty()) {
        return nullptr;
    }

    const int relative = relativeTick(tick);
    const DirectionMapSnapshot* best = nullptr;
    for (const DirectionMapSnapshot& snapshot : m_directionMapSnapshots) {
        if (snapshot.tick > relative) {
            break;
        }
        best = &snapshot;
    }
    return best != nullptr ? best : &m_directionMapSnapshots.front();
}

bool PlanData::isRoomMap() const
{
    QString normalized = m_mapPath.toLower();
    normalized.replace('\\', '/');
    return normalized.contains("/room.domain/") ||
           QFileInfo(m_mapPath).fileName().toLower().startsWith("room-");
}

bool PlanData::trackReachedGoal(int agentId, int beginTick, int endTick, const QPointF& goal) const
{
    if (agentId < 0 || agentId >= m_tracks.size()) {
        return false;
    }
    const AgentTrack& track = m_tracks[agentId];
    if (track.frames.isEmpty()) {
        return false;
    }
    const int begin = std::clamp(beginTick, 0, static_cast<int>(track.frames.size()) - 1);
    const int end = std::clamp(endTick, begin, static_cast<int>(track.frames.size()) - 1);
    for (int tick = begin; tick <= end; ++tick) {
        const QPointF cell = track.frames[tick].cell;
        if (std::abs(cell.x() - goal.x()) < 0.35 && std::abs(cell.y() - goal.y()) < 0.35) {
            return true;
        }
    }
    return false;
}

int PlanData::taskProgressForAgent(int agentId, int taskId, int tick) const
{
    if (agentId < 0 || agentId >= m_taskProgressTimeline.size() || taskId < 0) {
        return 0;
    }

    const int relTick = relativeTick(tick);
    int nextErrand = 0;
    for (const TaskProgressEvent& event : m_taskProgressTimeline[agentId]) {
        if (event.tick > relTick) {
            break;
        }
        if (event.task == taskId) {
            nextErrand = std::max(nextErrand, event.nextErrand);
        }
    }
    return nextErrand;
}
