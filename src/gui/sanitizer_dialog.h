#ifndef LOFTOOLS_GUI_SANITIZER_DIALOG_H
#define LOFTOOLS_GUI_SANITIZER_DIALOG_H

#include <QDialog>
#include <QString>

#include "core/sanitizer.h"

class QLabel;
class QPushButton;
class QTableWidget;

namespace loftools {

// Dialog that opens a save game or ROST file, shows whether the game's cheat
// protection would flag it (or any character), and repairs it on request.
class SanitizerDialog : public QDialog {
    Q_OBJECT

public:
    explicit SanitizerDialog(QWidget* parent = nullptr);

    // Load `path`, inspect it, and populate the view.
    void load_and_show(const QString& path);

private slots:
    void browse();
    void recheck();
    void fix();

private:
    void set_report(const SanitizeReport& rep);
    void update_buttons();

    QLabel* file_label_ = nullptr;
    QLabel* verdict_label_ = nullptr;
    QTableWidget* table_ = nullptr;
    QPushButton* fix_button_ = nullptr;
    QPushButton* recheck_button_ = nullptr;

    QString path_;
    SanitizeReport report_;
};

}  // namespace loftools

#endif  // LOFTOOLS_GUI_SANITIZER_DIALOG_H
