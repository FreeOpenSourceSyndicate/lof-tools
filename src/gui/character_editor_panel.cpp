#include "gui/character_editor_panel.h"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include "core/tables.h"

namespace loftools {

namespace {

constexpr int kStatMax = 255;
constexpr int kSkillMax = 100;

QSpinBox* make_spin(int lo, int hi, QWidget* parent) {
    auto* s = new QSpinBox(parent);
    s->setRange(lo, hi);
    return s;
}

void fill_item_combo(QComboBox* box) {
    box->addItem(QObject::tr("(empty)"), 0);
    for (size_t i = 0; i < kItemCount; ++i)
        box->addItem(QString::fromLatin1(kItems[i].name), (int)(i + 1));
}

void fill_spell_combo(QComboBox* box) {
    box->addItem(QObject::tr("(empty)"), 0);
    for (size_t i = 0; i < kSpellCount; ++i)
        box->addItem(QString::fromLatin1(kSpellNames[i]), (int)(i + 1));
}

int combo_index_for_value(const QComboBox* box, int value) {
    const int idx = box->findData(value);
    return idx >= 0 ? idx : 0;
}

}  // namespace

// ---------------------------------------------------------------------------
// CharacterForm
// ---------------------------------------------------------------------------

CharacterForm::CharacterForm(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);

    auto* top = new QHBoxLayout();

    auto* identity = new QGroupBox(tr("Identity"), this);
    auto* id_form = new QFormLayout(identity);
    name_ = new QLineEdit(this);
    name_->setMaxLength(13);
    level_ = make_spin(0, 255, this);
    sex_ = new QComboBox(this);
    for (uint8_t i = 0; i < kSexCount; ++i) sex_->addItem(sex_name(i), i);
    race_ = new QComboBox(this);
    for (uint8_t i = 0; i < kRaceCount; ++i) race_->addItem(race_name(i), i);
    profession_ = new QComboBox(this);
    for (uint8_t i = 0; i < kProfessionCount; ++i)
        profession_->addItem(profession_name(i), i);
    alignment_ = new QComboBox(this);
    for (uint8_t i = 0; i < kAlignmentCount; ++i)
        alignment_->addItem(alignment_name(i), i);
    id_form->addRow(tr("Name"), name_);
    id_form->addRow(tr("Rank (level)"), level_);
    id_form->addRow(tr("Sex"), sex_);
    id_form->addRow(tr("Race"), race_);
    id_form->addRow(tr("Profession"), profession_);
    id_form->addRow(tr("Alignment"), alignment_);
    top->addWidget(identity);

    auto* attrs = new QGroupBox(tr("Attributes"), this);
    auto* attr_form = new QFormLayout(attrs);
    str_ = make_spin(0, kStatMax, this);
    con_ = make_spin(0, kStatMax, this);
    dex_ = make_spin(0, kStatMax, this);
    intel_ = make_spin(0, kStatMax, this);
    wis_ = make_spin(0, kStatMax, this);
    attr_form->addRow(tr("Strength"), str_);
    attr_form->addRow(tr("Constitution"), con_);
    attr_form->addRow(tr("Dexterity"), dex_);
    attr_form->addRow(tr("Intelligence"), intel_);
    attr_form->addRow(tr("Wisdom"), wis_);
    top->addWidget(attrs);

    auto* vitals = new QGroupBox(tr("Vitals"), this);
    auto* vit_form = new QFormLayout(vitals);
    hp_cur_ = make_spin(0, 65535, this);
    hp_max_ = make_spin(0, 65535, this);
    mp_cur_ = make_spin(0, 255, this);
    mp_max_ = make_spin(0, 255, this);
    vit_form->addRow(tr("Hit points (current)"), hp_cur_);
    vit_form->addRow(tr("Hit points (max)"), hp_max_);
    vit_form->addRow(tr("Magic points (current)"), mp_cur_);
    vit_form->addRow(tr("Magic points (max)"), mp_max_);
    top->addWidget(vitals);

    auto* money = new QGroupBox(tr("Money && Progress"), this);
    auto* money_form = new QFormLayout(money);
    exp_ = make_spin(-2147483647, 2147483647, this);
    gold_ = make_spin(0, 2147483647, this);
    deposit_ = make_spin(0, 2147483647, this);
    rations_ = make_spin(0, 255, this);
    money_form->addRow(tr("Experience"), exp_);
    money_form->addRow(tr("Gold (purse)"), gold_);
    money_form->addRow(tr("Gold (deposit)"), deposit_);
    money_form->addRow(tr("Rations"), rations_);
    top->addWidget(money);

