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

#include "MainWindow.h"
#include "AboutDialog.h"
#include "NodeGraphModel.h"
#include "ToolNodeDelegate.h"
#include "TextOutputNode.h"
#include "ExecutionEngine.h"
#include "ExecutionAwarePainters.h"
#include "ExecutionStateModel.h"

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QComboBox>
#include <QStatusBar>
#include <QFont>
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
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QWidgetAction>
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
#include <limits>
#include "ExecutionIdUtils.h"

#include "CredentialsDialog.h"
#include "ProviderManagementDialog.h"
#include "SyntaxHighlightingOptionsDialog.h"
#include "UserInputDialog.h"
#include "Logger.h"

static MainWindow* s_instance = nullptr;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    s_instance = this;
    setWindowTitle("CognitivePipelines");
    resize(1100, 700);

    // Instantiate the graph model, execution engine, and view.
    _graphModel = new NodeGraphModel(this);
    currentGraphModel_ = _graphModel;
    _graphView = new QtNodes::GraphicsView(this);
    setCentralWidget(_graphView);

    // Create execution engine
    execEngine_ = new ExecutionEngine(_graphModel, this);

    // Live execution-state highlighting: custom painters are installed per scene.
    execStateModel_ = std::make_shared<ExecutionStateModel>(this);
    connectGraphModelSignals(_graphModel);
    installGraphScene(_graphModel);

    // Forward engine status updates to the state model
    connect(execEngine_, &ExecutionEngine::nodeStatusChanged,
            execStateModel_.get(), &ExecutionStateModel::onNodeStatusChanged);
    connect(execEngine_, &ExecutionEngine::connectionStatusChanged,
            execStateModel_.get(), &ExecutionStateModel::onConnectionStatusChanged);

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
    
    // Add Node Description section at the top
    descriptionLabel_ = new QLabel(tr("Node Description"), propertiesHost_);
    propertiesLayout_->addWidget(descriptionLabel_);
    descriptionEdit_ = new QPlainTextEdit(propertiesHost_);
    descriptionEdit_->setMaximumHeight(60);
    descriptionEdit_->setEnabled(false);
    propertiesLayout_->addWidget(descriptionEdit_);
    descriptionLabel_->setVisible(false);
    descriptionEdit_->setVisible(false);

    placeholderLabel_ = new QLabel(tr("No node selected"), propertiesHost_);
    placeholderLabel_->setAlignment(Qt::AlignCenter);
    propertiesLayout_->addWidget(placeholderLabel_);
    propertiesDock_->setWidget(propertiesHost_);
    addDockWidget(Qt::RightDockWidgetArea, propertiesDock_);

    // Create Stage Output dock (read-only)
    stageOutputDock_ = new QDockWidget(tr("Stage Output"), this);
    stageOutputDock_->setObjectName("StageOutputDock");
    stageOutputDock_->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    stageOutputText_ = new QTextEdit(stageOutputDock_);
    stageOutputText_->setReadOnly(true);
    stageOutputDock_->setWidget(stageOutputText_);
    addDockWidget(Qt::BottomDockWidgetArea, stageOutputDock_);

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

    // Global running status indicator wiring
    connect(execEngine_, &ExecutionEngine::executionStarted, this, [this]() {
        if (runAction_) runAction_->setEnabled(false);
        if (stopAction_) stopAction_->setEnabled(true);
        if (!m_statusLabel) return;
        m_statusLabel->setText(tr("Status: RUNNING"));
        QFont f = m_statusLabel->font();
        f.setBold(true);
        m_statusLabel->setFont(f);
        m_statusLabel->setStyleSheet("color: #1b8f22;"); // green
    });
    connect(execEngine_, &ExecutionEngine::executionFinished, this, [this]() {
        if (runAction_) runAction_->setEnabled(true);
        if (stopAction_) stopAction_->setEnabled(false);
        if (!m_statusLabel) return;
        m_statusLabel->setText(tr("Status: Idle"));
        QFont f = m_statusLabel->font();
        f.setBold(false);
        m_statusLabel->setFont(f);
        m_statusLabel->setStyleSheet("");
    });

    // Refresh Stage Output whenever a node's output packet changes (including
    // mid-run progress updates from long-running nodes such as RagIndexerNode).
    connect(execEngine_, &ExecutionEngine::nodeOutputChanged,
            this, [this](QtNodes::NodeId /*nodeId*/) {
                refreshStageOutput();
            });

    // Repaint specific node on status changes to force painter invocation
    connect(execEngine_, &ExecutionEngine::nodeStatusChanged,
            this, [this](const QUuid &nodeId, int /*state*/) { onNodeRepaint(nodeId); });

    updateGraphNavigation();
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

    manageProvidersAction_ = new QAction(tr("Manage Providers..."), this);
    manageProvidersAction_->setStatusTip(tr("Configure provider availability, endpoints, model filters, and test probes"));
    connect(manageProvidersAction_, &QAction::triggered, this, &MainWindow::onManageProviders);

    syntaxHighlightingOptionsAction_ = new QAction(tr("Syntax Highlighting Options..."), this);
    syntaxHighlightingOptionsAction_->setStatusTip(tr("Configure syntax highlighter commands and test availability"));
    connect(syntaxHighlightingOptionsAction_, &QAction::triggered, this, &MainWindow::onSyntaxHighlightingOptions);

    // Delete action
    deleteAction = new QAction(tr("Delete"), this);
    deleteAction->setShortcuts({QKeySequence::Delete, Qt::Key_Backspace});
    deleteAction->setStatusTip(tr("Delete selected nodes and connections"));
    connect(deleteAction, &QAction::triggered, this, &MainWindow::onDeleteSelected);

    // Run action removed in favor of Targeted Scenario Runner in toolbar
    /*
    runAction_ = new QAction(tr("&Run"), this);
    runAction_->setStatusTip(tr("Execute the current pipeline"));
    runAction_->setIcon(QIcon(":/icons/run.png")); // Assuming icons might exist or just set text
    // Connect to slot that clears output before running
    connect(runAction_, &QAction::triggered, this, &MainWindow::onRunPipeline);
    */

    stopAction_ = new QAction(tr("&Stop"), this);
    stopAction_->setStatusTip(tr("Stop the current pipeline execution"));
    stopAction_->setIcon(QIcon(":/icons/stop.png"));
    stopAction_->setEnabled(false);
    connect(stopAction_, &QAction::triggered, this, &MainWindow::onStopPipeline);

    // Save last output action (Pipeline menu)
    saveOutputAction_ = new QAction(tr("Save Last Output..."), this);
    saveOutputAction_->setStatusTip(tr("Save the text content from the Stage Output dock to a file"));
    connect(saveOutputAction_, &QAction::triggered, this, &MainWindow::onSaveOutput);

    // View menu action to toggle debug log dock
    showDebugLogAction_ = new QAction(tr("Show Debug Log"), this);
    showDebugLogAction_->setCheckable(true);
    showDebugLogAction_->setChecked(false);

    // Pipeline menu action to enable/disable debug logging
    enableDebugLoggingAction_ = new QAction(tr("Enable Debug Logging"), this);
    enableDebugLoggingAction_->setCheckable(true);
    enableDebugLoggingAction_->setChecked(AppLogHelper::isGlobalDebugEnabled());
    connect(enableDebugLoggingAction_, &QAction::toggled, this, [](bool enabled) {
        AppLogHelper::setGlobalDebugEnabled(enabled);
    });

    // Slow Motion Mode toggle
    slowMotionAction_ = new QAction(tr("Slow Motion Mode"), this);
    slowMotionAction_->setCheckable(true);
    slowMotionAction_->setChecked(false);

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
    editMenu->addAction(deleteAction);
    editMenu->addAction(clearCanvasAction_);

    // Settings menu
    QMenu* settingsMenu = menuBar()->addMenu(tr("&Settings"));
    settingsMenu->addAction(editCredentialsAction_);
    settingsMenu->addAction(manageProvidersAction_);
    settingsMenu->addSeparator();
    settingsMenu->addAction(syntaxHighlightingOptionsAction_);

    // View menu
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(showDebugLogAction_);

    // Pipeline menu
    QMenu* pipelineMenu = menuBar()->addMenu(tr("&Pipeline"));
    // Build a dynamic Run submenu that lists Entry Points
    runMenu_ = new QMenu(tr("&Run"), this);
    // Populate on demand to reflect current graph state
    connect(runMenu_, &QMenu::aboutToShow, this, &MainWindow::populateRunMenu);
    pipelineMenu->addMenu(runMenu_);
    pipelineMenu->addAction(stopAction_);
    pipelineMenu->addSeparator();
    pipelineMenu->addAction(saveOutputAction_);
    pipelineMenu->addAction(enableDebugLoggingAction_);

    // Slow Motion toggle (500 ms delay when enabled)
    pipelineMenu->addSeparator();
    pipelineMenu->addAction(slowMotionAction_);
    if (execEngine_) {
        connect(slowMotionAction_, &QAction::toggled, this, [this](bool enabled){
            if (!execEngine_) return;
            execEngine_->setExecutionDelay(enabled ? 500 : 0);
        });
    }

    // Help menu
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAction);
}


