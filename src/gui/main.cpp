#include <QApplication>
#include <QStringList>

#include "gui/main_window.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setApplicationName("lof-tools");
    app.setApplicationDisplayName("Legend of Faerghail Tools");
    app.setOrganizationName("FreeOpenSourceSyndicate");

    loftools::MainWindow window;
    window.show();

    // Optional arguments:
    //   lof-tools <file>              open the file in the sanitizer
    //   lof-tools --editor <file>     open the character editor on the file
    const QStringList args = app.arguments();
    if (args.size() > 1) {
        if (args[1] == "--editor") {
            window.open_character_editor_path(args.size() > 2 ? args[2] : QString());
        } else {
            window.open_path(args[1]);
        }
    }
    return app.exec();
}
