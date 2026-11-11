#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("EASy68K");
    app.setApplicationDisplayName("EASy68K 68000 IDE & Simulator");
    app.setOrganizationName("EASy68K");

    MainWindow window;
    window.show();

    return app.exec();
}