void MainWindow::createToolBar() {
    QToolBar* toolbar = addToolBar(tr("Main Toolbar"));
    toolbar->setObjectName("MainToolbar");

    loopBackButton_ = new QPushButton(tr("Back"), this);
    loopBackButton_->setEnabled(false);
    connect(loopBackButton_, &QPushButton::clicked, this, &MainWindow::navigateBackGraph);
    toolbar->addWidget(loopBackButton_);

    graphBreadcrumbLabel_ = new QLabel(tr(" Root "), this);
    toolbar->addWidget(graphBreadcrumbLabel_);
    toolbar->addSeparator();

    toolbar->addWidget(new QLabel(tr(" Scenario: "), this));
    scenarioCombo_ = new QComboBox(this);
    scenarioCombo_->setMinimumWidth(200);
    toolbar->addWidget(scenarioCombo_);

    runScenarioButton_ = new QPushButton(tr("Run Scenario"), this);
    runScenarioButton_->setIcon(QIcon(":/icons/run.png"));
    connect(runScenarioButton_, &QPushButton::clicked, this, [this]() {
        if (!scenarioCombo_ || !execEngine_) return;
        QVariant data = scenarioCombo_->currentData();
        if (data.isValid()) {
            QUuid uuid = data.value<QUuid>();
            QtNodes::NodeId nodeId = execEngine_->nodeIdForUuid(uuid);
            if (nodeId != std::numeric_limits<unsigned int>::max()) {
                // clear stage output and text output nodes before run
                if (stageOutputText_) stageOutputText_->clear();
                clearAllTextOutputNodes();

                if (m_currentFileName.isEmpty()) {
                    execEngine_->setProjectName(QStringLiteral("Untitled"));
                } else {
                    execEngine_->setProjectName(QFileInfo(m_currentFileName).baseName());
                }
                execEngine_->Run(nodeId);
            }
        }
    });
    toolbar->addWidget(runScenarioButton_);

    toolbar->addAction(stopAction_);

    RefreshScenarioList();
    updateGraphNavigation();
}

