#ifndef LOFTOOLS_GUI_CHARACTER_EDITOR_PANEL_H
#define LOFTOOLS_GUI_CHARACTER_EDITOR_PANEL_H

#include <QString>
#include <QWidget>
#include <vector>

#include "core/document.h"
#include "gui/tool_panel.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTableWidget;

namespace loftools {

// The editable form for one Character.  It lives inside a scroll area, so it
// works on small screens.
class CharacterForm : public QWidget {
    Q_OBJECT

public:
    explicit CharacterForm(QWidget* parent = nullptr);

    void load(const Character& c);   // copy character -> widgets
    void apply(Character& c) const;  // copy widgets -> character

signals:
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
    bool loading_ = false;
};

// Tab that edits a ROST roster or a save game one character at a time.  Saving
// recomputes the integrity checksum, so the game's cheat protection accepts the
// file.
class CharacterEditorPanel : public ToolPanel {
    Q_OBJECT

public:
    explicit CharacterEditorPanel(QWidget* parent = nullptr);

    QString tool_name() const override { return tr("Character editor"); }
    bool load_file(const QString& path) override;

    // Show the form for `index` (0-based); ignored when out of range.  Used when
    // another tool (e.g. the save-game editor) wants to jump to a member.
    void select_slot(int index);

private slots:
    void on_slot_changed(int index);
    void on_save();
    void on_save_as();
    void on_reload();

private:
    void rebuild_forms();
    void update_header();
    void collect_edits();

    CharacterDocument doc_;
    QString path_;
    bool dirty_ = false;

    QListWidget* slots_ = nullptr;
    QStackedWidget* forms_ = nullptr;
    QPushButton* save_ = nullptr;
    QPushButton* save_as_ = nullptr;
    QLabel* header_ = nullptr;
    QLabel* note_ = nullptr;

    std::vector<CharacterForm*> slot_forms_;
};

}  // namespace loftools

#endif  // LOFTOOLS_GUI_CHARACTER_EDITOR_PANEL_H
