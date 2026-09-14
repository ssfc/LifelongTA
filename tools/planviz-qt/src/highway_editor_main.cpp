#include "HighwayEditorWindow.h"

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("highway-editor-qt");
    QApplication::setOrganizationName("LORR");

    QCommandLineParser parser;
    parser.setApplicationDescription("Qt highway annotation editor");
    parser.addHelpOption();
    const QCommandLineOption mapOption({"m", "map"}, "Map file path.", "path");
    const QCommandLineOption jsonOption({"j", "json"}, "Annotation or plan JSON path.", "path");
    parser.addOption(mapOption);
    parser.addOption(jsonOption);
    parser.process(app);

    HighwayEditorWindow window;
    window.loadInitialFiles(parser.value(mapOption), parser.value(jsonOption));
    window.show();
    return app.exec();
}