void MainWindow::createStatusBar() {
    statusBar()->showMessage(tr("Ready"));
    if (!m_statusLabel) {
        m_statusLabel = new QLabel(tr("Status: Idle"), this);
    }
    statusBar()->addPermanentWidget(m_statusLabel, 0);
}

void MainWindow::connectGraphModelSignals(NodeGraphModel* model)
{
    if (!model || connectedGraphModels_.contains(model)) {
        return;
    }
    connectedGraphModels_.insert(model);

    connect(model, &NodeGraphModel::childGraphOpenRequested,
            this, &MainWindow::openChildGraph);
    connect(model, &NodeGraphModel::executionNodeStatusChanged,
            execStateModel_.get(), &ExecutionStateModel::onNodeStatusChanged,
            Qt::UniqueConnection);
    connect(model, &NodeGraphModel::executionConnectionStatusChanged,
            execStateModel_.get(), &ExecutionStateModel::onConnectionStatusChanged,
            Qt::UniqueConnection);
    connect(model, &NodeGraphModel::executionNodeStatusChanged,
            this, [this](const QUuid& nodeId, int) { onNodeRepaint(nodeId); });
    connect(model, &NodeGraphModel::executionNodeOutputChanged,
            this, [this](QtNodes::NodeId) { refreshStageOutput(); });

    if (model == _graphModel) {
        connect(model, &NodeGraphModel::nodeCreated, this, &MainWindow::RefreshScenarioList);
        connect(model, &NodeGraphModel::nodeDeleted, this, &MainWindow::RefreshScenarioList);
        connect(model, &NodeGraphModel::connectionCreated, this, &MainWindow::RefreshScenarioList);
        connect(model, &NodeGraphModel::connectionDeleted, this, &MainWindow::RefreshScenarioList);
    }
}

