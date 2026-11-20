#include "gui/sanitizer_panel.h"

#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace loftools {

SanitizerPanel::SanitizerPanel(QWidget* parent) : ToolPanel(parent) {
    auto* root = new QVBoxLayout(this);

    verdict_label_ = new QLabel(this);
    QFont vf = verdict_label_->font();
    vf.setBold(true);
    verdict_label_->setFont(vf);
    verdict_label_->setWordWrap(true);
    root->addWidget(verdict_label_);

    table_ = new QTableWidget(this);
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels(
        {tr("Slot"), tr("Character"), tr("Problem"), tr("Fixed")});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionMode(QAbstractItemView::NoSelection);
    root->addWidget(table_, 1);

    // Max HP to restore for an unrevivable character.  A failed temple revive
    // zeroes max HP and the value is not stored anywhere in the record, so it
    // has to come from the user (or from a backup).  Hidden when no character
    // needs it.
    hp_row_ = new QWidget(this);
    auto* hp_layout = new QHBoxLayout(hp_row_);
    hp_layout->setContentsMargins(0, 0, 0, 0);
    hp_layout->addWidget(new QLabel(tr("Max HP to restore for the dead "
                                       "character:"), hp_row_));
    hp_max_ = new QSpinBox(hp_row_);
    hp_max_->setRange(1, 32767);
    hp_max_->setValue(1);
    hp_max_->setToolTip(tr("The character's maximum hit points.  A failed temple "
                           "revive sets max HP to 0 and the value is not stored "
                           "anywhere in the file, so it has to be supplied here "
                           "(use a backup if you have one).  Minimum is 1."));
    hp_layout->addWidget(hp_max_);
    hp_layout->addStretch(1);
    root->addWidget(hp_row_);
    hp_row_->setVisible(false);

    auto* buttons = new QHBoxLayout();
    recheck_button_ = new QPushButton(tr("Re-check"), this);
    fix_button_ = new QPushButton(tr("Fix && Save"), this);
    fix_button_->setDefault(true);
    buttons->addWidget(recheck_button_);
    buttons->addStretch(1);
    buttons->addWidget(fix_button_);
    root->addLayout(buttons);

    connect(recheck_button_, &QPushButton::clicked, this, &SanitizerPanel::recheck);
    connect(fix_button_, &QPushButton::clicked, this, &SanitizerPanel::fix);

    update_buttons();
}

bool SanitizerPanel::load_file(const QString& path) {
    path_ = path;
    set_report(inspect(path.toStdString()));
    return report_.file_ok;
}

void SanitizerPanel::recheck() {
    if (path_.isEmpty()) return;
    set_report(inspect(path_.toStdString()));
}

void SanitizerPanel::set_report(const SanitizeReport& rep) {
    report_ = rep;

    const char* kind = "unknown";
    switch (rep.kind) {
        case FileKind::Roster:   kind = "ROST roster"; break;
        case FileKind::SaveGame: kind = "save game"; break;
        default: break;
    }

    if (!rep.file_ok) {
        verdict_label_->setStyleSheet("color: #b00020;");
        verdict_label_->setText(tr("Cannot read as a %1: %2")
                                    .arg(kind)
                                    .arg(QString::fromStdString(rep.error)));
    } else if (rep.flagged && rep.has_unrevivable) {
        verdict_label_->setStyleSheet("color: #b00020;");
        verdict_label_->setText(tr("Format: %1 - the cheat protection WOULD flag "
                                   "this file, and it also has %2 character "
                                   "problem(s).")
                                    .arg(kind)
                                    .arg(rep.findings.size()));
    } else if (rep.flagged) {
        verdict_label_->setStyleSheet("color: #b00020;");
        verdict_label_->setText(tr("Format: %1 - the cheat protection WOULD flag "
                                   "this file (%2 problem(s)).")
                                    .arg(kind)
                                    .arg(rep.findings.size()));
    } else if (rep.has_unrevivable) {
        verdict_label_->setStyleSheet("color: #b35c00;");
        verdict_label_->setText(
            tr("Format: %1 - passes the cheat protection. %2 dead character(s) "
               "the temple will no longer revive (max HP 0).")
                .arg(kind)
                .arg(rep.findings.size()));
    } else {
        verdict_label_->setStyleSheet("color: #1b7f3b;");
        verdict_label_->setText(
            tr("Format: %1 - clean. The cheat protection would not flag this file.")
                .arg(kind));
    }

    table_->setRowCount((int)rep.findings.size());
    for (int i = 0; i < (int)rep.findings.size(); ++i) {
        const Finding& f = rep.findings[i];
        const QString slot = f.slot >= 0 ? QString::number(f.slot) : tr("(whole file)");
        table_->setItem(i, 0, new QTableWidgetItem(slot));
        table_->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(f.character)));
        const QString tag = f.informational ? tr("[info] ")
                            : f.revive_blocked ? tr("[dead] ")
                                               : QString();
        const QString detail = tag + QString::fromStdString(f.detail);
        table_->setItem(i, 2, new QTableWidgetItem(detail));
        table_->setItem(i, 3, new QTableWidgetItem(f.fixed ? tr("yes") : tr("")));
    }

    update_buttons();
}

void SanitizerPanel::update_buttons() {
    const bool loaded = report_.file_ok;
    const bool can_fix =
        loaded && (report_.flagged || report_.has_unrevivable) && !report_.fixed;
    recheck_button_->setEnabled(loaded);
    fix_button_->setEnabled(can_fix);
    fix_button_->setToolTip(can_fix ? tr("Write the repaired file back to disk")
                                    : tr("Nothing to fix"));
    hp_row_->setVisible(loaded && report_.has_unrevivable && !report_.fixed);
}

void SanitizerPanel::fix() {
    if (path_.isEmpty()) return;
    SanitizeReport rep = sanitize(path_.toStdString(), "",
                                  (uint16_t)hp_max_->value());
    set_report(rep);
    if (rep.fixed) {
        QMessageBox::information(
            this, tr("Repaired"),
            tr("%1\n\nThe repaired file has been written back to disk.")
                .arg(QString::fromStdString(rep.summary)));
        emit file_changed(path_);
    } else if (!rep.file_ok) {
        QMessageBox::warning(this, tr("Could not repair"),
                             QString::fromStdString(rep.summary));
    } else {
        QMessageBox::information(this, tr("Nothing to do"),
                                 QString::fromStdString(rep.summary));
    }
}

}  // namespace loftools
