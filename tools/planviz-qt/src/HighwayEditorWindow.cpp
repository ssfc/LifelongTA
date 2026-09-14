#include "HighwayEditorWindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QTextEdit>
#include <QVBoxLayout>

HighwayEditorWindow::HighwayEditorWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Highway Editor Qt");
    resize(1500, 920);

    auto* central = new QWidget(this);
    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);

    auto* splitter = new QSplitter(Qt::Horizontal, central);
    root->addWidget(splitter);

    auto* side = new QWidget(splitter);
    auto* sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(12, 12, 12, 12);
    sideLayout->setSpacing(8);

    auto* title = new QLabel("<b>Highway Editor</b>", side);
    sideLayout->addWidget(title);

    auto* openMapButton = new QPushButton("Open Map", side);
    auto* openJsonButton = new QPushButton("Open/Import JSON", side);
    auto* saveJsonButton = new QPushButton("Save JSON", side);
    sideLayout->addWidget(openMapButton);
    sideLayout->addWidget(openJsonButton);
    sideLayout->addWidget(saveJsonButton);

    m_stats = new QLabel("Load a map and JSON to begin.", side);
    m_stats->setWordWrap(true);
    m_stats->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    m_stats->setMinimumHeight(118);
    sideLayout->addWidget(m_stats);

    auto* modeLabel = new QLabel("<b>Paint Mode</b>", side);
    sideLayout->addWidget(modeLabel);
    sideLayout->addWidget(makeModeButton("Yellow primary", HighwayEditorData::Layer::Primary));
    sideLayout->addWidget(makeModeButton("Blue secondary", HighwayEditorData::Layer::Secondary));
    sideLayout->addWidget(makeModeButton("Upper parking", HighwayEditorData::Layer::UpperParking));
    sideLayout->addWidget(makeModeButton("Lower parking", HighwayEditorData::Layer::LowerParking));
    sideLayout->addWidget(makeModeButton("Red manual penalty", HighwayEditorData::Layer::ManualPenalty));
    sideLayout->addWidget(makeModeButton("Erase", HighwayEditorData::Layer::Erase));

    auto* dirLabel = new QLabel("<b>Arrow dirs</b>", side);
    sideLayout->addWidget(dirLabel);
    auto* dirRow = new QWidget(side);
    auto* dirLayout = new QHBoxLayout(dirRow);
    dirLayout->setContentsMargins(0, 0, 0, 0);
    const QStringList labels = {"R", "D", "L", "U"};
    for (int i = 0; i < 4; ++i) {
        auto* check = new QCheckBox(labels[i], dirRow);
        check->setChecked(i == 0);
        m_dirChecks.push_back(check);
        dirLayout->addWidget(check);
        connect(check, &QCheckBox::toggled, this, [this]() {
            m_view->setManualMask(selectedManualMask());
        });
    }
    sideLayout->addWidget(dirRow);

    auto* fitFullButton = new QPushButton("Fit Full Map", side);
    auto* fitOrzButton = new QPushButton("Fit ORZ Tube", side);
    auto* showHighway = new QCheckBox("Highway arrows", side);
    showHighway->setChecked(true);
    sideLayout->addWidget(fitFullButton);
    sideLayout->addWidget(fitOrzButton);
    sideLayout->addWidget(showHighway);

    auto* exportLabel = new QLabel("<b>Export</b>", side);
    sideLayout->addWidget(exportLabel);
    auto* exportJsonButton = new QPushButton("Export JSON Text", side);
    auto* exportCppButton = new QPushButton("Export C++ Arrays", side);
    auto* copyButton = new QPushButton("Copy Output", side);
    sideLayout->addWidget(exportJsonButton);
    sideLayout->addWidget(exportCppButton);
    sideLayout->addWidget(copyButton);
    m_output = new QTextEdit(side);
    m_output->setLineWrapMode(QTextEdit::NoWrap);
    m_output->setFontFamily("Consolas");
    sideLayout->addWidget(m_output, 1);

    m_view = new HighwayEditorView(splitter);
    m_view->setData(&m_data);

    splitter->addWidget(side);
    splitter->addWidget(m_view);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({320, 1180});

    m_coord = new QLabel("row,col", this);
    statusBar()->addPermanentWidget(m_coord);
    setCentralWidget(central);

    connect(openMapButton, &QPushButton::clicked, this, &HighwayEditorWindow::openMap);
    connect(openJsonButton, &QPushButton::clicked, this, &HighwayEditorWindow::openJson);
    connect(saveJsonButton, &QPushButton::clicked, this, &HighwayEditorWindow::saveJson);
    connect(exportJsonButton, &QPushButton::clicked, this, &HighwayEditorWindow::exportJson);
    connect(exportCppButton, &QPushButton::clicked, this, &HighwayEditorWindow::exportCpp);
    connect(copyButton, &QPushButton::clicked, this, &HighwayEditorWindow::copyOutput);
    connect(fitFullButton, &QPushButton::clicked, m_view, &HighwayEditorView::resetView);
    connect(fitOrzButton, &QPushButton::clicked, m_view, &HighwayEditorView::fitOrzTube);
    connect(showHighway, &QCheckBox::toggled, m_view, &HighwayEditorView::setShowHighway);
    connect(m_view, &HighwayEditorView::statsChanged, this, &HighwayEditorWindow::updateStats);
    connect(m_view, &HighwayEditorView::hoveredCellChanged, this, [this](int row, int col) {
        m_coord->setText(QString("%1,%2").arg(row).arg(col));
    });

    setLayer(HighwayEditorData::Layer::ManualPenalty);
}

