#include "MainWindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>

namespace {
QString idLinksHtml(const QVector<int>& ids, const QString& kind)
{
    QStringList parts;
    parts.reserve(ids.size());
    for (int id : ids) {
        if (id >= 0) {
            parts.push_back(QString("<a href=\"%1:%2\">%2</a>").arg(kind).arg(id));
        }
    }
    return QString("<style>a{color:#185abd;text-decoration:none;} body{font-size:13pt;}</style>%1")
        .arg(parts.join(", "));
}

QTextBrowser* makeIdBrowser(QWidget* parent)
{
    auto* browser = new QTextBrowser(parent);
    browser->setOpenExternalLinks(false);
    browser->setOpenLinks(false);
    browser->setFrameShape(QFrame::NoFrame);
    browser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    browser->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    browser->setReadOnly(true);
    browser->setMinimumHeight(96);
    return browser;
}

QString cellHtml(const ActionQueueSnapshot& snapshot)
{
    if (!snapshot.hasBaseCell) {
        return "n/a";
    }
    return QString("(%1, %2)")
        .arg(static_cast<int>(snapshot.baseCell.y()))
        .arg(static_cast<int>(snapshot.baseCell.x()));
}

QString actionQueueHtml(const QString& title,
                        const QString& baseLabel,
                        const ActionQueueSnapshot& snapshot,
                        bool available,
                        bool showSourceTick = false)
{
    const QString muted = "color:#687076;";
    if (!available) {
        return QString("<div><b>%1</b><br><span style=\"%2\">No snapshot</span></div>")
            .arg(title, muted);
    }

    QStringList visible = snapshot.actions;
    const int maxActions = 24;
    QString suffix;
    if (visible.size() > maxActions) {
        suffix = QString(" ... +%1").arg(visible.size() - maxActions);
        visible = visible.mid(0, maxActions);
    }
    const QString body = visible.isEmpty()
        ? QString("<span style=\"%1\">empty</span>").arg(muted)
        : QString("<code>%1%2</code>").arg(visible.join(" "), suffix);
    const QString sourceLine = showSourceTick
        ? QString("<br><span style=\"%1\">computed/start @ Tick %2</span>").arg(muted).arg(snapshot.sourceTick)
        : QString();
    return QString("<div><b>%1 @ Tick %2</b>%3<br><span style=\"%4\">%5: %6</span><br>%7</div>")
        .arg(title)
        .arg(snapshot.tick)
        .arg(sourceLine)
        .arg(muted)
        .arg(baseLabel)
        .arg(cellHtml(snapshot))
        .arg(body);
}

QString stagedLocsHtml(const StagedLocSnapshot& snapshot, bool available)
{
    const QString muted = "color:#687076;";
    if (!available) {
        return QString("<div><b>Executor staged locs</b><br><span style=\"%1\">No snapshot</span></div>")
            .arg(muted);
    }

    QStringList cells;
    const int maxCells = 24;
    for (int i = 0; i < snapshot.cells.size() && i < maxCells; ++i) {
        const QPointF& cell = snapshot.cells[i];
        cells.push_back(QString("(%1,%2)")
                            .arg(static_cast<int>(cell.y()))
                            .arg(static_cast<int>(cell.x())));
    }
    QString suffix;
    if (snapshot.cells.size() > maxCells) {
        suffix = QString(" ... +%1").arg(snapshot.cells.size() - maxCells);
    }
    const QString body = cells.isEmpty()
        ? QString("<span style=\"%1\">empty</span>").arg(muted)
        : QString("<code>%1%2</code>").arg(cells.join(" "), suffix);
    return QString("<div><b>Executor staged locs @ Tick %1</b><br>%2</div>")
        .arg(snapshot.tick)
        .arg(body);
}

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

QString cornerRewriteHtml(const ActionQueueSnapshot& plannerSnapshot,
                          bool hasPlanner,
                          const ActionQueueSnapshot& stagedSnapshot,
                          bool hasStaged)
{
    if (!hasPlanner || !hasStaged || !isCornerRewrite(plannerSnapshot.actions, stagedSnapshot.actions)) {
        return QString();
    }

    const QString plannerPrefix = plannerSnapshot.actions.mid(0, 4).join(" ");
    const QString stagedPrefix = stagedSnapshot.actions.mid(0, 3).join(" ");
    return QString("<div style=\"border-left:4px solid #f28c28;padding-left:8px;\">"
                   "<b style=\"color:#a95000;\">Corner rewrite detected</b><br>"
                   "<span style=\"color:#687076;\">planner prefix:</span> <code>%1</code><br>"
                   "<span style=\"color:#687076;\">staged prefix:</span> <code>%2</code>"
                   "</div>")
        .arg(plannerPrefix, stagedPrefix);
}

QString tpgQueuesHtml(const TpgSnapshot& snapshot, bool available)
{
    const QString muted = "color:#687076;";
    if (!available) {
        return QString("<div><b>TPG queues</b><br><span style=\"%1\">No snapshot</span></div>")
            .arg(muted);
    }

    if (snapshot.queues.isEmpty()) {
        return QString("<div><b>TPG queues @ Tick %1</b><br><span style=\"%2\">agent not in any queue</span></div>")
            .arg(snapshot.tick)
            .arg(muted);
    }

    QStringList lines;
    const int maxQueues = 8;
    for (int i = 0; i < snapshot.queues.size() && i < maxQueues; ++i) {
        const TpgQueue& queue = snapshot.queues[i];
        QStringList agents;
        for (int agent : queue.agents) {
            agents.push_back(QString::number(agent));
        }
        lines.push_back(QString("(%1, %2): <code>%3</code>")
                            .arg(static_cast<int>(queue.cell.y()))
                            .arg(static_cast<int>(queue.cell.x()))
                            .arg(agents.join(" ")));
    }
    if (snapshot.queues.size() > maxQueues) {
        lines.push_back(QString("<span style=\"%1\">... +%2 more</span>")
                            .arg(muted)
                            .arg(snapshot.queues.size() - maxQueues));
    }

    return QString("<div><b>TPG queues @ Tick %1</b><br>%2</div>")
        .arg(snapshot.tick)
        .arg(lines.join("<br>"));
}
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    auto* root = new QWidget(this);
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(6, 6, 6, 6);

