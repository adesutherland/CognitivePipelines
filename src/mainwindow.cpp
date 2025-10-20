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
#include <QTextEdit>

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

    // Create Pipeline Output dock (read-only)
    pipelineOutputDock_ = new QDockWidget(tr("Pipeline Output"), this);
    pipelineOutputDock_->setObjectName("PipelineOutputDock");
    pipelineOutputDock_->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    pipelineOutputText_ = new QTextEdit(pipelineOutputDock_);
    pipelineOutputText_->setReadOnly(true);
    pipelineOutputDock_->setWidget(pipelineOutputText_);
    addDockWidget(Qt::BottomDockWidgetArea, pipelineOutputDock_);

    // Create Debug Log dock (read-only, hidden by default)
    debugLogDock_ = new QDockWidget(tr("Debug Log"), this);
    debugLogDock_->setObjectName("DebugLogDock");
    debugLogDock_->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    debugLogText_ = new QTextEdit(debugLogDock_);
    debugLogText_->setReadOnly(true);
    debugLogDock_->setWidget(debugLogText_);
    addDockWidget(Qt::BottomDockWidgetArea, debugLogDock_);
    debugLogDock_->hide();

    // Wire action to dock visibility and keep them in sync
    connect(showDebugLogAction_, &QAction::toggled, debugLogDock_, &QDockWidget::setVisible);
    connect(debugLogDock_, &QDockWidget::visibilityChanged, showDebugLogAction_, &QAction::setChecked);

    // Connect engine signals to UI slots
    connect(execEngine_, &ExecutionEngine::pipelineFinished,
            this, &MainWindow::onPipelineFinished);
    connect(execEngine_, &ExecutionEngine::nodeLog,
            this, &MainWindow::onNodeLog);

    // Connect selection signals
    connect(scene, &QtNodes::BasicGraphicsScene::nodeSelected,
            this, &MainWindow::onNodeSelected);
    connect(scene, &QGraphicsScene::selectionChanged,
            this, &MainWindow::onSelectionChanged);
}

void MainWindow::createActions() {

    exitAction = new QAction(tr("E&xit"), this);
    exitAction->setShortcuts(QKeySequence::Quit);
    exitAction->setStatusTip(tr("Exit the application"));

    aboutAction = new QAction(tr("&About..."), this);
    aboutAction->setStatusTip(tr("About this application"));

    // Run action (moved from toolbar to the Pipeline menu)
    runAction_ = new QAction(tr("&Run"), this);
    runAction_->setStatusTip(tr("Execute the current pipeline"));
    // Keep the same behavior: trigger the execution engine
    connect(runAction_, &QAction::triggered, execEngine_, &ExecutionEngine::run);

    // View menu action to toggle debug log dock
    showDebugLogAction_ = new QAction(tr("Show Debug Log"), this);
    showDebugLogAction_->setCheckable(true);
    showDebugLogAction_->setChecked(false);

    // Pipeline menu action to enable/disable debug logging
    enableDebugLoggingAction_ = new QAction(tr("Enable Debug Logging"), this);
    enableDebugLoggingAction_->setCheckable(true);
    enableDebugLoggingAction_->setChecked(false);

    // Modern signal-slot connections using function pointers / lambdas

    connect(exitAction, &QAction::triggered, this, [this]() {
        // Close the main window (equivalent to quitting when it's the main window)
        close();
    });

    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);

}

void MainWindow::createMenus() {
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);

    // New View menu containing toggle for Debug Log
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(showDebugLogAction_);

    // Pipeline menu containing the Run action and Enable Debug Logging toggle
    QMenu* pipelineMenu = menuBar()->addMenu(tr("&Pipeline"));
    pipelineMenu->addAction(runAction_);
    pipelineMenu->addSeparator();
    pipelineMenu->addAction(enableDebugLoggingAction_);

    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAction);
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

void MainWindow::onPipelineFinished(const DataPacket& finalOutput)
{
    QString text;
    if (finalOutput.contains(QStringLiteral("text"))) {
        text = finalOutput.value(QStringLiteral("text")).toString();
    } else if (!finalOutput.isEmpty()) {
        QStringList lines;
        for (auto it = finalOutput.cbegin(); it != finalOutput.cend(); ++it) {
            lines << QStringLiteral("%1: %2").arg(it.key(), it.value().toString());
        }
        text = lines.join('\n');
    } else {
        text = tr("<no output>");
    }
    if (pipelineOutputText_) {
        pipelineOutputText_->setPlainText(text);
    }
    if (pipelineOutputDock_ && !pipelineOutputDock_->isVisible()) {
        pipelineOutputDock_->show();
    }
}

void MainWindow::onNodeLog(const QString& message)
{
    if (!enableDebugLoggingAction_ || !enableDebugLoggingAction_->isChecked()) {
        return;
    }
    if (debugLogText_) {
        debugLogText_->append(message);
    }
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