    root->addLayout(top);

    auto* mid = new QHBoxLayout();

    auto* skills = new QGroupBox(tr("Skills (%)"), this);
    auto* skill_grid = new QGridLayout(skills);
    for (size_t i = 0; i < kSkillCount; ++i) {
        skill_grid->addWidget(new QLabel(QString::fromLatin1(skill_name(i)), this),
                              (int)i, 0);
        auto* spin = make_spin(0, kSkillMax, this);
        skills_.push_back(spin);
        skill_grid->addWidget(spin, (int)i, 1);
    }
    mid->addWidget(skills);

    auto* langs = new QGroupBox(tr("Languages"), this);
    auto* lang_grid = new QGridLayout(langs);
    for (size_t i = 0; i < kLanguageCount; ++i) {
        auto* cb = new QCheckBox(QString::fromLatin1(language_name(i)), this);
        languages_.push_back(cb);
        lang_grid->addWidget(cb, (int)i, 0);
    }
    mid->addWidget(langs);

    inventory_ = new QTableWidget(char_off::kInventorySlots, 5, this);
    inventory_->setHorizontalHeaderLabels(
        {tr("#"), tr("Item"), tr("Equipped"), tr("Condition %"), tr("Charges")});
    inventory_->verticalHeader()->setVisible(false);
    inventory_->setSelectionMode(QAbstractItemView::NoSelection);
    for (size_t s = 0; s < char_off::kInventorySlots; ++s) {
        auto* num = new QTableWidgetItem(QString::number(s + 1));
        num->setFlags(Qt::ItemIsEnabled);
        inventory_->setItem((int)s, 0, num);

        auto* combo = new QComboBox(inventory_);
        fill_item_combo(combo);
        inventory_->setCellWidget((int)s, 1, combo);

        auto* eq = new QTableWidgetItem();
        eq->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        eq->setCheckState(Qt::Unchecked);
        inventory_->setItem((int)s, 2, eq);

        inventory_->setCellWidget((int)s, 3, make_spin(0, kSkillMax, inventory_));
        inventory_->setCellWidget((int)s, 4, make_spin(0, 255, inventory_));
    }
    inventory_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    inventory_->setMinimumHeight(180);
    mid->addWidget(inventory_, 2);

    root->addLayout(mid, 1);

    spells_ = new QTableWidget(char_off::kSpellSlots, 3, this);
    spells_->setHorizontalHeaderLabels({tr("#"), tr("Spell"), tr("Casts")});
    spells_->verticalHeader()->setVisible(false);
    spells_->setSelectionMode(QAbstractItemView::NoSelection);
    for (size_t s = 0; s < char_off::kSpellSlots; ++s) {
        auto* num = new QTableWidgetItem(QString::number(s + 1));
        num->setFlags(Qt::ItemIsEnabled);
        spells_->setItem((int)s, 0, num);
        auto* combo = new QComboBox(spells_);
        fill_spell_combo(combo);
        spells_->setCellWidget((int)s, 1, combo);
        spells_->setCellWidget((int)s, 2, make_spin(0, 255, spells_));
    }
    spells_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    spells_->setMaximumHeight(170);
    root->addWidget(spells_);

    derived_ = new QLabel(this);
    derived_->setWordWrap(true);
    derived_->setTextFormat(Qt::RichText);
    root->addWidget(derived_);