    auto* content = new QHBoxLayout();
    content->setSpacing(8);
    m_view = new MapView(root);
    content->addWidget(m_view, 1);

    auto* sidePanel = new QWidget(root);
    sidePanel->setMinimumWidth(360);
    sidePanel->setMaximumWidth(520);
    auto* sideLayout = new QVBoxLayout(sidePanel);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    auto* detailSplitter = new QSplitter(Qt::Vertical, sidePanel);
    detailSplitter->setChildrenCollapsible(false);
    detailSplitter->setHandleWidth(7);
    detailSplitter->setStyleSheet(
        "QSplitter::handle:vertical { background: #d5d9dd; margin: 2px 0; }"
        "QSplitter::handle:vertical:hover { background: #7a8793; }");

    auto* assignmentSection = new QWidget(detailSplitter);
    auto* assignmentLayout = new QVBoxLayout(assignmentSection);
    assignmentLayout->setContentsMargins(0, 0, 0, 0);
    auto* assignmentTitle = new QLabel("Assignment Snapshot", assignmentSection);
    assignmentTitle->setStyleSheet("font-weight: 600; font-size: 14pt;");
    m_assignmentTable = new QTableWidget(2, 3, assignmentSection);
    m_assignmentTable->setHorizontalHeaderLabels({"Set", "Count", "Ids"});
    QFont assignmentFont = m_assignmentTable->font();
    assignmentFont.setPointSize(12);
    m_assignmentTable->setFont(assignmentFont);
    QFont assignmentHeaderFont = assignmentFont;
    assignmentHeaderFont.setPointSize(12);
    assignmentHeaderFont.setBold(true);
    m_assignmentTable->horizontalHeader()->setFont(assignmentHeaderFont);
    m_assignmentTable->verticalHeader()->setVisible(false);
    m_assignmentTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_assignmentTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_assignmentTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_assignmentTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_assignmentTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_assignmentTable->setWordWrap(true);
    m_assignmentTable->setTextElideMode(Qt::ElideNone);
    m_assignmentTable->setMinimumHeight(120);
    m_assignmentTable->setRowHeight(0, 130);
    m_assignmentTable->setRowHeight(1, 170);
    m_assignmentTable->setItem(0, 0, new QTableWidgetItem("Free agents"));
    m_assignmentTable->setItem(1, 0, new QTableWidgetItem("Free tasks"));
    m_freeAgentIds = makeIdBrowser(m_assignmentTable);
    m_freeTaskIds = makeIdBrowser(m_assignmentTable);
    m_assignmentTable->setCellWidget(0, 2, m_freeAgentIds);
    m_assignmentTable->setCellWidget(1, 2, m_freeTaskIds);
    assignmentLayout->addWidget(assignmentTitle);
    assignmentLayout->addWidget(m_assignmentTable, 1);

