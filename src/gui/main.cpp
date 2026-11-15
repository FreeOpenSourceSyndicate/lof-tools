#include <QApplication>

#include "gui/main_window.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("lof-tools");
    app.setApplicationDisplayName("Legend of Faerghail Tools");
    app.setOrganizationName("FreeOpenSourceSyndicate");

    loftools::MainWindow window;
    window.show();
    // Optional command-line argument: open a file straight away.
    if (argc > 1) window.open_path(QString::fromLocal8Bit(argv[1]));
    return app.exec();
}
