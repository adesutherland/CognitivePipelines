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
#include "NodeGraphModel.h"
#include "ToolNodeDelegate.h"
#include "ExecutionEngine.h"

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
#include <QtNodes/GraphicsView>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/Definitions>
#include <QtNodes/internal/BasicGraphicsScene.hpp>
#include <QVBoxLayout>
#include <QDockWidget>
#include <QLabel>
#include <QGraphicsView>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("CognitivePipelines");
    resize(1100, 700);

    // Instantiate the graph model and view, and set as central widget
    _graphModel = new NodeGraphModel(this);
    auto* scene = new QtNodes::DataFlowGraphicsScene(*_graphModel, this);
    _graphView = new QtNodes::GraphicsView(scene, this);
    setCentralWidget(_graphView);

    // Create execution engine
    execEngine_ = new ExecutionEngine(_graphModel, this);

    createActions();
    createMenus();
    createToolBar();
    createStatusBar();

    // Create Properties dock on the right
    propertiesDock_ = new QDockWidget(tr("Properties"), this);
    propertiesDock_->setObjectName("PropertiesDock");
    propertiesDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    propertiesHost_ = new QWidget(propertiesDock_);
    propertiesLayout_ = new QVBoxLayout(propertiesHost_);
    propertiesLayout_->setContentsMargins(6, 6, 6, 6);
    propertiesLayout_->setSpacing(6);
    placeholderLabel_ = new QLabel(tr("No node selected"), propertiesHost_);
    placeholderLabel_->setAlignment(Qt::AlignCenter);
    propertiesLayout_->addWidget(placeholderLabel_);
    propertiesDock_->setWidget(propertiesHost_);
    addDockWidget(Qt::RightDockWidgetArea, propertiesDock_);

    // Connect selection signals
    connect(scene, &QtNodes::BasicGraphicsScene::nodeSelected,
            this, &MainWindow::onNodeSelected);
    connect(scene, &QGraphicsScene::selectionChanged,
            this, &MainWindow::onSelectionChanged);
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

    runAction_ = tb->addAction(tr("Run"));
    connect(runAction_, &QAction::triggered, execEngine_, &ExecutionEngine::run);
}

void MainWindow::createStatusBar() {
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::setPropertiesWidget(QWidget* w)
{
    // Remove old widget from layout if any, but do not delete to avoid races with QPointer holders
    if (currentConfigWidget_) {
        propertiesLayout_->removeWidget(currentConfigWidget_);
        currentConfigWidget_->hide();
        // keep parent as propertiesHost_ to ensure proper lifetime management
    }

    if (!w) {
        if (placeholderLabel_) placeholderLabel_->setVisible(true);
        currentConfigWidget_.clear();
        return;
    }

    // Add/show new widget
    if (placeholderLabel_) placeholderLabel_->setVisible(false);

    currentConfigWidget_ = w;
    if (currentConfigWidget_ && currentConfigWidget_->parent() != propertiesHost_) {
        currentConfigWidget_->setParent(propertiesHost_);
    }
    if (currentConfigWidget_ && propertiesLayout_->indexOf(currentConfigWidget_) == -1) {
        propertiesLayout_->addWidget(currentConfigWidget_);
    }
    if (currentConfigWidget_) currentConfigWidget_->show();
}

void MainWindow::onNodeSelected(QtNodes::NodeId nodeId)
{
    if (!_graphModel) { setPropertiesWidget(nullptr); return; }

    // Fetch our ToolNodeDelegate for this nodeId
    auto* delegate = _graphModel->delegateModel<ToolNodeDelegate>(nodeId);
    if (!delegate) {
        setPropertiesWidget(nullptr);
        return;
    }

    // Request the configuration widget from ToolNodeDelegate (not embedded in node)
    QWidget* cfg = delegate->configurationWidget();
    setPropertiesWidget(cfg);
}

void MainWindow::onSelectionChanged()
{
    auto* scene = qobject_cast<QtNodes::DataFlowGraphicsScene*>(_graphView ? _graphView->scene() : nullptr);
    if (!scene) { setPropertiesWidget(nullptr); return; }

    const auto sel = scene->selectedNodes();
    if (sel.empty()) {
        setPropertiesWidget(nullptr);
        return;
    }

    // For now, take the first selected node
    onNodeSelected(*sel.begin());
}

void MainWindow::onAbout() {
    AboutDialog dlg(this);
    dlg.exec();
}


void MainWindow::onInteractivePrompt() {
    PromptDialog dlg(this);
    dlg.exec();
}


MainWindow::~MainWindow()
{
    // Ensure properties panel does not hold onto any widget
    setPropertiesWidget(nullptr);

    // Proactively tear down the scene and its items while the graph model still exists.
    if (_graphView) {
        QGraphicsScene* scene = _graphView->scene();
        // Detach using base class to avoid QtNodes::GraphicsView assuming non-null scene
        static_cast<QGraphicsView*>(_graphView)->setScene(nullptr);
        if (scene) {
            // If this is a QtNodes scene, clear via its model-driven API to avoid double-deletion
            if (auto* bscene = dynamic_cast<QtNodes::BasicGraphicsScene*>(scene)) {
                bscene->clearScene();
            } else {
                // Fallback for plain QGraphicsScene
                scene->clear();
            }
            delete scene;
        }
    }
}