    auto* actionSection = new QWidget(detailSplitter);
    auto* actionLayout = new QVBoxLayout(actionSection);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    auto* actionTitle = new QLabel("Action Pipeline", actionSection);
    actionTitle->setStyleSheet("font-weight: 600; font-size: 14pt;");
    m_actionPipelineBrowser = new QTextBrowser(actionSection);
    m_actionPipelineBrowser->setOpenExternalLinks(false);
    m_actionPipelineBrowser->setFrameShape(QFrame::StyledPanel);
    m_actionPipelineBrowser->setReadOnly(true);
    m_actionPipelineBrowser->setMinimumHeight(120);
    actionLayout->addWidget(actionTitle);
    actionLayout->addWidget(m_actionPipelineBrowser, 1);

    detailSplitter->addWidget(assignmentSection);
    detailSplitter->addWidget(actionSection);
    detailSplitter->setStretchFactor(0, 1);
    detailSplitter->setStretchFactor(1, 1);
    detailSplitter->setSizes({430, 520});
    sideLayout->addWidget(detailSplitter, 1);
    content->addWidget(sidePanel);
    layout->addLayout(content, 1);

    auto* controls = new QHBoxLayout();
    auto* prev = new QPushButton("Prev", root);
    m_playButton = new QPushButton("Play", root);
    auto* next = new QPushButton("Next", root);
    auto* reset = new QPushButton("Fullsize", root);
    auto* toggles = new QHBoxLayout();
    auto* ids = new QCheckBox("Agent ids", root);
    auto* goalLines = new QCheckBox("Goal lines", root);
    m_goalLinesCheck = goalLines;
    auto* buffers = new QCheckBox("Buffers", root);
    auto* directions = new QCheckBox("Door dir", root);
    auto* highway = new QCheckBox("Highway", root);
    auto* directionMap = new QCheckBox("Direction map", root);
    auto* heatMap = new QCheckBox("Heat map", root);
    auto* waitMap = new QCheckBox("Wait map", root);
    auto* heatOutlines = new QCheckBox("Heat outlines", root);
    auto* guidePath = new QCheckBox("Guide path", root);
    auto* stagedLocs = new QCheckBox("Staged locs", root);
    auto* freeAgents = new QCheckBox("Free agents", root);
    m_freeAgentsCheck = freeAgents;
    auto* freeTasks = new QCheckBox("Free tasks", root);
    auto* delayedAgents = new QCheckBox("Delayed", root);
    auto* coords = new QCheckBox("Coords", root);
    auto* pibtTrace = new QCheckBox("PIBT trace", root);
    ids->setChecked(true);
    goalLines->setChecked(true);
    buffers->setChecked(true);
    directions->setChecked(true);
    highway->setChecked(true);
    directionMap->setChecked(true);
    heatMap->setChecked(false);
    waitMap->setChecked(false);
    heatOutlines->setChecked(false);
    guidePath->setChecked(true);
    stagedLocs->setChecked(true);
    freeAgents->setChecked(true);
    freeTasks->setChecked(false);
    delayedAgents->setChecked(true);
    coords->setChecked(false);
    pibtTrace->setChecked(true);
    m_view->setShowAgentIds(true);
    m_view->setShowAllGoalArrows(true);
    m_view->setShowDebugOverlays(true);
    m_view->setShowOrzBottlenecks(false);
    m_view->setShowDebugDirections(true);
    m_view->setShowHighwayDirections(true);
    m_view->setShowDirectionMap(true);
    m_view->setShowHeatMap(false);
    m_view->setShowWaitMap(false);
    m_view->setShowHeatOutlines(false);
    m_view->setShowGuidePath(true);
    m_view->setShowStagedLocs(true);
    m_view->setHighlightFreeAgents(true);
    m_view->setShowFreeTasks(false);
    m_view->setShowDelayedAgents(true);
    m_view->setShowCoordinates(false);
    m_view->setShowPibtTrace(true);
    auto* tickText = new QLabel("Tick", root);
    m_tickInput = new QSpinBox(root);
    m_tickInput->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_tickInput->setKeyboardTracking(false);
    m_tickInput->setMinimumWidth(72);
    auto* agentText = new QLabel("Agent", root);
    m_agentInput = new QSpinBox(root);
    m_agentInput->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_agentInput->setKeyboardTracking(false);
    m_agentInput->setMinimumWidth(72);
    m_agentInput->setRange(-1, -1);
    m_agentInput->setSpecialValueText("none");
    m_agentInput->setValue(-1);
    m_agentInput->installEventFilter(this);
    m_slider = new QSlider(Qt::Horizontal, root);
    m_label = new QLabel("Tick 0", root);
    m_label->setMinimumWidth(220);
    m_selectedLabel = new QLabel("Selected none", root);
    m_selectedLabel->setMinimumWidth(520);
    m_selectedLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    controls->addWidget(tickText);
    controls->addWidget(m_tickInput);
    controls->addSpacing(8);
    controls->addWidget(agentText);
    controls->addWidget(m_agentInput);
    controls->addWidget(m_slider, 1);
    controls->addWidget(m_label);
    layout->addLayout(controls);