    for (QSpinBox* s : skills_)
        connect(s, &QSpinBox::valueChanged, this, &CharacterForm::on_any_change);
    for (QCheckBox* c : languages_)
        connect(c, &QCheckBox::toggled, this, &CharacterForm::on_any_change);
    connect(name_, &QLineEdit::textChanged, this, &CharacterForm::on_any_change);
    connect(level_, &QSpinBox::valueChanged, this, &CharacterForm::on_any_change);
    connect(str_, &QSpinBox::valueChanged, this, &CharacterForm::on_any_change);
    connect(con_, &QSpinBox::valueChanged, this, &CharacterForm::on_any_change);
    connect(dex_, &QSpinBox::valueChanged, this, &CharacterForm::on_any_change);
    connect(intel_, &QSpinBox::valueChanged, this, &CharacterForm::on_any_change);
    connect(wis_, &QSpinBox::valueChanged, this, &CharacterForm::on_any_change);
    connect(hp_cur_, &QSpinBox::valueChanged, this, &CharacterForm::on_any_change);
    connect(hp_max_, &QSpinBox::valueChanged, this, &CharacterForm::on_any_change);
    connect(mp_cur_, &QSpinBox::valueChanged, this, &CharacterForm::on_any_change);
    connect(mp_max_, &QSpinBox::valueChanged, this, &CharacterForm::on_any_change);
    connect(exp_, &QSpinBox::valueChanged, this, &CharacterForm::on_any_change);
    connect(gold_, &QSpinBox::valueChanged, this, &CharacterForm::on_any_change);
    connect(deposit_, &QSpinBox::valueChanged, this, &CharacterForm::on_any_change);
    connect(rations_, &QSpinBox::valueChanged, this, &CharacterForm::on_any_change);
    connect(sex_, &QComboBox::currentIndexChanged, this, &CharacterForm::on_any_change);
    connect(race_, &QComboBox::currentIndexChanged, this, &CharacterForm::on_any_change);
    connect(profession_, &QComboBox::currentIndexChanged, this,
            &CharacterForm::on_any_change);
    connect(alignment_, &QComboBox::currentIndexChanged, this,
            &CharacterForm::on_any_change);
}

void CharacterForm::on_any_change() {
    if (!loading_) emit changed();
}

void CharacterForm::load(const Character& c) {
    loading_ = true;
    name_->setText(QString::fromLatin1(c.name().c_str()));
    level_->setValue(c.level());
    sex_->setCurrentIndex(combo_index_for_value(sex_, c.sex()));
    race_->setCurrentIndex(combo_index_for_value(race_, c.race()));
    profession_->setCurrentIndex(combo_index_for_value(profession_, c.profession()));
    alignment_->setCurrentIndex(combo_index_for_value(alignment_, c.alignment()));

    str_->setValue(c.str());
    con_->setValue(c.con());
    dex_->setValue(c.dex());
    intel_->setValue(c.intel());
    wis_->setValue(c.wis());

    hp_cur_->setValue(c.hp_cur());
    hp_max_->setValue(c.hp_max());
    mp_cur_->setValue(c.mp_cur());
    mp_max_->setValue(c.mp_max());

    exp_->setValue(c.experience());
    gold_->setValue((int)std::min<uint32_t>(c.gold(), 2147483647u));
    deposit_->setValue((int)std::min<uint32_t>(c.deposit(), 2147483647u));
    rations_->setValue(c.rations());

    for (size_t i = 0; i < skills_.size(); ++i) skills_[i]->setValue(c.skill(i));
    for (size_t i = 0; i < languages_.size(); ++i)
        languages_[i]->setChecked(c.knows_language(i));

    for (size_t s = 0; s < char_off::kInventorySlots; ++s) {
        InvSlot slot = c.inventory(s);
        auto* combo = qobject_cast<QComboBox*>(inventory_->cellWidget((int)s, 1));
        combo->setCurrentIndex(combo_index_for_value(combo, slot.item));
        inventory_->item((int)s, 2)->setCheckState(slot.equipped ? Qt::Checked
                                                                 : Qt::Unchecked);
        qobject_cast<QSpinBox*>(inventory_->cellWidget((int)s, 3))
            ->setValue(slot.condition);
        qobject_cast<QSpinBox*>(inventory_->cellWidget((int)s, 4))
            ->setValue(slot.charges);
    }

    for (size_t s = 0; s < char_off::kSpellSlots; ++s) {
        SpellSlot sp = c.spell(s);
        auto* combo = qobject_cast<QComboBox*>(spells_->cellWidget((int)s, 1));
        combo->setCurrentIndex(combo_index_for_value(combo, sp.id));
        qobject_cast<QSpinBox*>(spells_->cellWidget((int)s, 2))->setValue(sp.casts);
    }

    derived_->setText(
        tr("<b>Read-only (recomputed by the engine):</b> defence %1, "
           "weight %2/%3 (shown /10), inventory slots used %4, spells %5. "
           "<b>Tamper flag:</b> %6")
            .arg(c.defence())
            .arg(c.weight_cur())
            .arg(c.weight_max())
            .arg(c.inv_used())
            .arg(c.spell_count())
            .arg(c.tampered() ? tr("SET (0xFC) - will be cleared on save")
                              : tr("clear")));
    loading_ = false;
}

