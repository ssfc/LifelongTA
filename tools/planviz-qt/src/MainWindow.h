#pragma once

#include "MapView.h"
#include "PlanData.h"

#include <QMainWindow>
#include <QTimer>

class QLabel;
class QCheckBox;
class QPushButton;
class QSlider;
class QSpinBox;
class QTableWidget;
class QTextBrowser;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    bool load(const QString& mapPath, const QString& planPath, int agentLimit, int startTick, int endTick, QString* error);
    bool loadLifelongTrace(const QString& tracePath, int agentLimit, int startTick, int endTick, QString* error);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void playPause();
    void nextTick();
    void prevTick();
    void sliderChanged(int value);
    void tickInputChanged(int value);
    void agentInputChanged(int value);
    void centerOnAgentInput();

private:
    void updateTick(int tick);
    void updateLabel();
    void updateAssignmentTable();
    void updateActionPipeline();
    void handleAssignmentLink(const QUrl& url);

    PlanData m_data;
    MapView* m_view = nullptr;
    QSlider* m_slider = nullptr;
    QSpinBox* m_tickInput = nullptr;
    QSpinBox* m_agentInput = nullptr;
    QLabel* m_label = nullptr;
    QLabel* m_selectedLabel = nullptr;
    QTableWidget* m_assignmentTable = nullptr;
    QTextBrowser* m_freeAgentIds = nullptr;
    QTextBrowser* m_freeTaskIds = nullptr;
    QTextBrowser* m_actionPipelineBrowser = nullptr;
    QCheckBox* m_goalLinesCheck = nullptr;
    QCheckBox* m_freeAgentsCheck = nullptr;
    QPushButton* m_playButton = nullptr;
    QTimer m_timer;
    int m_tick = 0;
};