    toggles->addWidget(prev);
    toggles->addWidget(m_playButton);
    toggles->addWidget(next);
    toggles->addWidget(reset);
    toggles->addSpacing(8);
    toggles->addWidget(ids);
    toggles->addWidget(goalLines);
    toggles->addWidget(buffers);
    toggles->addWidget(directions);
    toggles->addWidget(highway);
    toggles->addWidget(directionMap);
    toggles->addWidget(heatMap);
    toggles->addWidget(waitMap);
    toggles->addWidget(heatOutlines);
    toggles->addWidget(guidePath);
    toggles->addWidget(stagedLocs);
    toggles->addWidget(freeAgents);
    toggles->addWidget(freeTasks);
    toggles->addWidget(delayedAgents);
    toggles->addWidget(coords);
    toggles->addWidget(pibtTrace);
    toggles->addStretch(1);
    layout->addLayout(toggles);

    auto* selectedInfo = new QHBoxLayout();
    selectedInfo->addWidget(m_selectedLabel);
    selectedInfo->addStretch(1);
    layout->addLayout(selectedInfo);

    setCentralWidget(root);
    setWindowTitle("PlanViz Qt");

    connect(prev, &QPushButton::clicked, this, &MainWindow::prevTick);
    connect(next, &QPushButton::clicked, this, &MainWindow::nextTick);
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::playPause);
    connect(reset, &QPushButton::clicked, m_view, &MapView::resetView);
    connect(ids, &QCheckBox::toggled, m_view, &MapView::setShowAgentIds);
    connect(goalLines, &QCheckBox::toggled, m_view, &MapView::setShowAllGoalArrows);
    connect(buffers, &QCheckBox::toggled, m_view, &MapView::setShowDebugOverlays);
    connect(directions, &QCheckBox::toggled, m_view, &MapView::setShowDebugDirections);
    connect(highway, &QCheckBox::toggled, m_view, &MapView::setShowHighwayDirections);
    connect(directionMap, &QCheckBox::toggled, m_view, &MapView::setShowDirectionMap);
    connect(heatMap, &QCheckBox::toggled, m_view, &MapView::setShowHeatMap);
    connect(waitMap, &QCheckBox::toggled, m_view, &MapView::setShowWaitMap);
    connect(heatOutlines, &QCheckBox::toggled, m_view, &MapView::setShowHeatOutlines);
    connect(guidePath, &QCheckBox::toggled, m_view, &MapView::setShowGuidePath);
    connect(stagedLocs, &QCheckBox::toggled, m_view, &MapView::setShowStagedLocs);
    connect(freeAgents, &QCheckBox::toggled, m_view, &MapView::setHighlightFreeAgents);
    connect(freeTasks, &QCheckBox::toggled, m_view, &MapView::setShowFreeTasks);
    connect(delayedAgents, &QCheckBox::toggled, m_view, &MapView::setShowDelayedAgents);
    connect(coords, &QCheckBox::toggled, m_view, &MapView::setShowCoordinates);
    connect(pibtTrace, &QCheckBox::toggled, m_view, &MapView::setShowPibtTrace);
    connect(m_freeAgentIds, &QTextBrowser::anchorClicked, this, &MainWindow::handleAssignmentLink);
    connect(m_freeTaskIds, &QTextBrowser::anchorClicked, this, &MainWindow::handleAssignmentLink);
    connect(m_view, &MapView::selectedAgentChanged, this, [this](int agent) {
        if (m_agentInput && m_agentInput->value() != agent) {
            const QSignalBlocker blocker(m_agentInput);
            m_agentInput->setValue(agent);
        }
        updateLabel();
        updateActionPipeline();
    });
    connect(m_slider, &QSlider::valueChanged, this, &MainWindow::sliderChanged);
    connect(m_tickInput, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::tickInputChanged);
    connect(m_agentInput, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::agentInputChanged);
    connect(&m_timer, &QTimer::timeout, this, &MainWindow::nextTick);
    m_timer.setInterval(30);
}

