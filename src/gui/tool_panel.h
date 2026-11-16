#ifndef LOFTOOLS_GUI_TOOL_PANEL_H
#define LOFTOOLS_GUI_TOOL_PANEL_H

#include <QString>
#include <QWidget>

namespace loftools {

// Base class for a tool shown as a tab in the main window.  A tool is given the
// currently open file and reports back when it writes that file to disk, so the
// other tabs can refresh.  Adding a new tool is: subclass this, and add an
// instance to the tab widget in MainWindow.
class ToolPanel : public QWidget {
    Q_OBJECT

public:
    explicit ToolPanel(QWidget* parent = nullptr) : QWidget(parent) {}

    // Title shown on the tab.
    virtual QString tool_name() const = 0;

    // Load `path`.  Returns false when the file is not usable by this tool (the
    // panel should then explain why in its own view).
    virtual bool load_file(const QString& path) = 0;

signals:
    // The panel wrote `path` back to disk (e.g. a repair or an edit), so the
    // window should reload the other panels from it.
    void file_changed(const QString& path);
};

}  // namespace loftools

#endif  // LOFTOOLS_GUI_TOOL_PANEL_H
