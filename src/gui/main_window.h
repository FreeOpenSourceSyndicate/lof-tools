#ifndef LOFTOOLS_GUI_MAIN_WINDOW_H
#define LOFTOOLS_GUI_MAIN_WINDOW_H

#include <QMainWindow>
#include <QString>

class QLabel;

namespace loftools {

class SanitizerDialog;

// The application shell.  It owns the menu bar; the sanitizer lives in its own
// dialog so that the future save-game / character editors can be added as
// further menu entries without restructuring this window.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Load a file into the sanitizer and show it (used for a path given on the
    // command line).
    void open_path(const QString& path);

private slots:
    void open_file();
    void open_sanitizer();
    void about();

private:
    SanitizerDialog* sanitizer_ = nullptr;
    QLabel* hint_ = nullptr;
    QString last_dir_;
};

}  // namespace loftools

#endif  // LOFTOOLS_GUI_MAIN_WINDOW_H
