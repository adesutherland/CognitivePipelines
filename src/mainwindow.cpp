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
#include "ExecutionAwarePainters.h"
#include "ExecutionStateModel.h"

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
#include <QtNodes/internal/ConnectionGraphicsObject.hpp>
#include <QVBoxLayout>
#include <QDockWidget>
#include <QLabel>
#include <QGraphicsView>
#include <QTextEdit>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QSaveFile>
#include "ExecutionIdUtils.h"

#include "LLMConnector.h"
#include "CredentialsEditorDialog.h"
#include "UserInputDialog.h"

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

    // Live execution-state highlighting: install custom painters and wire signals
    execStateModel_ = std::make_shared<ExecutionStateModel>(this);
    scene->setNodePainter(std::unique_ptr<QtNodes::AbstractNodePainter>(
        new ExecutionAwareNodePainter(execStateModel_)));
    scene->setConnectionPainter(std::unique_ptr<QtNodes::AbstractConnectionPainter>(
        new ExecutionAwareConnectionPainter(execStateModel_)));

    // Forward engine status updates to the state model
    connect(execEngine_, &ExecutionEngine::nodeStatusChanged,
            execStateModel_.get(), &ExecutionStateModel::onNodeStatusChanged);
    connect(execEngine_, &ExecutionEngine::connectionStatusChanged,
            execStateModel_.get(), &ExecutionStateModel::onConnectionStatusChanged);

    // Repaint the scene when any state changes
    connect(execStateModel_.get(), &ExecutionStateModel::stateChanged, scene, [scene]() { scene->update(); });

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

    // Repaint specific node on status changes to force painter invocation
    connect(execEngine_, &ExecutionEngine::nodeStatusChanged,
            this, [this](const QUuid &nodeId, int /*state*/) { onNodeRepaint(nodeId); });

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

    // File actions
    openAction_ = new QAction(tr("&Open..."), this);
    openAction_->setShortcuts(QKeySequence::Open);
    openAction_->setStatusTip(tr("Open a pipeline from a file"));
    connect(openAction_, &QAction::triggered, this, &MainWindow::onOpen);

    saveAsAction_ = new QAction(tr("Save &As..."), this);
    saveAsAction_->setShortcuts(QKeySequence::SaveAs);
    saveAsAction_->setStatusTip(tr("Save the current pipeline to a file"));
    connect(saveAsAction_, &QAction::triggered, this, &MainWindow::onSaveAs);

    // Clear Canvas action
    clearCanvasAction_ = new QAction(tr("Clear Canvas"), this);
    clearCanvasAction_->setStatusTip(tr("Clear all nodes and connections from the canvas"));
    connect(clearCanvasAction_, &QAction::triggered, this, &MainWindow::onClearCanvas);

    // Credentials editor
    editCredentialsAction_ = new QAction(tr("Edit Credentials..."), this);
    editCredentialsAction_->setStatusTip(tr("Open or create accounts.json in the standard app data location"));
    connect(editCredentialsAction_, &QAction::triggered, this, &MainWindow::onEditCredentials);

    // Delete action
    deleteAction = new QAction(tr("Delete"), this);
    deleteAction->setShortcuts({QKeySequence::Delete, Qt::Key_Backspace});
    deleteAction->setStatusTip(tr("Delete selected nodes and connections"));
    connect(deleteAction, &QAction::triggered, this, &MainWindow::onDeleteSelected);

    // Run action (moved from toolbar to the Pipeline menu)
    runAction_ = new QAction(tr("&Run"), this);
    runAction_->setStatusTip(tr("Execute the current pipeline"));
    // Keep the same behavior: trigger the execution engine
    connect(runAction_, &QAction::triggered, execEngine_, &ExecutionEngine::run);

    // Save last output action (Pipeline menu)
    saveOutputAction_ = new QAction(tr("Save Last Output..."), this);
    saveOutputAction_->setStatusTip(tr("Save the text content from the Pipeline Output dock to a file"));
    connect(saveOutputAction_, &QAction::triggered, this, &MainWindow::onSaveOutput);

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
    // File menu
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(openAction_);
    fileMenu->addAction(saveAsAction_);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAction);

    // Edit menu
    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(editCredentialsAction_);
    editMenu->addSeparator();
    editMenu->addAction(deleteAction);
    editMenu->addAction(clearCanvasAction_);

    // View menu
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(showDebugLogAction_);

    // Pipeline menu
    QMenu* pipelineMenu = menuBar()->addMenu(tr("&Pipeline"));
    pipelineMenu->addAction(runAction_);
    pipelineMenu->addSeparator();
    pipelineMenu->addAction(saveOutputAction_);
    pipelineMenu->addAction(enableDebugLoggingAction_);

    // Help menu
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