void MainWindow::installGraphScene(NodeGraphModel* model)
{
    if (!_graphView || !model) {
        return;
    }

    auto* oldScene = _graphView->scene();
    if (oldScene) {
        static_cast<QGraphicsView*>(_graphView)->setScene(nullptr);
        delete oldScene;
    }

    auto* scene = new QtNodes::DataFlowGraphicsScene(*model, this);
    scene->setNodePainter(std::unique_ptr<QtNodes::AbstractNodePainter>(
        new ExecutionAwareNodePainter(execStateModel_, model, scene)));
    scene->setConnectionPainter(std::unique_ptr<QtNodes::AbstractConnectionPainter>(
        new ExecutionAwareConnectionPainter(execStateModel_, model)));

    connect(execStateModel_.get(), &ExecutionStateModel::stateChanged,
            scene, [scene]() { scene->update(); });
    connect(scene, &QtNodes::BasicGraphicsScene::nodeSelected,
            this, &MainWindow::onNodeSelected);
    connect(scene, &QGraphicsScene::selectionChanged,
            this, &MainWindow::onSelectionChanged);

    static_cast<QGraphicsView*>(_graphView)->setScene(scene);
}

NodeGraphModel* MainWindow::activeGraphModel() const
{
    return currentGraphModel_ ? currentGraphModel_ : _graphModel;
}

void MainWindow::updateGraphNavigation()
{
    if (loopBackButton_) {
        loopBackButton_->setEnabled(!graphStack_.isEmpty());
    }
    if (graphBreadcrumbLabel_) {
        QStringList labels;
        labels << QStringLiteral("Root");
        for (const auto& entry : graphStack_) {
            if (entry.first != _graphModel && !entry.second.isEmpty()) {
                labels << entry.second;
            }
        }
        if (currentGraphModel_ != _graphModel && !currentGraphLabel_.isEmpty()) {
            labels << currentGraphLabel_;
        }
        graphBreadcrumbLabel_->setText(QStringLiteral(" %1 ").arg(labels.join(QStringLiteral(" / "))));
    }
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
        if (descriptionLabel_) descriptionLabel_->setVisible(false);
        if (descriptionEdit_) descriptionEdit_->setVisible(false);
        currentConfigWidget_.clear();
        return;
    }

    // Add/show new widget
    if (placeholderLabel_) placeholderLabel_->setVisible(false);
    if (descriptionLabel_) descriptionLabel_->setVisible(true);
    if (descriptionEdit_) descriptionEdit_->setVisible(true);

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
    NodeGraphModel* model = activeGraphModel();
    if (!model) {
        setPropertiesWidget(nullptr);
        // Clear and disable description edit
        if (descriptionEdit_) {
            descriptionEdit_->blockSignals(true);
            descriptionEdit_->clear();
            descriptionEdit_->setEnabled(false);
            descriptionEdit_->blockSignals(false);
        }
        return; 
    }

    // Fetch our ToolNodeDelegate for this nodeId
    auto* delegate = model->delegateModel<ToolNodeDelegate>(nodeId);
    if (!delegate) {
        setPropertiesWidget(nullptr);
        // Clear and disable description edit
        if (descriptionEdit_) {
            descriptionEdit_->blockSignals(true);
            descriptionEdit_->clear();
            descriptionEdit_->setEnabled(false);
            descriptionEdit_->blockSignals(false);
        }
        return;
    }

    // Update description edit
    if (descriptionEdit_) {
        descriptionEdit_->blockSignals(true);
        descriptionEdit_->setPlainText(delegate->description());
        descriptionEdit_->setEnabled(true);
        descriptionEdit_->blockSignals(false);
        
        // Disconnect any previous connections to avoid duplicates
        disconnect(descriptionEdit_, &QPlainTextEdit::textChanged, nullptr, nullptr);
        
        // Connect textChanged to update delegate and trigger repaint
        connect(descriptionEdit_, &QPlainTextEdit::textChanged, this, [this, nodeId, delegate]() {
            if (delegate) {
                delegate->setDescription(descriptionEdit_->toPlainText());
                // Trigger repaint of the node on canvas
                auto* scene = qobject_cast<QtNodes::DataFlowGraphicsScene*>(_graphView ? _graphView->scene() : nullptr);
                if (scene) {
                    scene->update();
                }
            }
        });
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
        refreshStageOutput();
        return;
    }

    // For now, take the first selected node
    onNodeSelected(*sel.begin());
    refreshStageOutput();
}

