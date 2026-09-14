#pragma once

#include "HighwayEditorData.h"
#include "HighwayEditorView.h"

#include <QMainWindow>

class QCheckBox;
class QLabel;
class QPushButton;
class QTextEdit;

class HighwayEditorWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit HighwayEditorWindow(QWidget* parent = nullptr);
    void loadInitialFiles(const QString& mapPath, const QString& jsonPath);

private:
    void openMap();
    void openJson();
    void saveJson();
    void exportJson();
    void exportCpp();
    void copyOutput();
    void updateStats();
    int selectedManualMask() const;
    void setLayer(HighwayEditorData::Layer layer);
    QPushButton* makeModeButton(const QString& text, HighwayEditorData::Layer layer);

    HighwayEditorData m_data;
    HighwayEditorView* m_view = nullptr;
    QLabel* m_stats = nullptr;
    QLabel* m_coord = nullptr;
    QTextEdit* m_output = nullptr;
    QVector<QPushButton*> m_modeButtons;
    QVector<QCheckBox*> m_dirChecks;
};
