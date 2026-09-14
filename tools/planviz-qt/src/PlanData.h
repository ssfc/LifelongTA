#pragma once

#include <QColor>
#include <QJsonArray>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

struct AgentFrame {
    QPointF cell;
    double dir = 0;
};

struct AgentTrack {
    QPointF start;
    double startDir = 0;
    QVector<unsigned char> actions;
    QVector<AgentFrame> frames;
};

struct TaskInfo {
    int id = -1;
    QVector<QPointF> errands;
};

struct AgentTaskAssignment {
    int agent = -1;
    int task = -1;
    int assignedTick = 0;
    int finishedTick = -1;
};

struct TaskProgressEvent {
    int tick = 0;
    int task = -1;
    int nextErrand = 0;
};

struct GuidePathSnapshot {
    int tick = 0;
    QVector<QPointF> cells;
};

struct PibtTraceEvent {
    QString type;
    int fromAgent = -1;
    int toAgent = -1;
    QPointF cell;
};

struct DebugOverlay {
    QString name;
    QRectF rect;
    QColor color;
};

struct DebugDirectionMarker {
    QString name;
    QPointF cell;
    QString axis = "vertical";
    QString positiveLabel = "down";
    QString negativeLabel = "up";
    QVector<int> directions;
    QVector<int> downVotes;
    QVector<int> upVotes;
    QVector<int> pendingDirections;
    QVector<int> yellowActive;
    QVector<int> zeroVoteBlocked;
    QVector<QVector<int>> agentVotes;
    QVector<QVector<int>> agentVoteLocs;
    QVector<QVector<int>> agentVoteGroups;
};

struct HighwayDirection {
    QPointF cell;
    int dir = 0;
    int blockedDir = 0;
    int weight = 0;
};

struct DirectionMapCell {
    QPointF cell;
    double x = 0.0;
    double y = 0.0;
    double magnitude = 0.0;
};

struct DirectionMapSnapshot {
    int tick = 0;
    QVector<DirectionMapCell> cells;
};

struct AssignmentSnapshot {
    int tick = 0;
    QVector<int> freeAgents;
    QVector<int> freeTasks;
};

struct ActionQueueSnapshot {
    int tick = 0;
    int sourceTick = 0;
    QStringList actions;
    QPointF baseCell;
    bool hasBaseCell = false;
};

struct StagedLocSnapshot {
    int tick = 0;
    QVector<QPointF> cells;
};

struct TpgQueue {
    QPointF cell;
    QVector<int> agents;
};

struct TpgSnapshot {
    int tick = 0;
    QVector<TpgQueue> queues;
};

class PlanData {
public:
    bool loadMap(const QString& path, QString* error);
    bool loadPlan(const QString& path, int agentLimit, int startTick, int endTick, QString* error);
    bool loadLifelongTrace(const QString& path, int agentLimit, int startTick, int endTick, QString* error);

    int width() const { return m_width; }
    int height() const { return m_height; }
    int teamSize() const { return m_tracks.size(); }
    int startTick() const { return m_startTick; }
    int endTick() const { return m_endTick; }
    int maxTick() const { return m_displayTickOffset + m_maxTick; }
    int ticksPerTimestep() const { return m_ticksPerTimestep; }
    int finishedTasks() const { return m_finishedTasks; }
    int finishedTasksAtTick(int tick) const;
    bool isSandboxPlan() const { return m_isSandboxPlan; }
    bool hasAgentOrientations() const { return m_hasAgentOrientations; }
    const QVector<unsigned char>& mapCells() const { return m_map; }
    const QVector<AgentTrack>& tracks() const { return m_tracks; }
    const QVector<int>& occupancyHeat() const { return m_occupancyHeat; }
    int maxOccupancyHeat() const { return m_maxOccupancyHeat; }
    double occupancyHeatLogLow() const { return m_occupancyHeatLogLow; }
    double occupancyHeatLogHigh() const { return m_occupancyHeatLogHigh; }
    const QVector<int>& waitHeat() const { return m_waitHeat; }
    int maxWaitHeat() const { return m_maxWaitHeat; }
    double waitHeatLogLow() const { return m_waitHeatLogLow; }
    double waitHeatLogHigh() const { return m_waitHeatLogHigh; }
    void ensureOccupancyHeat();
    void ensureWaitHeat();
    const QVector<DebugOverlay>& debugOverlays() const { return m_debugOverlays; }
    const QVector<DebugDirectionMarker>& debugDirectionMarkers() const { return m_debugDirectionMarkers; }
    const QVector<HighwayDirection>& highwayDirections() const { return m_highwayDirections; }
    const DirectionMapSnapshot* directionMapAtTick(int tick) const;
    const QVector<QPointF>* taskErrands(int taskId) const;