void MainWindow::onSaveAs()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("Save Pipeline As"),
                                                    QDir::homePath(),
                                                    tr("Flow Scene Files (*.flow);;JSON Files (*.json);;All Files (*)"));
    if (fileName.isEmpty()) return;

    if (!fileName.endsWith(".flow", Qt::CaseInsensitive) &&
        !fileName.endsWith(".json", Qt::CaseInsensitive)) {
        fileName += ".flow";
    }

    if (!_graphModel) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Save Failed"),
                             tr("Could not open file for writing:\n%1").arg(file.errorString()));
        return;
    }

    const QJsonObject json = _graphModel->save();
    file.write(QJsonDocument(json).toJson());
    file.close();

    statusBar()->showMessage(tr("Saved to %1").arg(QFileInfo(fileName).fileName()), 3000);
}

void MainWindow::onEditCredentials()
{
    const QString filePath = LLMConnector::defaultAccountsFilePath();
    if (filePath.isEmpty()) {
        QMessageBox::critical(this, tr("Error"), tr("Could not find or create accounts.json file."));
        return;
    }

    CredentialsEditorDialog dialog(filePath, this);
    dialog.exec();
}

void MainWindow::onOpen()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Open Pipeline"),
                                                    QDir::homePath(),
                                                    tr("Flow Scene Files (*.flow);;JSON Files (*.json);;All Files (*)"));
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Open Failed"),
                             tr("Could not open file for reading:\n%1").arg(file.errorString()));
        return;
    }

    const QByteArray data = file.readAll();
    file.close();

    // Clear properties panel to avoid dangling widgets from nodes being deleted
    setPropertiesWidget(nullptr);

    // Clear existing graph before loading
    if (_graphView && _graphView->scene()) {
        if (auto* bscene = dynamic_cast<QtNodes::BasicGraphicsScene*>(_graphView->scene())) {
            bscene->clearScene();
        } else {
            _graphView->scene()->clear();
        }
    }

    QJsonParseError parseErr{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, tr("Open Failed"),
                             tr("Invalid JSON in file: %1").arg(parseErr.errorString()));
        return;
    }

    // Migrate legacy model names to current IDs and infer when missing
    QJsonObject migrated = doc.object();
    QJsonArray nodesArray = migrated.value(QStringLiteral("nodes")).toArray();
    for (int i = 0; i < nodesArray.size(); ++i) {
        QJsonObject nodeObj = nodesArray.at(i).toObject();
        QJsonObject internal = nodeObj.value(QStringLiteral("internal-data")).toObject();
        const QString modelName = internal.value(QStringLiteral("model-name")).toString();
        QString mapped = modelName;

        if (modelName.isEmpty()) {
            // Infer from known state keys when model-name is absent (older saves)
            if (internal.contains(QStringLiteral("text"))) {
                mapped = QStringLiteral("text-input");
            } else if (internal.contains(QStringLiteral("template"))) {
                mapped = QStringLiteral("prompt-builder");
            } else if (internal.contains(QStringLiteral("apiKey")) || internal.contains(QStringLiteral("prompt"))) {
                mapped = QStringLiteral("llm-connector");
            } else {
                // Fallback to a safe default to allow loading
                mapped = QStringLiteral("text-input");
            }
        } else {
            // Remap legacy human-readable names to stable IDs
            if (modelName == QStringLiteral("LLM Connector") || modelName == QStringLiteral("LLMConnector")) {
                mapped = QStringLiteral("llm-connector");
            } else if (modelName == QStringLiteral("Prompt Builder") || modelName == QStringLiteral("PromptBuilderNode")) {
                mapped = QStringLiteral("prompt-builder");
            } else if (modelName == QStringLiteral("Text Input") || modelName == QStringLiteral("TextInputNode")) {
                mapped = QStringLiteral("text-input");
            }
        }

        if ((mapped != modelName || modelName.isEmpty()) && !mapped.isEmpty()) {
            internal.insert(QStringLiteral("model-name"), mapped);
            nodeObj.insert(QStringLiteral("internal-data"), internal);
            nodesArray.replace(i, nodeObj);
        }
    }
    migrated.insert(QStringLiteral("nodes"), nodesArray);

    try {
        if (_graphModel) {
            _graphModel->load(migrated);
        }
    } catch (const std::exception& e) {
        QMessageBox::critical(this, tr("Open Failed"),
                              tr("An error occurred while loading the pipeline:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    } catch (...) {
        QMessageBox::critical(this, tr("Open Failed"),
                              tr("An unknown error occurred while loading the pipeline."));
        return;
    }

    statusBar()->showMessage(tr("Loaded from %1").arg(QFileInfo(fileName).fileName()), 3000);
}

void MainWindow::onClearCanvas()
{
    // Clear all nodes and connections from the graph model
    if (_graphModel) {
        _graphModel->clear();
    }

    // Reset the properties panel so it doesn't hold stale pointers
    setPropertiesWidget(nullptr);
}

void MainWindow::onDeleteSelected()
{
    // Get the scene
    auto* scene = qobject_cast<QtNodes::DataFlowGraphicsScene*>(_graphView ? _graphView->scene() : nullptr);
    if (!scene || !_graphModel) {
        return;
    }

    // Get selected connections and delete them first (to avoid dangling references)
    const auto selectedItems = scene->selectedItems();
    for (auto* item : selectedItems) {
        if (auto* connectionGraphics = dynamic_cast<QtNodes::ConnectionGraphicsObject*>(item)) {
            _graphModel->deleteConnection(connectionGraphics->connectionId());
        }
    }

    // Get selected nodes and delete them
    const auto selectedNodes = scene->selectedNodes();
    for (const auto& nodeId : selectedNodes) {
        _graphModel->deleteNode(nodeId);
    }

    // Clear the properties panel if any deleted node was being displayed
    if (!selectedNodes.empty()) {
        setPropertiesWidget(nullptr);
    }
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
        pipelineOutputText_->setMarkdown(text);
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

QString MainWindow::requestUserInput(const QString& prompt)
{
    UserInputDialog dialog(prompt, this);
    if (dialog.exec() == QDialog::Accepted) {
        return dialog.getText();
    }
    return QString(); // Return empty if canceled
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


void MainWindow::onNodeRepaint(const QUuid& nodeUuid)
{
    // Find the node by deterministic execution UUID and repaint its graphics object
    auto *qscene = _graphView ? _graphView->scene() : nullptr;
    auto *scene = qscene ? dynamic_cast<QtNodes::BasicGraphicsScene*>(qscene) : nullptr;
    if (!scene || !_graphModel)
        return;

    QtNodes::NodeId foundId = 0;
    bool found = false;
    for (auto nid : _graphModel->allNodeIds()) {
        if (ExecIds::nodeUuid(nid) == nodeUuid) {
            foundId = nid;
            found = true;
            break;
        }
    }

    if (found) {
        if (auto *ngo = scene->nodeGraphicsObject(foundId)) {
            // Trigger a repaint of this specific item so our custom painter runs
            ngo->update();
            return;
        }
    }

    // Fallback: if not found, update the whole scene (should be rare)
    scene->update();
}

void MainWindow::onSaveOutput()
{
    // Prompt user for destination file
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("Save Output As"),
                                                    QDir::homePath(),
                                                    tr("Text Files (*.txt);;All Files (*)"));
    if (fileName.isEmpty()) {
        // User canceled
        return;
    }

    if (!pipelineOutputText_) {
        return;
    }

    const QString text = pipelineOutputText_->toPlainText();

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Save Failed"),
                             tr("Could not open file for writing:\n%1").arg(file.errorString()));
        return;
    }

    const QByteArray bytes = text.toUtf8();
    if (file.write(bytes) != bytes.size()) {
        QMessageBox::warning(this, tr("Save Failed"),
                             tr("Could not write all data to file:\n%1").arg(file.errorString()));
        file.close();
        return;
    }

    file.close();
    statusBar()->showMessage(tr("Output saved to %1").arg(QFileInfo(fileName).fileName()), 3000);
}
