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

#pragma once

#include <QMainWindow>
#include "llm_api_client.h"
#include "ExecutionEngine.h"
#include "ExecutionStateModel.h"
#include "UserInputDialog.h"

#include <memory>
#include <QPointer>

class QAction;
class QPushButton;
class QDockWidget;
class QVBoxLayout;
class QLabel;
class QTextEdit;
class NodeGraphModel;

namespace QtNodes {
class GraphicsView;
namespace QtNodes { class DataFlowGraphicsScene; }
}

namespace QtNodes { namespace QtNodesInternal { class BasicGraphicsScene; } }

namespace QtNodes { using NodeId = unsigned int; }

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Accessors used by headless tests
    NodeGraphModel* graphModel() const { return _graphModel; }
    ExecutionEngine* executionEngine() const { return execEngine_; }

public slots:
    // Final pipeline result
    void onPipelineFinished(const DataPacket& finalOutput);
    // Per-node debug logging
    void onNodeLog(const QString& message);

    // Request user input from worker thread (blocking)
    Q_INVOKABLE QString requestUserInput(const QString& prompt);

private slots:
    void onAbout();
    void onOpen();
    void onSaveAs();
    void onEditCredentials();
    void onClearCanvas();
    void onSaveOutput();
    void onDeleteSelected();

    // Selection handling
    void onNodeSelected(QtNodes::NodeId nodeId);
    void onSelectionChanged();

    // Execution highlighting: repaint a specific node by execution QUuid
    void onNodeRepaint(const QUuid& nodeUuid);

private:
    void createActions();
    void createMenus();
    void createStatusBar();

    void setPropertiesWidget(QWidget* w);
    void refreshStageOutput();

    QAction* exitAction {nullptr};
    QAction* openAction_ {nullptr};
    QAction* saveAsAction_ {nullptr};
    QAction* aboutAction {nullptr};

    QAction* runAction_ {nullptr};
    QAction* saveOutputAction_ {nullptr};
    QAction* showDebugLogAction_ {nullptr};
    QAction* enableDebugLoggingAction_ {nullptr};
    QAction* editCredentialsAction_ {nullptr};
    QAction* clearCanvasAction_ {nullptr};
    QAction* deleteAction {nullptr};
    ExecutionEngine* execEngine_ {nullptr};

    NodeGraphModel* _graphModel {nullptr};
    QtNodes::GraphicsView* _graphView {nullptr};

    // Properties dock
    QDockWidget* propertiesDock_ {nullptr};
    QWidget* propertiesHost_ {nullptr};
    QVBoxLayout* propertiesLayout_ {nullptr};
    QLabel* placeholderLabel_ {nullptr};
    QPointer<QWidget> currentConfigWidget_ {nullptr};

    // Output docks
    QDockWidget* stageOutputDock_ {nullptr};
    QTextEdit* stageOutputText_ {nullptr};

    QDockWidget* debugLogDock_ {nullptr};
    QTextEdit* debugLogText_ {nullptr};

    // Live execution highlighting
    std::shared_ptr<ExecutionStateModel> execStateModel_;
};