void CharacterForm::apply(Character& c) const {
    c.set_name(name_->text().toLatin1().constData());
    c.set_level((uint8_t)level_->value());
    c.set_sex((uint8_t)sex_->currentData().toInt());
    c.set_race((uint8_t)race_->currentData().toInt());
    c.set_profession((uint8_t)profession_->currentData().toInt());
    c.set_alignment((uint8_t)alignment_->currentData().toInt());

    c.set_str((uint8_t)str_->value());
    c.set_con((uint8_t)con_->value());
    c.set_dex((uint8_t)dex_->value());
    c.set_intel((uint8_t)intel_->value());
    c.set_wis((uint8_t)wis_->value());

    c.set_hp_cur((uint16_t)hp_cur_->value());
    c.set_hp_max((uint16_t)hp_max_->value());
    c.set_mp_cur((uint8_t)mp_cur_->value());
    c.set_mp_max((uint8_t)mp_max_->value());

    c.set_experience(exp_->value());
    c.set_gold((uint32_t)gold_->value());
    c.set_deposit((uint32_t)deposit_->value());
    c.set_rations((uint8_t)rations_->value());

    for (size_t i = 0; i < skills_.size(); ++i)
        c.set_skill(i, (uint8_t)skills_[i]->value());
    for (size_t i = 0; i < languages_.size(); ++i)
        c.set_language(i, languages_[i]->isChecked());

    for (size_t s = 0; s < char_off::kInventorySlots; ++s) {
        InvSlot slot;
        auto* combo = qobject_cast<QComboBox*>(inventory_->cellWidget((int)s, 1));
        slot.item = (uint8_t)combo->currentData().toInt();
        slot.equipped =
            inventory_->item((int)s, 2)->checkState() == Qt::Checked ? 1 : 0;
        slot.condition =
            (uint8_t)qobject_cast<QSpinBox*>(inventory_->cellWidget((int)s, 3))
                ->value();
        slot.charges =
            (uint8_t)qobject_cast<QSpinBox*>(inventory_->cellWidget((int)s, 4))
                ->value();
        c.set_inventory(s, slot);
    }
    // The engine scans for the first empty slot, so the count is the leading
    // run of non-empty slots.
    uint8_t used = 0;
    for (size_t s = 0; s < char_off::kInventorySlots; ++s) {
        if (c.inventory(s).item == 0) break;
        ++used;
    }
    c.set_byte(char_off::kInvUsed, used);

    for (size_t s = 0; s < char_off::kSpellSlots; ++s) {
        auto* combo = qobject_cast<QComboBox*>(spells_->cellWidget((int)s, 1));
        uint8_t id = (uint8_t)combo->currentData().toInt();
        uint8_t casts =
            (uint8_t)qobject_cast<QSpinBox*>(spells_->cellWidget((int)s, 2))->value();
        c.raw()[char_off::kSpells + s * 2] = id;
        c.raw()[char_off::kSpells + s * 2 + 1] = casts;
    }
    uint8_t spell_count = 0;
    for (size_t s = 0; s < char_off::kSpellSlots; ++s) {
        if (c.spell(s).id == 0) break;
        ++spell_count;
    }
    c.set_spell_count(spell_count);
}

// ---------------------------------------------------------------------------
// CharacterEditorPanel
// ---------------------------------------------------------------------------

CharacterEditorPanel::CharacterEditorPanel(QWidget* parent) : ToolPanel(parent) {
    auto* root = new QVBoxLayout(this);

    header_ = new QLabel(this);
    header_->setWordWrap(true);
    root->addWidget(header_);

    auto* body = new QHBoxLayout();

    slots_ = new QListWidget(this);
    slots_->setMaximumWidth(220);
    connect(slots_, &QListWidget::currentRowChanged, this,
            &CharacterEditorPanel::on_slot_changed);
    body->addWidget(slots_);

    // The form lives in a scroll area so the tab works on small screens.
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    forms_ = new QStackedWidget(scroll);
    scroll->setWidget(forms_);
    body->addWidget(scroll, 1);

    root->addLayout(body, 1);

    note_ = new QLabel(this);
    note_->setWordWrap(true);
    note_->setText(tr(
        "Editing a field is safe: the integrity checksum is recomputed when you "
        "save, so the game's cheat protection will not flag the file."));
    root->addWidget(note_);

    auto* buttons = new QHBoxLayout();
    save_ = new QPushButton(tr("Save"), this);
    save_as_ = new QPushButton(tr("Save as..."), this);
    auto* reload = new QPushButton(tr("Reload"), this);
    buttons->addWidget(reload);
    buttons->addStretch(1);
    buttons->addWidget(save_as_);
    buttons->addWidget(save_);
    root->addLayout(buttons);

    connect(save_, &QPushButton::clicked, this, &CharacterEditorPanel::on_save);
    connect(save_as_, &QPushButton::clicked, this, &CharacterEditorPanel::on_save_as);
    connect(reload, &QPushButton::clicked, this, &CharacterEditorPanel::on_reload);

    update_header();
}