void MainWindow::openChildGraph(const QString& bodyId, const QString& title, int graphKind)
{
    NodeGraphModel* model = activeGraphModel();
    if (!model) {
        return;
    }

    const auto kind = static_cast<NodeGraphModel::GraphKind>(graphKind);
    NodeGraphModel* body = model->ensureSubgraph(bodyId, kind);
    if (!body) {
        return;
    }

    connectGraphModelSignals(body);
    graphStack_.append(qMakePair(model, currentGraphLabel_));
    currentGraphModel_ = body;
    currentGraphLabel_ = title.isEmpty() ? tr("Scope Body") : title;

    setPropertiesWidget(nullptr);
    installGraphScene(body);
    updateGraphNavigation();
    if (stageOutputText_) {
        stageOutputText_->setPlainText(tr("Editing scope body. Outputs are shown on the parent scope after execution."));
    }
}

void MainWindow::navigateBackGraph()
{
    if (graphStack_.isEmpty()) {
        return;
    }
    const auto entry = graphStack_.takeLast();
    currentGraphModel_ = entry.first ? entry.first : _graphModel;
    currentGraphLabel_ = entry.second.isEmpty() ? QStringLiteral("Root") : entry.second;

    setPropertiesWidget(nullptr);
    installGraphScene(currentGraphModel_);
    updateGraphNavigation();
    refreshStageOutput();
}

void MainWindow::RefreshScenarioList() {
    if (!scenarioCombo_ || !_graphModel) return;

    // Store current selection to restore it if possible
    QUuid currentUuid = scenarioCombo_->currentData().value<QUuid>();

    scenarioCombo_->clear();
    const auto entries = _graphModel->getEntryPoints();
    for (const auto& pair : entries) {
        scenarioCombo_->addItem(pair.second, pair.first);
    }

    // Restore selection
    if (!currentUuid.isNull()) {
        int index = scenarioCombo_->findData(currentUuid);
        if (index >= 0) {
            scenarioCombo_->setCurrentIndex(index);
        }
    }
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

    // Clear the stage output after saving (UI only, not part of saved state)
    if (stageOutputText_) {
        stageOutputText_->clear();
    }

    // Clear all TextOutputNode displays after UI cleanup
    clearAllTextOutputNodes();

    m_currentFileName = fileName;

    statusBar()->showMessage(tr("Saved to %1").arg(QFileInfo(fileName).fileName()), 3000);
}

void MainWindow::onRunPipeline()
{
    runScenarioFromNodeId(std::numeric_limits<unsigned int>::max());
}

void MainWindow::runScenarioFromNodeId(unsigned int nodeId)
{
    if (stageOutputText_) {
        stageOutputText_->clear();
    }

    clearAllTextOutputNodes();

    if (!execEngine_) {
        return;
    }

    if (m_currentFileName.isEmpty()) {
        execEngine_->setProjectName(QStringLiteral("Untitled"));
    } else {
        execEngine_->setProjectName(QFileInfo(m_currentFileName).baseName());
    }

    execEngine_->Run(nodeId);
}

void MainWindow::onStopPipeline() {
    if (execEngine_) {
        execEngine_->stop();
    }
}

void MainWindow::populateRunMenu()
{
    if (!runMenu_) return;
    runMenu_->clear();

    if (!_graphModel || !execEngine_) return;

    const auto entries = _graphModel->getEntryPoints();
    for (const auto& pair : entries) {
        const QUuid uuid = pair.first;
        const QString label = pair.second;
        QAction* act = runMenu_->addAction(tr("Run: %1").arg(label));
        connect(act, &QAction::triggered, this, [this, uuid]() {
            if (execEngine_) {
                runScenarioFromNodeId(execEngine_->nodeIdForUuid(uuid));
            }
        });
    }
}

void MainWindow::onEditCredentials()
{
    CredentialsDialog dialog(this);
    dialog.exec();
}

void MainWindow::onManageProviders()
{
    ProviderManagementDialog dialog(this);
    dialog.exec();
}

