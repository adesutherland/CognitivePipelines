//
// Cognitive Pipeline Application
//
// Custom painters that overlay execution-state highlighting for nodes and connections.
//
// Copyright (c) 2025 Adrian Sutherland
// MIT License
//
#pragma once

#include <memory>

#include <QColor>
#include <QPen>

#include <QtNodes/internal/AbstractNodePainter.hpp>
#include <QtNodes/internal/AbstractConnectionPainter.hpp>
#include <QtNodes/internal/DefaultNodePainter.hpp>
#include <QtNodes/internal/DefaultConnectionPainter.hpp>
#include <QtNodes/internal/NodeGraphicsObject.hpp>
#include <QtNodes/internal/ConnectionGraphicsObject.hpp>
#include <QtNodes/internal/AbstractNodeGeometry.hpp>
#include <QtNodes/internal/BasicGraphicsScene.hpp>

#include "ExecutionStateModel.h"
#include "ExecutionIdUtils.h"

class ExecutionAwareNodePainter : public QtNodes::AbstractNodePainter
{
public:
    explicit ExecutionAwareNodePainter(std::shared_ptr<ExecutionStateModel> model,
                                        QtNodes::AbstractGraphModel* graphModel,
                                        QtNodes::BasicGraphicsScene* scene)
        : model_(std::move(model)), graphModel_(graphModel), scene_(scene) {}

    void paint(QPainter *painter, QtNodes::NodeGraphicsObject &ngo) const override;

private:
    static QPen highlightPenFor(ExecutionState state);

private:
    QtNodes::DefaultNodePainter fallback_{};
    std::shared_ptr<ExecutionStateModel> model_;
    QtNodes::AbstractGraphModel* graphModel_;
    QtNodes::BasicGraphicsScene* scene_;
};

class ExecutionAwareConnectionPainter : public QtNodes::AbstractConnectionPainter
{
public:
    explicit ExecutionAwareConnectionPainter(std::shared_ptr<ExecutionStateModel> model)
        : model_(std::move(model)) {}

    void paint(QPainter *painter, QtNodes::ConnectionGraphicsObject const &cgo) const override;
    QPainterPath getPainterStroke(QtNodes::ConnectionGraphicsObject const &cgo) const override;

private:
    static QPen highlightPenFor(ExecutionState state);

private:
    QtNodes::DefaultConnectionPainter fallback_{};
    std::shared_ptr<ExecutionStateModel> model_;
};
