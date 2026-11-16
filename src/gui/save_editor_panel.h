#ifndef LOFTOOLS_GUI_SAVE_EDITOR_PANEL_H
#define LOFTOOLS_GUI_SAVE_EDITOR_PANEL_H

#include <QString>
#include <QWidget>

#include "core/document.h"
#include "gui/tool_panel.h"

class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace loftools {

// Tab that edits the non-character state of a save game: which slots form the
// party and where the party is (location and facing).  The character data
// itself is edited in the Character editor tab, which shares the same document
// model; this panel deliberately only touches state the character editor does
// not.
//
// Every edit is written through CharacterDocument, whose save() refreshes the
// whole-file checksum, so the game's cheat protection accepts the file.
class SaveEditorPanel : public ToolPanel {
    Q_OBJECT

public:
    explicit SaveEditorPanel(QWidget* parent = nullptr);

    QString tool_name() const override { return tr("Save game editor"); }
    bool load_file(const QString& path) override;

signals:
    // The user asked to edit a party member; the window should switch to the
    // character editor tab on the same slot.
    void edit_character_requested(int slot);

private slots:
    void on_party_changed(int row, int column);
    void on_location_changed();
    void on_facing_changed();
    void on_save();
    void on_save_as();
    void on_reload();

private:
    void build_ui();
    void refresh();
    void update_header();
    void update_derived();
    void collect_state();

    CharacterDocument doc_;
    QString path_;
    bool loading_ = false;
    bool dirty_ = false;

    QLabel* header_ = nullptr;
    QLabel* note_ = nullptr;

    QTableWidget* party_ = nullptr;

    QSpinBox* location_ = nullptr;
    QComboBox* facing_ = nullptr;
    QLabel* derived_ = nullptr;

    QPushButton* save_ = nullptr;
    QPushButton* save_as_ = nullptr;
};

}  // namespace loftools

#endif  // LOFTOOLS_GUI_SAVE_EDITOR_PANEL_H
