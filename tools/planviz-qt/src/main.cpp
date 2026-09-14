#include "MainWindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QStringList>

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

int main(int argc, char* argv[])
{
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif

    QApplication app(argc, argv);
    QApplication::setApplicationName("PlanViz Qt");

    QString mapPath;
    QString planPath;
    QString tracePath;
    int agents = 0;
    int startTick = 0;
    int endTick = 0;

    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); i++) {
        const QString key = args[i];
        if (key == "--help" || key == "-h") {
            QMessageBox::information(nullptr, "PlanViz Qt",
                                     "Usage: planviz-qt --trace <lifelongta-trace.json> [--n <agents>] [--start <tick>] [--end <tick>]\n"
                                     "   or: planviz-qt --map <map> --plan <json> [--n <agents>] [--start <tick>] [--end <tick>]");
            return 0;
        }
        if (i + 1 >= args.size()) {
            QMessageBox::critical(nullptr, "PlanViz Qt", "Missing value for argument: " + key);
            return 1;
        }
        const QString value = args[++i];
        if (key == "--map" || key == "-m") {
            mapPath = value;
        } else if (key == "--plan" || key == "-p") {
            planPath = value;
        } else if (key == "--trace" || key == "-t") {
            tracePath = value;
        } else if (key == "--n") {
            agents = value.toInt();
        } else if (key == "--start") {
            startTick = value.toInt();
        } else if (key == "--end") {
            endTick = value.toInt();
        } else {
            QMessageBox::critical(nullptr, "PlanViz Qt", "Unknown argument: " + key);
            return 1;
        }
    }

    if (tracePath.isEmpty() && (mapPath.isEmpty() || planPath.isEmpty())) {
        QMessageBox::critical(nullptr, "PlanViz Qt",
                              "Usage: planviz-qt --trace <lifelongta-trace.json> [--n <agents>] [--start <tick>] [--end <tick>]\n"
                              "   or: planviz-qt --map <map> --plan <json> [--n <agents>] [--start <tick>] [--end <tick>]");
        return 1;
    }

    MainWindow window;
    QString error;
    const bool loaded = tracePath.isEmpty()
        ? window.load(mapPath, planPath, agents, startTick, endTick, &error)
        : window.loadLifelongTrace(tracePath, agents, startTick, endTick, &error);
    if (!loaded) {
        QMessageBox::critical(nullptr, "PlanViz Qt", error);
        return 2;
    }

    window.show();
    return app.exec();
}
