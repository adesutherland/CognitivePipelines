//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "mainwindow.h"
#include "about_dialog.h"
#include "PromptDialog.h"

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QWidget>
#include <QKeySequence>
#include <QPushButton>
#include <QMessageBox>
#include <QByteArray>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("CognitivePipelines");
    resize(900, 600);

    createActions();
    createMenus();
    createToolBar();
    createStatusBar();
    createCentralPlaceholder();
}

void MainWindow::createActions() {
    openAction = new QAction(tr("&Open..."), this);
    openAction->setShortcuts(QKeySequence::Open);
    openAction->setStatusTip(tr("Open a file"));

    saveAction = new QAction(tr("&Save"), this);
    saveAction->setShortcuts(QKeySequence::Save);
    saveAction->setStatusTip(tr("Save current file"));

    exitAction = new QAction(tr("E&xit"), this);
    exitAction->setShortcuts(QKeySequence::Quit);
    exitAction->setStatusTip(tr("Exit the application"));

    aboutAction = new QAction(tr("&About..."), this);
    aboutAction->setStatusTip(tr("About this application"));

    // Modern signal-slot connections using function pointers / lambdas
    connect(openAction, &QAction::triggered, this, [this]() {
        const QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"));
        if (!fileName.isEmpty()) {
            statusBar()->showMessage(tr("Opened: %1").arg(fileName), 3000);
        }
    });

    connect(saveAction, &QAction::triggered, this, [this]() {
        const QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"));
        if (!fileName.isEmpty()) {
            statusBar()->showMessage(tr("Saved: %1").arg(fileName), 3000);
        }
    });

    connect(exitAction, &QAction::triggered, this, [this]() {
        // Close the main window (equivalent to quitting when it's the main window)
        close();
    });

    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);

    // Tools -> Interactive Prompt action
    interactivePromptAction_ = new QAction(tr("Interactive Prompt..."), this);
    interactivePromptAction_->setStatusTip(tr("Open an interactive prompt dialog"));
    connect(interactivePromptAction_, &QAction::triggered, this, &MainWindow::onInteractivePrompt);
}

void MainWindow::createMenus() {
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(openAction);
    fileMenu->addAction(saveAction);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);

    QMenu* toolsMenu = menuBar()->addMenu(tr("&Tools"));
    toolsMenu->addAction(interactivePromptAction_);

    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAction);
}

void MainWindow::createToolBar() {
    QToolBar* tb = addToolBar(tr("Main Toolbar"));
    tb->setObjectName("MainToolbar");
    tb->setMovable(true);
    tb->addAction(openAction);
    tb->addAction(saveAction);

    // Add a QPushButton labeled "Run" to trigger the LLM call
    runButton_ = new QPushButton(tr("Run"), tb);
    tb->addWidget(runButton_);
    connect(runButton_, &QPushButton::clicked, this, &MainWindow::onRunButtonClicked);
}

void MainWindow::createStatusBar() {
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::createCentralPlaceholder() {
    // A simple placeholder widget for now
    QWidget* placeholder = new QWidget(this);
    setCentralWidget(placeholder);
}

void MainWindow::onAbout() {
    AboutDialog dlg(this);
    dlg.exec();
}

void MainWindow::onRunButtonClicked() {
    // Read API key from environment for security
    const QByteArray apiKeyBA = qgetenv("OPENAI_API_KEY");
    if (apiKeyBA.isEmpty()) {
        QMessageBox::warning(this, tr("Missing API Key"), tr("OPENAI_API_KEY environment variable is not set. Please configure it and try again."));
        return;
    }

    const std::string apiKey = apiKeyBA.toStdString();
    const std::string prompt = "Tell me a short joke about programming.";

    // Call the LLM client synchronously (simple initial integration)
    const std::string response = llmClient_.sendPrompt(apiKey, prompt);

    // Show the response to the user
    QMessageBox::information(this, tr("LLM Response"), QString::fromStdString(response));
}

void MainWindow::onInteractivePrompt() {
    PromptDialog dlg(this);
    dlg.exec();
}