void HighwayEditorWindow::loadInitialFiles(const QString& mapPath, const QString& jsonPath)
{
    QString error;
    if (!mapPath.isEmpty() && !m_data.loadMap(mapPath, &error)) {
        QMessageBox::warning(this, "Map load failed", error);
    }
    if (!jsonPath.isEmpty() && !m_data.loadJson(jsonPath, &error)) {
        QMessageBox::warning(this, "JSON load failed", error);
    }
    m_view->setData(&m_data);
    updateStats();
}

void HighwayEditorWindow::openMap()
{
    const QString path = QFileDialog::getOpenFileName(this, "Open map", QString(), "Maps (*.map *.txt);;All files (*)");
    if (path.isEmpty()) {
        return;
    }
    QString error;
    if (!m_data.loadMap(path, &error)) {
        QMessageBox::warning(this, "Map load failed", error);
        return;
    }
    m_view->setData(&m_data);
    updateStats();
}

void HighwayEditorWindow::openJson()
{
    const QString path = QFileDialog::getOpenFileName(this, "Open annotation/plan JSON", QString(), "JSON (*.json);;All files (*)");
    if (path.isEmpty()) {
        return;
    }
    QString error;
    if (!m_data.loadJson(path, &error)) {
        QMessageBox::warning(this, "JSON load failed", error);
        return;
    }
    m_view->update();
    updateStats();
}

void HighwayEditorWindow::saveJson()
{
    const QString path = QFileDialog::getSaveFileName(this, "Save annotation JSON", QString(), "JSON (*.json);;All files (*)");
    if (path.isEmpty()) {
        return;
    }
    QString error;
    if (!m_data.saveJson(path, &error)) {
        QMessageBox::warning(this, "Save failed", error);
        return;
    }
    statusBar()->showMessage("Saved " + path, 5000);
}

void HighwayEditorWindow::exportJson()
{
    m_output->setPlainText(m_data.exportJsonText());
}

void HighwayEditorWindow::exportCpp()
{
    m_output->setPlainText(m_data.exportCppText());
}

void HighwayEditorWindow::copyOutput()
{
    QGuiApplication::clipboard()->setText(m_output->toPlainText());
    statusBar()->showMessage("Output copied.", 3000);
}

void HighwayEditorWindow::updateStats()
{
    m_stats->setText(QString("Map: %1 x %2\n"
                             "Yellow primary: %3\n"
                             "Blue secondary: %4\n"
                             "Upper parking: %5\n"
                             "Lower parking: %6\n"
                             "Red manual penalty: %7 cells / %8 dirs\n"
                             "Highway arrows: %9")
                         .arg(m_data.rows())
                         .arg(m_data.cols())
                         .arg(m_data.primaryCount())
                         .arg(m_data.secondaryCount())
                         .arg(m_data.upperParkingCount())
                         .arg(m_data.lowerParkingCount())
                         .arg(m_data.manualPenaltyCellCount())
                         .arg(m_data.manualPenaltyMoveCount())
                         .arg(m_data.highwayArrowCount()));
}

int HighwayEditorWindow::selectedManualMask() const
{
    int mask = 0;
    for (int i = 0; i < m_dirChecks.size(); ++i) {
        if (m_dirChecks[i]->isChecked()) {
            mask |= HighwayEditorData::dirMask(i + 2);
        }
    }
    return mask;
}

void HighwayEditorWindow::setLayer(HighwayEditorData::Layer layer)
{
    m_view->setLayer(layer);
    m_view->setManualMask(selectedManualMask());
    for (QPushButton* button : m_modeButtons) {
        button->setChecked(button->property("layer").toInt() == static_cast<int>(layer));
    }
}

QPushButton* HighwayEditorWindow::makeModeButton(const QString& text, HighwayEditorData::Layer layer)
{
    auto* button = new QPushButton(text, this);
    button->setCheckable(true);
    button->setProperty("layer", static_cast<int>(layer));
    m_modeButtons.push_back(button);
    connect(button, &QPushButton::clicked, this, [this, layer]() {
        setLayer(layer);
    });
    return button;
}
