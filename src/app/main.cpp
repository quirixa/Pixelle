#include "MainWindow.h"
#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Pixelle");
    QApplication::setApplicationDisplayName("Pixelle");
    QApplication::setApplicationVersion("0.2.0");
    QApplication::setOrganizationName("Pixelle");
    QGuiApplication::setDesktopFileName("pixelle");

    QCommandLineParser parser;
    parser.setApplicationDescription("Pixelle - raster image editor");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file", "Image to open.", "[file]");
    parser.process(app);

    MainWindow w;
    if (!parser.positionalArguments().isEmpty()) w.openFile(parser.positionalArguments().first());
    w.show();
    return app.exec();
}
