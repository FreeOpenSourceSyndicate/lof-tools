#ifndef LOFTOOLS_GUI_CHARACTER_EDITOR_DIALOG_H
#define LOFTOOLS_GUI_CHARACTER_EDITOR_DIALOG_H

#include <QDialog>
#include <QString>
#include <vector>

#include <QWidget>

#include "core/document.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTableWidget;
class QTabWidget;

namespace loftools {

// A form that reads/writes one Character.  The editor keeps one per slot in a
// QStackedWidget; switching slots just changes the visible page.
class CharacterForm : public QWidget {
    Q_OBJECT

public:
    explicit CharacterForm(QWidget* parent = nullptr);

    // Copy the character's values into the widgets.
    void load(const Character& c);
    // Copy the widget values back into the character.
    void apply(Character& c) const;

signals:
    // Emitted whenever any editor on the form changes (used for dirty state).
    void changed();

private slots:
    void on_any_change();

private:
    QLineEdit* name_ = nullptr;
    QSpinBox* level_ = nullptr;
    QComboBox* sex_ = nullptr;
    QComboBox* race_ = nullptr;
    QComboBox* profession_ = nullptr;
    QComboBox* alignment_ = nullptr;

    QSpinBox* str_ = nullptr;
    QSpinBox* con_ = nullptr;
    QSpinBox* dex_ = nullptr;
    QSpinBox* intel_ = nullptr;
    QSpinBox* wis_ = nullptr;

    QSpinBox* hp_cur_ = nullptr;
    QSpinBox* hp_max_ = nullptr;
    QSpinBox* mp_cur_ = nullptr;
    QSpinBox* mp_max_ = nullptr;

    QSpinBox* exp_ = nullptr;
    QSpinBox* gold_ = nullptr;
    QSpinBox* deposit_ = nullptr;
    QSpinBox* rations_ = nullptr;

    std::vector<QSpinBox*> skills_;
    std::vector<QCheckBox*> languages_;

    QTableWidget* inventory_ = nullptr;
    QTableWidget* spells_ = nullptr;
    QLabel* derived_ = nullptr;
    bool loading_ = false;  // true while load() populates widgets
};

// The character editor: open a ROST roster or a save game, edit one character
// at a time, and save the file back (with the integrity checksum recomputed).
class CharacterEditorDialog : public QDialog {
    Q_OBJECT

public:
    explicit CharacterEditorDialog(QWidget* parent = nullptr);

    // Load `path` and select it for editing.  Returns false on failure.
    bool load(const QString& path);
    // Ask the user for a file, then load it.
    bool open_file_dialog();

private slots:
    void on_slot_changed(int index);
    void on_save();
    void on_save_as();
    void on_reload();

private:
    void rebuild_forms();
    void update_title();
    // Write the visible forms back into the document (without saving to disk).
    void collect_edits();

    CharacterDocument doc_;
    QString path_;
    bool dirty_ = false;

    QListWidget* slots_ = nullptr;
    QStackedWidget* forms_ = nullptr;
    QTabWidget* detail_tabs_ = nullptr;
    QPushButton* save_ = nullptr;
    QPushButton* save_as_ = nullptr;
    QLabel* header_ = nullptr;

    std::vector<CharacterForm*> slot_forms_;
};

}  // namespace loftools

#endif  // LOFTOOLS_GUI_CHARACTER_EDITOR_DIALOG_H