bool MainWindow::load(const QString& mapPath, const QString& planPath, int agentLimit, int startTick, int endTick, QString* error)
{
    if (!m_data.loadMap(mapPath, error)) {
        return false;
    }
    if (!m_data.loadPlan(planPath, agentLimit, startTick, endTick, error)) {
        return false;
    }
    if (m_goalLinesCheck) {
        const bool showGoalLinesByDefault =
            !QFileInfo(mapPath).fileName().toLower().startsWith("orz");
        m_goalLinesCheck->setChecked(showGoalLinesByDefault);
        m_view->setShowAllGoalArrows(showGoalLinesByDefault);
    }
    if (m_freeAgentsCheck) {
        const bool showFreeAgents = !m_data.isSandboxPlan();
        m_freeAgentsCheck->setChecked(showFreeAgents);
        m_view->setHighlightFreeAgents(showFreeAgents);
    }
    m_tick = m_data.startTick();
    m_slider->setRange(m_data.startTick(), m_data.endTick());
    m_slider->setValue(m_tick);
    m_tickInput->setRange(m_data.startTick(), m_data.endTick());
    m_tickInput->setValue(m_tick);
    m_view->setPlanData(&m_data);
    const QSignalBlocker agentInputBlocker(m_agentInput);
    m_agentInput->setRange(-1, std::max(-1, m_data.teamSize() - 1));
    m_agentInput->setSpecialValueText("none");
    m_agentInput->setValue(m_view->selectedAgent());
    m_agentInput->setEnabled(m_data.teamSize() > 0);
    m_view->setTick(m_tick);
    updateLabel();
    updateAssignmentTable();
    updateActionPipeline();
    setWindowTitle(QString("PlanViz Qt - %1 agents - %2")
                       .arg(m_data.teamSize())
                       .arg(QFileInfo(mapPath).fileName()));
    showMaximized();
    QTimer::singleShot(100, m_view, &MapView::resetView);
    return true;
}

