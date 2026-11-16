#ifndef LOFTOOLS_GUI_MAIN_WINDOW_H
#define LOFTOOLS_GUI_MAIN_WINDOW_H

#include <QMainWindow>
#include <QString>

class QStackedWidget;
class QTabWidget;

namespace loftools {

class SanitizerPanel;
class CharacterEditorPanel;

// The application shell.  It owns the menu bar and one tab per tool; opening a
// file loads it into every tool, so new tools are added as a tab rather than a
// separate window.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Open `path` into every tool (File > Open, or a command-line path).
    void open_path(const QString& path);

    // Show the character-editor tab, optionally after loading `path`.
    void open_character_editor_path(const QString& path);

private slots:
    void open_file();
    void reload_file();
    void close_file();
    void show_sanitizer();
    void show_character_editor();
    void about();

private:
    void build_tools();
    void show_tools();

    QStackedWidget* central_ = nullptr;
    QWidget* welcome_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    SanitizerPanel* sanitizer_ = nullptr;
    CharacterEditorPanel* char_editor_ = nullptr;
    QString path_;
    QString last_dir_;
};

}  // namespace loftools

#endif  // LOFTOOLS_GUI_MAIN_WINDOW_H
