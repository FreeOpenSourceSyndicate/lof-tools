#ifndef LOFTOOLS_GUI_SANITIZER_PANEL_H
#define LOFTOOLS_GUI_SANITIZER_PANEL_H

#include <QString>

#include "core/sanitizer.h"
#include "gui/tool_panel.h"

class QLabel;
class QPushButton;
class QTableWidget;

namespace loftools {

// Tab that shows whether the game's cheat protection would flag the open file
// (or any character) and repairs it on request.  The file itself is opened via
// the main window (File > Open); this panel just reports on it.
class SanitizerPanel : public ToolPanel {
    Q_OBJECT

public:
    explicit SanitizerPanel(QWidget* parent = nullptr);

    QString tool_name() const override { return tr("Sanitizer"); }
    bool load_file(const QString& path) override;

private slots:
    void recheck();
    void fix();

private:
    void set_report(const SanitizeReport& rep);
    void update_buttons();

    QLabel* verdict_label_ = nullptr;
    QTableWidget* table_ = nullptr;
    QPushButton* fix_button_ = nullptr;
    QPushButton* recheck_button_ = nullptr;

    QString path_;
    SanitizeReport report_;
};

}  // namespace loftools

#endif  // LOFTOOLS_GUI_SANITIZER_PANEL_H