bool MainWindow::loadLifelongTrace(const QString& tracePath, int agentLimit, int startTick, int endTick, QString* error)
{
    if (!m_data.loadLifelongTrace(tracePath, agentLimit, startTick, endTick, error)) {
        return false;
    }
    if (m_goalLinesCheck) {
        m_goalLinesCheck->setChecked(true);
        m_view->setShowAllGoalArrows(true);
    }
    if (m_freeAgentsCheck) {
        m_freeAgentsCheck->setChecked(true);
        m_view->setHighlightFreeAgents(true);
    }
    m_tick = m_data.startTick();
    m_slider->setRange(m_data.startTick(), m_data.endTick());
    m_slider->setValue(m_tick);
    m_tickInput->setRange(m_data.startTick(), m_data.endTick());
    m_tickInput->setValue(m_tick);
    m_view->setPlanData(&m_data);
    const QSignalBlocker agentInputBlocker(m_agentInput);
    m_agentInput->setRange(-1, std::max(-1, m_data.teamSize() - 1));
    m_agentInput->setSpecialValueText("none");
    m_agentInput->setValue(m_view->selectedAgent());
    m_agentInput->setEnabled(m_data.teamSize() > 0);
    m_view->setTick(m_tick);
    updateLabel();
    updateAssignmentTable();
    updateActionPipeline();
    setWindowTitle(QString("PlanViz Qt - LifelongTA trace - %1 agents - %2")
                       .arg(m_data.teamSize())
                       .arg(QFileInfo(tracePath).fileName()));
    showMaximized();
    QTimer::singleShot(100, m_view, &MapView::resetView);
    return true;
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_agentInput && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            m_agentInput->interpretText();
            centerOnAgentInput();
            event->accept();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    QWidget* focus = QApplication::focusWidget();
    if (focus == m_tickInput || (m_tickInput && m_tickInput->isAncestorOf(focus)) ||
        focus == m_agentInput || (m_agentInput && m_agentInput->isAncestorOf(focus))) {
        QMainWindow::keyPressEvent(event);
        return;
    }
    if (event->key() == Qt::Key_Left) {
        prevTick();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Right) {
        nextTick();
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::playPause()
{
    if (m_timer.isActive()) {
        m_timer.stop();
        m_playButton->setText("Play");
    } else {
        m_timer.start();
        m_playButton->setText("Pause");
    }
}

void MainWindow::nextTick()
{
    if (m_tick >= m_data.endTick()) {
        m_timer.stop();
        m_playButton->setText("Play");
        return;
    }
    updateTick(m_tick + 1);
}

void MainWindow::prevTick()
{
    updateTick(std::max(m_data.startTick(), m_tick - 1));
}

void MainWindow::sliderChanged(int value)
{
    updateTick(value);
}

void MainWindow::tickInputChanged(int value)
{
    updateTick(value);
}

void MainWindow::agentInputChanged(int value)
{
    if (!m_view) {
        return;
    }
    m_view->setSelectedAgent(value);
}

void MainWindow::centerOnAgentInput()
{
    if (!m_view || !m_agentInput) {
        return;
    }
    const int agent = m_agentInput->value();
    m_view->setSelectedAgent(agent);
    if (agent >= 0) {
        m_view->centerOnAgent(agent);
    }
}

void MainWindow::updateTick(int tick)
{
    if (tick == m_tick) {
        return;
    }
    m_tick = tick;
    if (m_slider->value() != tick) {
        const QSignalBlocker blocker(m_slider);
        m_slider->setValue(tick);
    }
    if (m_tickInput->value() != tick) {
        const QSignalBlocker blocker(m_tickInput);
        m_tickInput->setValue(tick);
    }
    m_view->setTick(m_tick);
    updateLabel();
    updateAssignmentTable();
    updateActionPipeline();
}

void MainWindow::updateLabel()
{
    QString selectedText = "Selected none";
    if (m_view && m_view->selectedAgent() >= 0) {
        const int agent = m_view->selectedAgent();
        double priority = 0.0;
        const QString priorityText = m_data.priorityForAgent(agent, m_tick, &priority)
            ? QString::number(priority, 'f', 2)
            : QString("n/a");
        int task = -1;
        int currentErrand = 0;
        int totalErrands = 0;
        QString taskText = QString::number(m_data.currentTaskForAgent(agent, m_tick));
        if (m_data.currentTaskProgressForAgent(agent, m_tick, &task, &currentErrand, &totalErrands)) {
            taskText = QString("%1 E%2/%3")
                           .arg(task)
                           .arg(currentErrand)
                           .arg(totalErrands);
        }
        const AgentFrame frame = m_data.frameAt(agent, m_tick);
        const int col = static_cast<int>(std::floor(frame.cell.x() + 0.5));
        const int row = static_cast<int>(std::floor(frame.cell.y() + 0.5));
        QString cellText = "out";
        if (row >= 0 && row < m_data.height() && col >= 0 && col < m_data.width()) {
            const int index = row * m_data.width() + col;
            const bool free =
                index >= 0 && index < m_data.mapCells().size() &&
                m_data.mapCells()[index] == 0;
            cellText = QString("(%1,%2) %3")
                           .arg(row)
                           .arg(col)
                           .arg(free ? "free" : "obstacle");
        }
        selectedText = QString("Selected Agent %1 | Cell %2 | Task %3 | Priority %4 | Delay %5")
                           .arg(agent)
                           .arg(cellText)
                           .arg(taskText)
                           .arg(priorityText)
                           .arg(m_data.isAgentDelayed(agent, m_tick) ? "yes" : "no");
    }

    m_label->setText(QString("Tick %1 / %2 | Agents %3 | Finished %4/%5")
                         .arg(m_tick)
                         .arg(m_data.endTick())
                         .arg(m_data.teamSize())
                         .arg(m_data.finishedTasksAtTick(m_tick))
                         .arg(m_data.finishedTasks()));
    if (m_selectedLabel) {
        m_selectedLabel->setText(selectedText);
    }
}

void MainWindow::updateAssignmentTable()
{
    if (!m_assignmentTable) {
        return;
    }

    AssignmentSnapshot snapshot;
    if (!m_data.assignmentSnapshotAt(m_tick, &snapshot)) {
        m_assignmentTable->setHorizontalHeaderLabels({"Set", "Count", "No snapshot"});
        m_assignmentTable->setItem(0, 1, new QTableWidgetItem("0"));
        m_assignmentTable->setItem(1, 1, new QTableWidgetItem("0"));
        if (m_freeAgentIds) m_freeAgentIds->clear();
        if (m_freeTaskIds) m_freeTaskIds->clear();
        return;
    }

    m_assignmentTable->setHorizontalHeaderLabels({
        "Set",
        "Count",
        QString("Ids @ %1").arg(snapshot.tick)
    });
    m_assignmentTable->setItem(0, 1, new QTableWidgetItem(QString::number(snapshot.freeAgents.size())));
    m_assignmentTable->setItem(1, 1, new QTableWidgetItem(QString::number(snapshot.freeTasks.size())));
    if (m_freeAgentIds) {
        m_freeAgentIds->setHtml(idLinksHtml(snapshot.freeAgents, "agent"));
    }
    if (m_freeTaskIds) {
        m_freeTaskIds->setHtml(idLinksHtml(snapshot.freeTasks, "task"));
    }
    m_assignmentTable->setRowHeight(0, 130);
    m_assignmentTable->setRowHeight(1, 170);
}

void MainWindow::updateActionPipeline()
{
    if (!m_actionPipelineBrowser) {
        return;
    }
    if (!m_view || m_view->selectedAgent() < 0) {
        m_actionPipelineBrowser->setHtml("<style>body{font-size:15pt;color:#687076;}</style>Select an agent.");
        return;
    }

    const int agent = m_view->selectedAgent();
    ActionQueueSnapshot plannerSnapshot;
    ActionQueueSnapshot stagedBeforeSnapshot;
    ActionQueueSnapshot stagedSnapshot;
    StagedLocSnapshot stagedLocSnapshot;
    TpgSnapshot tpgSnapshot;
    const bool hasPlanner = m_data.plannerActionsForAgent(agent, m_tick, &plannerSnapshot);
    const bool hasStagedBefore = m_data.stagedActionsBeforeForAgent(agent, m_tick, &stagedBeforeSnapshot);
    const bool hasStaged = m_data.stagedActionsForAgent(agent, m_tick, &stagedSnapshot);
    const bool hasStagedLocs = m_data.stagedLocsForAgent(agent, m_tick, &stagedLocSnapshot);
    const bool hasTpg = m_data.tpgQueuesForAgent(agent, m_tick, &tpgSnapshot);
    const AgentTrack& track = m_data.tracks()[agent];
    const int relTick = m_tick - m_data.startTick();
    QString actual = "n/a";
    if (relTick >= 0 && relTick < track.actions.size()) {
        actual = QString(QChar(track.actions[relTick]));
    }

    QString html;
    html += "<style>body{font-size:15pt;} code{font-family:Consolas,monospace;"
            "font-size:15pt;background:#f3f6fa;padding:2px 4px;} div{margin-bottom:12px;}</style>";
    html += QString("<div><b>Agent %1 @ Tick %2</b><br>Actual action: <code>%3</code></div>")
                .arg(agent)
                .arg(m_tick)
                .arg(actual);
    html += actionQueueHtml("Planner actions accepted", "start state", plannerSnapshot, hasPlanner, true);
    html += cornerRewriteHtml(plannerSnapshot, hasPlanner, stagedSnapshot, hasStaged);
    html += actionQueueHtml("Executor staged before", "base system", stagedBeforeSnapshot, hasStagedBefore);
    html += actionQueueHtml("Executor staged after", "base system", stagedSnapshot, hasStaged);
    html += stagedLocsHtml(stagedLocSnapshot, hasStagedLocs);
    html += tpgQueuesHtml(tpgSnapshot, hasTpg);
    m_actionPipelineBrowser->setHtml(html);
}

void MainWindow::handleAssignmentLink(const QUrl& url)
{
    if (!m_view) {
        return;
    }

    bool ok = false;
    const int id = url.path().toInt(&ok);
    if (!ok) {
        return;
    }

    if (url.scheme() == "agent") {
        m_view->setSelectedAgent(id);
        updateLabel();
        updateActionPipeline();
        return;
    }

    if (url.scheme() == "task") {
        m_view->setHighlightedTask(id);
    }
}
