#include "gui/main_window.h"

#include <algorithm>

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "gui/character_editor_panel.h"
#include "gui/sanitizer_panel.h"

namespace loftools {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(tr("Legend of Faerghail - Tools"));
    // Fit comfortably on the screen, but never larger than it; the window stays
    // freely resizable and the tool tabs scroll.
    QSize target(820, 620);
    if (QScreen* screen = QApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        target.setWidth(std::min(target.width(), avail.width() - 80));
        target.setHeight(std::min(target.height(), avail.height() - 80));
    }
    resize(target.expandedTo(QSize(480, 360)));

    // --- File menu --------------------------------------------------------
    QMenu* file = menuBar()->addMenu(tr("&File"));
    QAction* open = file->addAction(tr("&Open save / ROST..."));
    open->setShortcut(QKeySequence::Open);
    connect(open, &QAction::triggered, this, &MainWindow::open_file);
    QAction* reload = file->addAction(tr("&Reload"));
    reload->setShortcut(QKeySequence::Refresh);
    connect(reload, &QAction::triggered, this, &MainWindow::reload_file);
    QAction* close = file->addAction(tr("&Close file"));
    connect(close, &QAction::triggered, this, &MainWindow::close_file);
    file->addSeparator();
    QAction* quit = file->addAction(tr("&Quit"));
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, qApp, &QApplication::quit);

    // --- Tools menu -------------------------------------------------------
    // Each tool is a tab; these entries switch to it.
    QMenu* tools = menuBar()->addMenu(tr("&Tools"));
    QAction* sanitize = tools->addAction(tr("&Sanitizer (cheat protection)"));
    connect(sanitize, &QAction::triggered, this, &MainWindow::show_sanitizer);
    QAction* char_editor = tools->addAction(tr("&Character editor"));
    connect(char_editor, &QAction::triggered, this, &MainWindow::show_character_editor);
    tools->addSeparator();
    QAction* save_editor = tools->addAction(tr("Save game editor"));
    save_editor->setEnabled(false);
    save_editor->setToolTip(tr("Planned"));

    // --- Help menu --------------------------------------------------------
    QMenu* help = menuBar()->addMenu(tr("&Help"));
    QAction* about_action = help->addAction(tr("&About"));
    connect(about_action, &QAction::triggered, this, &MainWindow::about);

    // --- Central area: welcome page or the tool tabs ----------------------
    central_ = new QStackedWidget(this);
    setCentralWidget(central_);

    welcome_ = new QWidget(this);
    auto* welcome_layout = new QVBoxLayout(welcome_);
    welcome_layout->addStretch(1);
    auto* welcome_text = new QLabel(welcome_);
    welcome_text->setAlignment(Qt::AlignCenter);
    welcome_text->setWordWrap(true);
    welcome_text->setText(tr(
        "<h3>Legend of Faerghail tools</h3>"
        "<p>Open a save game or a ROST roster to get started.</p>"));
    welcome_layout->addWidget(welcome_text);
    auto* open_button = new QPushButton(tr("Open save / ROST..."), welcome_);
    open_button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(open_button, &QPushButton::clicked, this, &MainWindow::open_file);
    welcome_layout->addWidget(open_button, 0, Qt::AlignHCenter);
    welcome_layout->addStretch(2);
    central_->addWidget(welcome_);

    tabs_ = new QTabWidget(this);
    tabs_->setDocumentMode(true);
    central_->addWidget(tabs_);
    build_tools();

    statusBar()->showMessage(tr("Ready - no file open"));
}

void MainWindow::build_tools() {
    sanitizer_ = new SanitizerPanel(this);
    char_editor_ = new CharacterEditorPanel(this);

    tabs_->addTab(sanitizer_, sanitizer_->tool_name());
    tabs_->addTab(char_editor_, char_editor_->tool_name());

    connect(sanitizer_, &ToolPanel::file_changed, this, [this](const QString& p) {
        if (p == path_) sanitizer_->load_file(path_);  // keep the view in sync
    });
    connect(char_editor_, &ToolPanel::file_changed, this, [this](const QString& p) {
        if (p == path_) sanitizer_->load_file(path_);
    });
}

void MainWindow::open_file() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open save game or ROST"), last_dir_,
        tr("Save / ROST (*.SAV GAME* ROST*);;All files (*)"));
    if (path.isEmpty()) return;
    open_path(path);
}

void MainWindow::open_path(const QString& path) {
    path_ = path;
    last_dir_ = QFileInfo(path).absolutePath();
    const bool ok = sanitizer_->load_file(path);
    char_editor_->load_file(path);
    show_tools();
    statusBar()->showMessage(
        ok ? tr("Opened %1").arg(path)
           : tr("Could not read %1 as a save game or ROST").arg(path));
}

void MainWindow::reload_file() {
    if (!path_.isEmpty()) open_path(path_);
}

void MainWindow::close_file() {
    path_.clear();
    central_->setCurrentWidget(welcome_);
    setWindowTitle(tr("Legend of Faerghail - Tools"));
    statusBar()->showMessage(tr("Ready - no file open"));
}

void MainWindow::show_tools() {
    central_->setCurrentWidget(tabs_);
    setWindowTitle(tr("%1 - Legend of Faerghail Tools")
                       .arg(QFileInfo(path_).fileName()));
}

void MainWindow::show_sanitizer() {
    if (path_.isEmpty()) {
        open_file();
        if (path_.isEmpty()) return;
    }
    show_tools();
    tabs_->setCurrentWidget(sanitizer_);
}

void MainWindow::show_character_editor() {
    if (path_.isEmpty()) {
        open_file();
        if (path_.isEmpty()) return;
    }
    show_tools();
    tabs_->setCurrentWidget(char_editor_);
}

void MainWindow::open_character_editor_path(const QString& path) {
    if (!path.isEmpty()) open_path(path);
    show_tools();
    tabs_->setCurrentWidget(char_editor_);
}

void MainWindow::about() {
    QMessageBox::about(
        this, tr("About Legend of Faerghail Tools"),
        tr("<h3>Legend of Faerghail Tools</h3>"
           "<p>Utilities for the 1990 DOS RPG <i>Legend of Faerghail</i>.</p>"
           "<p>Each tool is a tab. Opening a file loads it into every tool: the "
           "sanitizer checks and repairs the game's cheat protection, and the "
           "character editor edits the characters.</p>"
           "<p>This is an independent, unofficial tool. It is not affiliated "
           "with the original authors or publishers.</p>"));
}

}  // namespace loftools
