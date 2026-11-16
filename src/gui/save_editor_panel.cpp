#include "gui/save_editor_panel.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include "core/tables.h"

namespace loftools {

SaveEditorPanel::SaveEditorPanel(QWidget* parent) : ToolPanel(parent) {
    build_ui();
    update_header();
}

void SaveEditorPanel::build_ui() {
    auto* root = new QVBoxLayout(this);

    header_ = new QLabel(this);
    header_->setWordWrap(true);
    root->addWidget(header_);

    // --- party roster (informational) -------------------------------------
    auto* party_box = new QGroupBox(tr("Party"), this);
    auto* party_layout = new QVBoxLayout(party_box);
    party_ = new QTableWidget(kSavePartySlots, 6, party_box);
    party_->setHorizontalHeaderLabels(
        {tr("Slot"), tr("Name"), tr("Race"), tr("Profession"), tr("Rank"),
         tr("HP")});
    party_->verticalHeader()->setVisible(false);
    party_->setSelectionBehavior(QAbstractItemView::SelectRows);
    party_->setSelectionMode(QAbstractItemView::SingleSelection);
    party_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    party_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    party_->setMinimumHeight(150);
    connect(party_, &QTableWidget::cellDoubleClicked, this,
            &SaveEditorPanel::on_party_changed);
    party_layout->addWidget(party_);

    auto* party_hint = new QLabel(
        tr("Double-click a member (or use <b>Tools &gt; Character editor</b>) to "
           "change name, attributes, skills, inventory and spells."),
        party_box);
    party_hint->setWordWrap(true);
    party_layout->addWidget(party_hint);
    root->addWidget(party_box);

    // --- party position ---------------------------------------------------
    auto* pos_box = new QGroupBox(tr("Position"), this);
    auto* pos_form = new QFormLayout(pos_box);
    location_ = new QSpinBox(pos_box);
    location_->setRange(0, (int)kSaveLocationMax);
    location_->setToolTip(
        tr("Dungeon id; the engine loads DNGS\\DNG<id> from it (0..99)."));
    facing_ = new QComboBox(pos_box);
    for (uint8_t i = 0; i < kFacingCount; ++i)
        facing_->addItem(facing_name(i), i);
    pos_form->addRow(tr("Location (dungeon id)"), location_);
    pos_form->addRow(tr("Facing"), facing_);
    derived_ = new QLabel(pos_box);
    derived_->setWordWrap(true);
    pos_form->addRow(tr("Loads map"), derived_);
    root->addWidget(pos_box);
    root->addStretch(1);

    note_ = new QLabel(this);
    note_->setWordWrap(true);
    note_->setText(tr(
        "Saving recomputes the file's integrity checksum, so the game's cheat "
        "protection will not flag it."));
    root->addWidget(note_);

    auto* buttons = new QHBoxLayout();
    auto* reload = new QPushButton(tr("Reload"), this);
    save_as_ = new QPushButton(tr("Save as..."), this);
    save_ = new QPushButton(tr("Save"), this);
    save_->setDefault(true);
    buttons->addWidget(reload);
    buttons->addStretch(1);
    buttons->addWidget(save_as_);
    buttons->addWidget(save_);
    root->addLayout(buttons);

    connect(location_, &QSpinBox::valueChanged, this,
            &SaveEditorPanel::on_location_changed);
    connect(facing_, &QComboBox::currentIndexChanged, this,
            &SaveEditorPanel::on_facing_changed);
    connect(reload, &QPushButton::clicked, this, &SaveEditorPanel::on_reload);
    connect(save_, &QPushButton::clicked, this, &SaveEditorPanel::on_save);
    connect(save_as_, &QPushButton::clicked, this, &SaveEditorPanel::on_save_as);
}

bool SaveEditorPanel::load_file(const QString& path) {
    doc_ = CharacterDocument::load(path.toLocal8Bit().constData());
    path_ = path;
    dirty_ = false;
    refresh();
    update_header();
    return doc_.ok();
}

void SaveEditorPanel::refresh() {
    loading_ = true;

    const bool save = doc_.ok() && doc_.is_save_game();

    for (int row = 0; row < (int)kSavePartySlots; ++row) {
        const Character* c = save ? doc_.character((size_t)row) : nullptr;
        auto set = [&](int col, const QString& text) {
            auto* item = new QTableWidgetItem(text);
            party_->setItem(row, col, item);
        };
        set(0, QString::number(row + 1));
        if (c && c->present()) {
            set(1, QString::fromLatin1(c->name().c_str()));
            set(2, race_name(c->race()));
            set(3, profession_name(c->profession()));
            set(4, QString::number(c->level()));
            set(5, QStringLiteral("%1 / %2")
                       .arg(c->hp_cur())
                       .arg(c->hp_max()));
        } else {
            set(1, tr("(empty)"));
            for (int col = 2; col <= 5; ++col) set(col, QString());
        }
    }

    if (save) {
        location_->setValue(doc_.save_location());
        facing_->setCurrentIndex(facing_->findData(doc_.save_facing()));
    } else {
        location_->setValue(0);
        facing_->setCurrentIndex(0);
    }
    update_derived();

    const bool enabled = save;
    party_->setEnabled(enabled);
    location_->setEnabled(enabled);
    facing_->setEnabled(enabled);
    save_->setEnabled(enabled);
    save_as_->setEnabled(enabled);

    loading_ = false;
}

void SaveEditorPanel::update_derived() {
    const int loc = location_->value();
    const std::string map = dungeon_map_name((uint16_t)loc);
    derived_->setText(tr("<b>%1</b> - %2 (region %3), facing %4")
                          .arg(QString::fromStdString(map))
                          .arg(scenario_name((uint8_t)(loc % 10)))
                          .arg(loc % 10)
                          .arg(facing_->currentText()));
}

void SaveEditorPanel::on_party_changed(int row, int /*column*/) {
    if (row >= 0 && row < (int)kSavePartySlots)
        emit edit_character_requested(row);
}

void SaveEditorPanel::on_location_changed() {
    update_derived();
    if (!loading_) {
        dirty_ = true;
        update_header();
    }
}

void SaveEditorPanel::on_facing_changed() {
    update_derived();
    if (!loading_) {
        dirty_ = true;
        update_header();
    }
}

void SaveEditorPanel::collect_state() {
    if (!doc_.ok() || !doc_.is_save_game()) return;
    doc_.set_save_location((uint16_t)location_->value());
    doc_.set_save_facing((uint8_t)facing_->currentData().toInt());
}

void SaveEditorPanel::on_save() {
    collect_state();
    if (!doc_.save()) {
        QMessageBox::warning(this, tr("Save failed"),
                             QString::fromStdString(doc_.error()));
        return;
    }
    dirty_ = false;
    refresh();
    update_header();
    QMessageBox::information(
        this, tr("Saved"),
        tr("Wrote %1.\nThe integrity checksum was recomputed, so the game's "
           "cheat protection will accept the file.")
            .arg(path_));
    emit file_changed(path_);
}

void SaveEditorPanel::on_save_as() {
    collect_state();
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save as"), path_, tr("Save game (*);;All files (*)"));
    if (path.isEmpty()) return;
    if (!doc_.save(path.toLocal8Bit().constData())) {
        QMessageBox::warning(this, tr("Save failed"),
                             QString::fromStdString(doc_.error()));
        return;
    }
    path_ = path;
    dirty_ = false;
    refresh();
    update_header();
    QMessageBox::information(this, tr("Saved"),
                             tr("Wrote %1 with a recomputed checksum.").arg(path_));
    emit file_changed(path_);
}

void SaveEditorPanel::on_reload() {
    if (!path_.isEmpty()) load_file(path_);
}

void SaveEditorPanel::update_header() {
    if (!doc_.ok()) {
        header_->setText(tr("<b>No file loaded.</b> Use File > Open in the menu "
                            "bar to open a save game."));
        return;
    }
    if (!doc_.is_save_game()) {
        header_->setText(
            tr("<b>%1</b> is a ROST roster, not a save game. This editor edits "
               "save-game state (party position); open a file from GAMES/ to "
               "use it.")
                .arg(path_));
        return;
    }
    header_->setText(tr("<b>%1</b> - save game (%2).%3")
                         .arg(path_)
                         .arg(tr("party position and roster"))
                         .arg(dirty_ ? tr("  Unsaved changes.") : QString()));
}

}  // namespace loftools