    AgentFrame frameAt(int agentId, int tick) const;
    bool nextGoalForAgent(int agentId, int tick, QPointF* goal) const;
    int currentTaskForAgent(int agentId, int tick) const;
    bool currentTaskProgressForAgent(int agentId, int tick, int* task, int* currentErrand, int* totalErrands) const;
    bool priorityForAgent(int agentId, int tick, double* priority) const;
    bool isAgentDelayed(int agentId, int tick) const;
    bool debugDoorVoteForAgent(int agentId, int tick, int* vote, QPointF* voteCell, int* voteGroup = nullptr) const;
    bool guidePathForAgent(int agentId, int tick, QVector<QPointF>* path) const;
    bool assignmentSnapshotAt(int tick, AssignmentSnapshot* snapshot) const;
    bool plannerActionsForAgent(int agentId, int tick, ActionQueueSnapshot* snapshot) const;
    bool stagedActionsBeforeForAgent(int agentId, int tick, ActionQueueSnapshot* snapshot) const;
    bool stagedActionsForAgent(int agentId, int tick, ActionQueueSnapshot* snapshot) const;
    bool stagedLocsForAgent(int agentId, int tick, StagedLocSnapshot* snapshot) const;
    bool tpgQueuesForAgent(int agentId, int tick, TpgSnapshot* snapshot) const;
    QVector<PibtTraceEvent> pibtTraceForTick(int tick) const;

private:
    static int directionFromString(const QString& value);
    static QVector<unsigned char> decodePath(const QString& path, QString* error);
    bool loadTrackPath(const QString& path, AgentTrack* track, QString* error) const;
    void repairObstacleFrames(AgentTrack* track) const;
    void applyAction(AgentFrame& frame, unsigned char action) const;
    void loadTasksAndAssignments(const QJsonObject& root);
    void loadScheduleTimeline(const QJsonObject& root);
    void loadSandboxGoalTimeline(const QJsonObject& root);
    void loadPlannerPriorityTimeline(const QJsonObject& root, int agentCount);
    void loadGuidePathTimeline(const QJsonObject& root, int agentCount);
    void loadAssignmentSnapshots(const QJsonObject& root);
    void loadActionSnapshotTimeline(const QJsonObject& root,
                                    const QString& fieldName,
                                    int agentCount,
                                    QVector<QVector<ActionQueueSnapshot>>* destination);
    void loadStagedLocSnapshots(const QJsonObject& root, int agentCount);
    void anchorFramesToExecutorStateLocs(const QJsonObject& root, int agentCount);
    void loadTpgSnapshots(const QJsonObject& root);
    void loadPibtTraceTimeline(const QJsonObject& root);
    void loadDelayIntervals(const QJsonObject& root, int agentCount);
    void buildOccupancyHeat();
    void buildWaitHeat();
    void loadDebugOverlays(const QJsonObject& root);
    void loadDebugDirections(const QJsonObject& root);
    void loadHighwayDirections(const QJsonObject& root);
    void loadDirectionMap(const QJsonObject& root);
    bool isRoomMap() const;
    bool scheduledTaskAt(int agentId, int tick, int* task, int* assignedTick) const;
    bool trackReachedGoal(int agentId, int beginTick, int endTick, const QPointF& goal) const;
    int taskProgressForAgent(int agentId, int taskId, int tick) const;
    int relativeTick(int tick) const;

    int m_width = 0;
    int m_height = 0;
    int m_startTick = 0;
    int m_endTick = 0;
    int m_maxTick = 0;
    int m_displayTickOffset = 0;
    int m_ticksPerTimestep = 1;
    int m_finishedTasks = 0;
    bool m_isSandboxPlan = false;
    bool m_hasAgentOrientations = true;
    QString m_mapPath;
    QVector<unsigned char> m_map;
    QVector<AgentTrack> m_tracks;
    QVector<TaskInfo> m_tasks;
    QVector<QVector<AgentTaskAssignment>> m_agentAssignments;
    QVector<int> m_finishedTaskTicks;
    QVector<QVector<QPair<int, int>>> m_agentTaskTimeline;
    QVector<QVector<TaskProgressEvent>> m_taskProgressTimeline;
    QVector<QVector<QPointF>> m_sandboxGoalTimeline;
    QVector<QVector<double>> m_plannerPriorityTimeline;
    QVector<QVector<GuidePathSnapshot>> m_guidePathTimeline;
    QVector<AssignmentSnapshot> m_assignmentSnapshots;
    QVector<QVector<ActionQueueSnapshot>> m_plannerActionTimeline;
    QVector<QVector<ActionQueueSnapshot>> m_stagedActionBeforeTimeline;
    QVector<QVector<ActionQueueSnapshot>> m_stagedActionTimeline;
    QVector<QVector<StagedLocSnapshot>> m_stagedLocTimeline;
    QVector<TpgSnapshot> m_tpgSnapshots;
    QVector<QVector<PibtTraceEvent>> m_pibtTraceTimeline;
    QVector<QVector<QPair<int, int>>> m_delayIntervals;
    QVector<int> m_occupancyHeat;
    bool m_occupancyHeatBuilt = false;
    int m_maxOccupancyHeat = 0;
    double m_occupancyHeatLogLow = 0.0;
    double m_occupancyHeatLogHigh = 0.0;
    QVector<int> m_waitHeat;
    bool m_waitHeatBuilt = false;
    int m_maxWaitHeat = 0;
    double m_waitHeatLogLow = 0.0;
    double m_waitHeatLogHigh = 0.0;
    QVector<DebugOverlay> m_debugOverlays;
    QVector<DebugDirectionMarker> m_debugDirectionMarkers;
    QVector<HighwayDirection> m_highwayDirections;
    QVector<DirectionMapSnapshot> m_directionMapSnapshots;
};