bool CharacterEditorPanel::load_file(const QString& path) {
    doc_ = CharacterDocument::load(path.toLocal8Bit().constData());
    path_ = path;
    dirty_ = false;
    if (!doc_.ok()) {
        rebuild_forms();
        update_header();
        return false;
    }
    rebuild_forms();
    update_header();
    return true;
}

void CharacterEditorPanel::rebuild_forms() {
    slots_->clear();
    while (forms_->count() > 0) {
        QWidget* w = forms_->widget(0);
        forms_->removeWidget(w);
        delete w;
    }
    slot_forms_.clear();

    const size_t count = doc_.character_count();
    for (size_t i = 0; i < count; ++i) {
        const Character* c = doc_.character(i);
        QString label = QString::fromStdString(doc_.slot_label(i));
        if (c) {
            QString nm = QString::fromLatin1(c->name().c_str());
            label += nm.isEmpty() ? tr("  (empty)") : QStringLiteral("  %1").arg(nm);
        }
        slots_->addItem(label);

        auto* form = new CharacterForm(this);
        if (c) form->load(*c);
        connect(form, &CharacterForm::changed, this, [this]() {
            if (!dirty_) {
                dirty_ = true;
                update_header();
            }
        });
        forms_->addWidget(form);
        slot_forms_.push_back(form);
    }
    if (count > 0) {
        slots_->setCurrentRow(0);
        forms_->setCurrentIndex(0);
    }
}

void CharacterEditorPanel::on_slot_changed(int index) {
    collect_edits();
    if (index >= 0 && index < (int)forms_->count()) forms_->setCurrentIndex(index);
}

void CharacterEditorPanel::collect_edits() {
    for (size_t i = 0; i < slot_forms_.size(); ++i) {
        Character* c = doc_.character(i);
        if (c) slot_forms_[i]->apply(*c);
    }
}

void CharacterEditorPanel::on_save() {
    collect_edits();
    if (!doc_.save()) {
        QMessageBox::warning(this, tr("Save failed"),
                             QString::fromStdString(doc_.error()));
        return;
    }
    dirty_ = false;
    rebuild_forms();
    update_header();
    QMessageBox::information(
        this, tr("Saved"),
        tr("Wrote %1.\nThe integrity checksums were recomputed, so the game's "
           "cheat protection will accept the file.")
            .arg(path_));
    emit file_changed(path_);
}

void CharacterEditorPanel::on_save_as() {
    collect_edits();
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save as"), path_, tr("ROST / save game (*);;All files (*)"));
    if (path.isEmpty()) return;
    if (!doc_.save(path.toLocal8Bit().constData())) {
        QMessageBox::warning(this, tr("Save failed"),
                             QString::fromStdString(doc_.error()));
        return;
    }
    path_ = path;
    dirty_ = false;
    rebuild_forms();
    update_header();
    QMessageBox::information(this, tr("Saved"),
                             tr("Wrote %1 with recomputed checksums.").arg(path_));
    emit file_changed(path_);
}

void CharacterEditorPanel::on_reload() {
    if (!path_.isEmpty()) load_file(path_);
}

void CharacterEditorPanel::update_header() {
    const bool loaded = doc_.ok();
    save_->setEnabled(loaded);
    save_as_->setEnabled(loaded);

    if (!loaded) {
        header_->setText(tr("<b>No file loaded.</b> Use File > Open in the menu "
                            "bar to open a ROST or a save game."));
        note_->setVisible(true);
        return;
    }
    const QString kind = doc_.kind() == DocKind::SaveGame ? tr("save game")
                                                          : tr("ROST roster");
    header_->setText(tr("<b>%1</b> - %2 (%3 character(s)).%4")
                         .arg(path_, kind)
                         .arg(doc_.character_count())
                         .arg(dirty_ ? tr("  Unsaved changes.") : QString()));
    note_->setVisible(true);
}

}  // namespace loftools