void MainWindow::onSyntaxHighlightingOptions()
{
    SyntaxHighlightingOptionsDialog dialog(this);
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
                // Legacy LLM connector saves should map to universal-llm
                mapped = QStringLiteral("universal-llm");
            } else {
                // Fallback to a safe default to allow loading
                mapped = QStringLiteral("text-input");
            }
        } else {
            // Remap legacy human-readable names to stable IDs
            if (modelName == QStringLiteral("LLM Connector") || modelName == QStringLiteral("LLMConnector") ||
                modelName == QStringLiteral("Google LLM Connector") || modelName == QStringLiteral("GoogleLLMConnector") ||
                modelName == QStringLiteral("llm-connector") || modelName == QStringLiteral("google-llm-connector")) {
                mapped = QStringLiteral("universal-llm");
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

    // Clear UI state only after the file has been parsed successfully.
    if (stageOutputText_) {
        stageOutputText_->clear();
    }
    setPropertiesWidget(nullptr);

    graphStack_.clear();
    currentGraphModel_ = _graphModel;
    currentGraphLabel_ = QStringLiteral("Root");
    connectedGraphModels_.clear();
    connectedGraphModels_.insert(_graphModel);
    updateGraphNavigation();

    // Detach the scene before mutating the model. Otherwise loaded delegates can
    // return embedded widgets that are still owned by proxies in the old scene.
    if (_graphView && _graphView->scene()) {
        auto* oldScene = _graphView->scene();
        static_cast<QGraphicsView*>(_graphView)->setScene(nullptr);
        delete oldScene;
    }

    try {
        if (_graphModel) {
            _graphModel->load(migrated);
        }
    } catch (const std::exception& e) {
        installGraphScene(_graphModel);
        updateGraphNavigation();
        QMessageBox::critical(this, tr("Open Failed"),
                              tr("An error occurred while loading the pipeline:\n%1").arg(QString::fromUtf8(e.what())));
        return;
    } catch (...) {
        installGraphScene(_graphModel);
        updateGraphNavigation();
        QMessageBox::critical(this, tr("Open Failed"),
                              tr("An unknown error occurred while loading the pipeline."));
        return;
    }

    // Clear all TextOutputNode instances after loading
    clearAllTextOutputNodes();

    installGraphScene(_graphModel);
    updateGraphNavigation();

    m_currentFileName = fileName;
    RefreshScenarioList();

    // Zoom to fit the entire pipeline in view
    if (_graphView && _graphView->scene()) {
        QRectF boundingRect = _graphView->scene()->itemsBoundingRect();
        if (!boundingRect.isEmpty()) {
            // Add a small margin (10% on each side) so nodes aren't glued to the edges
            const qreal margin = 0.1;
            boundingRect.adjust(-boundingRect.width() * margin,
                                -boundingRect.height() * margin,
                                boundingRect.width() * margin,
                                boundingRect.height() * margin);
            _graphView->fitInView(boundingRect, Qt::KeepAspectRatio);
        }
    }

    statusBar()->showMessage(tr("Loaded from %1").arg(QFileInfo(fileName).fileName()), 3000);
}

void MainWindow::onClearCanvas()
{
    // Clear all nodes and connections from the graph model
    if (NodeGraphModel* model = activeGraphModel()) {
        model->clear();
    }

    if (currentGraphModel_ == _graphModel) {
        graphStack_.clear();
        connectedGraphModels_.clear();
        connectedGraphModels_.insert(_graphModel);
        updateGraphNavigation();
    }

    // Reset the properties panel so it doesn't hold stale pointers
    setPropertiesWidget(nullptr);
}

void MainWindow::onDeleteSelected()
{
    // Get the scene
    auto* scene = qobject_cast<QtNodes::DataFlowGraphicsScene*>(_graphView ? _graphView->scene() : nullptr);
    NodeGraphModel* model = activeGraphModel();
    if (!scene || !model) {
        return;
    }

    // Get selected connections and delete them first (to avoid dangling references)
    const auto selectedItems = scene->selectedItems();
    for (auto* item : selectedItems) {
        if (auto* connectionGraphics = dynamic_cast<QtNodes::ConnectionGraphicsObject*>(item)) {
            model->deleteConnection(connectionGraphics->connectionId());
        }
    }

    // Get selected nodes and delete them
    const auto selectedNodes = scene->selectedNodes();
    for (const auto& nodeId : selectedNodes) {
        model->deleteNode(nodeId);
    }

    // Clear the properties panel if any deleted node was being displayed
    if (!selectedNodes.empty()) {
        setPropertiesWidget(nullptr);
        refreshStageOutput();
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
    if (stageOutputText_) {
        stageOutputText_->setMarkdown(text);
    }
    if (stageOutputDock_ && !stageOutputDock_->isVisible()) {
        stageOutputDock_->show();
    }
    refreshStageOutput();
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

void MainWindow::logMessage(const QString& message)
{
    if (s_instance) {
        QMetaObject::invokeMethod(s_instance, "onNodeLog", Qt::QueuedConnection,
                                  Q_ARG(QString, message));
    }
}

bool MainWindow::instanceExists()
{
    return s_instance != nullptr;
}

bool MainWindow::requestUserInput(const QString& prompt, QString& outText)
{
    UserInputDialog dialog(prompt, this);
    if (dialog.exec() == QDialog::Accepted) {
        outText = dialog.getText();
        return true;
    }
    outText = QString(); // Clear output on cancel
    return false; // User canceled
}


MainWindow::~MainWindow()
{
    s_instance = nullptr;
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
    NodeGraphModel* model = activeGraphModel();
    if (!scene || !model)
        return;

    QtNodes::NodeId foundId = 0;
    bool found = false;
    for (auto nid : model->allNodeIds()) {
        if (ExecIds::nodeUuid(model->executionScopeKey(), nid) == nodeUuid) {
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

void MainWindow::refreshStageOutput()
{
    if (!stageOutputText_ || !_graphView || !execEngine_) {
        return;
    }

    auto* scene = qobject_cast<QtNodes::DataFlowGraphicsScene*>(_graphView->scene());
    if (!scene) {
        stageOutputText_->setPlainText(tr("No scene available."));
        return;
    }

    const auto selectedNodes = scene->selectedNodes();
    
    if (selectedNodes.empty()) {
        stageOutputText_->setPlainText(tr("Select a single node to view output."));
        return;
    }
    
    if (selectedNodes.size() > 1) {
        stageOutputText_->setPlainText(tr("Multiple nodes selected. Select a single node to view output."));
        return;
    }

    // Exactly one node selected
    const QtNodes::NodeId nodeId = *selectedNodes.begin();
    NodeGraphModel* model = activeGraphModel();
    const DataPacket packet = (model && model != _graphModel)
        ? model->nodeOutput(nodeId)
        : execEngine_->nodeOutput(nodeId);

    if (packet.isEmpty()) {
        stageOutputText_->setPlainText(tr("No output data available for this node.\n(Node may not have been executed yet.)"));
        return;
    }

    // Format the packet as Markdown
    QStringList lines;
    QString logs;
    for (auto it = packet.cbegin(); it != packet.cend(); ++it) {
        const QString key = it.key();
        if (key == QStringLiteral("logs")) {
            logs = it.value().toString();
            continue;
        }

        // Hidden internal fields
        if (key.startsWith(QLatin1Char('_'))) {
            continue;
        }

        QVariant val = it.value();
        QString valueStr;
        if (val.typeId() == QMetaType::QVariantList || val.typeId() == QMetaType::QStringList || val.typeId() == QMetaType::QVariantMap) {
            valueStr = QJsonDocument::fromVariant(val).toJson(QJsonDocument::Indented).trimmed();
            // Wrap in code block for better formatting in the dock
            valueStr = QStringLiteral("\n```json\n%1\n```").arg(valueStr);
        } else {
            valueStr = val.toString();
        }
        lines << QStringLiteral("**%1**: %2").arg(key, valueStr);
    }
    
    QString markdown;
    if (!logs.isEmpty()) {
        markdown = logs;
        if (!lines.isEmpty()) {
            markdown += QStringLiteral("\n\n---\n\n");
        }
    }
    markdown += lines.join(QStringLiteral("\n\n"));
    stageOutputText_->setMarkdown(markdown);
}

void MainWindow::clearAllTextOutputNodes()
{
    clearTextOutputNodes(_graphModel);
}

void MainWindow::clearTextOutputNodes(NodeGraphModel* model)
{
    if (!model) {
        return;
    }

    for (auto nodeId : model->allNodeIds()) {
        // Get the delegate for this node
        auto* delegate = model->delegateModel<ToolNodeDelegate>(nodeId);
        if (!delegate) {
            continue;
        }

        // Get the underlying node
        auto node = delegate->node();
        if (!node) {
            continue;
        }

        // Try to cast to TextOutputNode
        if (auto* textOutputNode = dynamic_cast<TextOutputNode*>(node.get())) {
            textOutputNode->clearOutput();
        }
    }

    for (NodeGraphModel* child : model->subgraphModels()) {
        clearTextOutputNodes(child);
    }
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

    if (!stageOutputText_) {
        return;
    }

    const QString text = stageOutputText_->toPlainText();

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
