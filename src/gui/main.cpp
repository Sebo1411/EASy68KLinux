#include <QApplication>
#include <QGuiApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("EASy68K");
    app.setApplicationDisplayName("EASy68K 68000 IDE & Simulator");
    app.setOrganizationName("EASy68K");
    QGuiApplication::setDesktopFileName(QStringLiteral("easy68k"));
    app.setWindowIcon(MainWindow::createAppWindowIcon());

    MainWindow window;
    window.show();

    return app.exec();
}
