#include "gui/main_window.h"

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QWidget>

#include "core/sanitizer.h"
#include "gui/sanitizer_dialog.h"

namespace loftools {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(tr("Legend of Faerghail - Tools"));
    resize(760, 520);

    // --- File menu --------------------------------------------------------
    QMenu* file = menuBar()->addMenu(tr("&File"));
    QAction* open = file->addAction(tr("&Open save / ROST..."));
    open->setShortcut(QKeySequence::Open);
    connect(open, &QAction::triggered, this, &MainWindow::open_file);
    file->addSeparator();
    QAction* quit = file->addAction(tr("&Quit"));
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, qApp, &QApplication::quit);

    // --- Tools menu -------------------------------------------------------
    // The sanitizer is the first tool.  Save-game and character editors are
    // planned here; they are disabled placeholders until implemented.
    QMenu* tools = menuBar()->addMenu(tr("&Tools"));
    QAction* sanitize = tools->addAction(tr("&Sanitizer (cheat protection)..."));
    connect(sanitize, &QAction::triggered, this, &MainWindow::open_sanitizer);
    tools->addSeparator();
    QAction* save_editor = tools->addAction(tr("Save game editor..."));
    save_editor->setEnabled(false);
    save_editor->setToolTip(tr("Planned"));
    QAction* char_editor = tools->addAction(tr("Character editor..."));
    char_editor->setEnabled(false);
    char_editor->setToolTip(tr("Planned"));

    // --- Help menu --------------------------------------------------------
    QMenu* help = menuBar()->addMenu(tr("&Help"));
    QAction* about_action = help->addAction(tr("&About"));
    connect(about_action, &QAction::triggered, this, &MainWindow::about);

    // --- Central hint -----------------------------------------------------
    hint_ = new QLabel(this);
    hint_->setAlignment(Qt::AlignCenter);
    hint_->setWordWrap(true);
    hint_->setText(tr(
        "Legend of Faerghail tools.\n\n"
        "Use File > Open to load a save game or a ROST roster, or\n"
        "Tools > Sanitizer to check and fix the game's cheat protection."));
    setCentralWidget(hint_);

    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::open_file() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open save game or ROST"), last_dir_,
        tr("Save / ROST (*.SAV GAME* ROST*);;All files (*)"));
    if (path.isEmpty()) return;
    open_path(path);
}

void MainWindow::open_path(const QString& path) {
    last_dir_ = QFileInfo(path).absolutePath();
    if (!sanitizer_) sanitizer_ = new SanitizerDialog(this);
    sanitizer_->load_and_show(path);
    sanitizer_->show();
    sanitizer_->raise();
    sanitizer_->activateWindow();
}

void MainWindow::open_sanitizer() {
    if (!sanitizer_) sanitizer_ = new SanitizerDialog(this);
    sanitizer_->show();
    sanitizer_->raise();
    sanitizer_->activateWindow();
}

void MainWindow::about() {
    QMessageBox::about(
        this, tr("About Legend of Faerghail Tools"),
        tr("<h3>Legend of Faerghail Tools</h3>"
           "<p>Utilities for the 1990 DOS RPG <i>Legend of Faerghail</i>.</p>"
           "<p>The sanitizer checks and repairs the game's cheat protection "
           "in save games and ROST rosters.</p>"
           "<p>This is an independent, unofficial tool. It is not affiliated "
           "with the original authors or publishers.</p>"));
}

}  // namespace loftools
