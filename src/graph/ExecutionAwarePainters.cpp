#include "ExecutionAwarePainters.h"
#include "ToolNodeDelegate.h"
#include "NodeGraphModel.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QJsonDocument>
#include <QtNodes/internal/NodeStyle.hpp>
#include <QtNodes/internal/StyleCollection.hpp>
#include <QtNodes/internal/DefaultNodePainter.hpp>

using namespace QtNodes;

static inline bool isActiveState(ExecutionState s)
{
    return s == ExecutionState::Running || s == ExecutionState::Finished || s == ExecutionState::Error;
}

static inline QColor colorFor(ExecutionState state)
{
    switch (state) {
    case ExecutionState::Running: return QColor("#AED6F1");
    case ExecutionState::Finished: return QColor("#A9DFBF");
    case ExecutionState::Error: return QColor("#F5B7B1");
    case ExecutionState::Idle: default: return QColor("#808080");
    }
}

QPen ExecutionAwareNodePainter::highlightPenFor(ExecutionState state)
{
    QPen p{colorFor(state)};
    p.setWidthF(3.0);
    return p;
}

void ExecutionAwareNodePainter::paint(QPainter *painter, NodeGraphicsObject &ngo) const
{
    // Replicate DefaultNodePainter drawing, but override border and title bar colors based on execution state.

    AbstractGraphModel &model = *graphModel_;
    NodeId const nodeId = ngo.nodeId();
    AbstractNodeGeometry &geometry = scene_->nodeGeometry();

    // Fetch style and size
    QJsonDocument json = QJsonDocument::fromVariant(model.nodeData(nodeId, NodeRole::Style));
    NodeStyle nodeStyle(json.object());
    const QSize size = geometry.size(nodeId);

    // Determine execution state and state-dependent color
    ExecutionState st = ExecutionState::Idle;
    if (model_) {
        const QUuid id = ExecIds::nodeUuid(nodeId);
        st = model_->stateFor(id);
    }

    QColor idleColor = QColor("#808080");
    QColor runningColor = QColor("#AED6F1");
    QColor finishedColor = QColor("#A9DFBF");
    QColor errorColor = QColor("#F5B7B1");

    QColor borderColor = idleColor;
    switch (st) {
        case ExecutionState::Running:  borderColor = runningColor; break;
        case ExecutionState::Finished: borderColor = finishedColor; break;
        case ExecutionState::Error:    borderColor = errorColor; break;
        case ExecutionState::Idle:     borderColor = idleColor; break;
        default:                       borderColor = idleColor; break;
    }

    // Step 1: Draw node body fill (gradient) without border
    QRectF boundary(0, 0, size.width(), size.height());
    double const radius = 3.0;
    {
        painter->setPen(Qt::NoPen);
        
        QLinearGradient gradient(QPointF(0.0, 0.0), QPointF(2.0, size.height()));
        gradient.setColorAt(0.0, nodeStyle.GradientColor0);
        gradient.setColorAt(0.10, nodeStyle.GradientColor1);
        gradient.setColorAt(0.90, nodeStyle.GradientColor2);
        gradient.setColorAt(1.0, nodeStyle.GradientColor3);
        painter->setBrush(gradient);
        
        painter->drawRoundedRect(boundary, radius, radius);
    }

    // Step 2: Draw the execution-color title bar background (fill only, no border)
    {
        const QRectF capRect = geometry.captionRect(nodeId);
        const qreal titleH = capRect.height() + 8.0; // small padding similar to default visuals
        QRectF titleBar(0, 0, size.width(), titleH);
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(borderColor);
        painter->drawRect(titleBar);
        painter->restore();
    }

    // Step 3: Draw border outline on top of all fills
    {
        const bool hovered = ngo.nodeState().hovered();
        const bool selected = ngo.isSelected();
        
        // Override border styling for selected nodes to ensure high-contrast visual feedback
        QColor finalBorderColor = borderColor;
        qreal finalPenWidth = hovered ? nodeStyle.HoveredPenWidth : nodeStyle.PenWidth;
        
        if (selected) {
            finalBorderColor = QColor("#FFD700"); // Gold for high-contrast selection
            finalPenWidth = 4.0; // Thicker border for visibility
        }
        
        QPen pen(finalBorderColor, finalPenWidth);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        
        painter->drawRoundedRect(boundary, radius, radius);
    }

    // 3) Draw connection points and filled points (these should appear on top of the title bar fill)
    // Create a temporary DefaultNodePainter to delegate to its helper methods
    // This avoids storing it as a member, which causes Windows DLL export issues
    QtNodes::DefaultNodePainter defaultPainter;
    defaultPainter.drawConnectionPoints(painter, ngo);
    defaultPainter.drawFilledConnectionPoints(painter, ngo);

    // 4) Draw caption text with state-aware contrast and adjusted vertical alignment,
    //    then draw entry labels (both on top of the title bar)
    {
        AbstractGraphModel &model2 = *graphModel_;
        NodeId const nodeId2 = ngo.nodeId();

        if (model2.nodeData(nodeId2, NodeRole::CaptionVisible).toBool()) {
            QString const name = model2.nodeData(nodeId2, NodeRole::Caption).toString();

            QFont f = painter->font();
            f.setBold(true);
            painter->setFont(f);

            QPointF pos = geometry.captionPosition(nodeId2);
            // Nudge up a bit more to better center within the colored title bar
            pos.ry() -= 4.0; // move text up by ~4 px total

            QJsonDocument json2 = QJsonDocument::fromVariant(model2.nodeData(nodeId2, NodeRole::Style));
            NodeStyle nodeStyle2(json2.object());

            if (isActiveState(st)) {
                painter->setPen(Qt::black); // strong contrast on pastel backgrounds
            } else {
                painter->setPen(nodeStyle2.FontColor); // default for Idle
            }

            painter->drawText(pos, name);

            f.setBold(false);
            painter->setFont(f);
        }
    }
    defaultPainter.drawEntryLabels(painter, ngo);

    // 5) Draw resize handle if any
    defaultPainter.drawResizeRect(painter, ngo);
}

QPen ExecutionAwareConnectionPainter::highlightPenFor(ExecutionState state)
{
    QPen p{colorFor(state)};
    p.setWidthF(3.0);
    p.setCapStyle(Qt::RoundCap);
    p.setJoinStyle(Qt::RoundJoin);
    return p;
}

void ExecutionAwareConnectionPainter::paint(QPainter *painter, ConnectionGraphicsObject const &cgo) const
{
    // Fully override the default connection drawing. We draw the cubic path ourselves
    // using a state-specific color and the default line width.
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(Qt::NoBrush);

    // Determine execution state (Idle if no model or unknown)
    ExecutionState st = ExecutionState::Idle;
    if (model_) {
        const QUuid id = ExecIds::connectionUuid(cgo.connectionId());
        st = model_->stateFor(id);
    }

    // State colors
    const QColor idleColor("#808080");
    const QColor runningColor("#AED6F1");
    const QColor finishedColor("#A9DFBF");
    const QColor errorColor("#F5B7B1");

    QColor penColor = idleColor;
    switch (st) {
        case ExecutionState::Running:  penColor = runningColor; break;
        case ExecutionState::Finished: penColor = finishedColor; break;
        case ExecutionState::Error:    penColor = errorColor; break;
        case ExecutionState::Idle:     penColor = idleColor; break;
        default:                       penColor = idleColor; break;
    }

    // Use the framework's default line width if available
    const auto &connectionStyle = QtNodes::StyleCollection::connectionStyle();
    const qreal lineWidth = static_cast<qreal>(connectionStyle.lineWidth());

    // Override pen styling for selected connections to ensure high-contrast visual feedback
    const bool selected = cgo.isSelected();
    QColor finalPenColor = penColor;
    qreal finalPenWidth = lineWidth > 0.0 ? lineWidth : 2.0;
    
    if (selected) {
        finalPenColor = QColor("#FFD700"); // Gold for high-contrast selection
        finalPenWidth = 4.0; // Thicker line for visibility
    }

    QPen pen(finalPenColor);
    pen.setWidthF(finalPenWidth);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(pen);

    // Build cubic path like the default painter
    const QPointF &inPt = cgo.endPoint(PortType::In);
    const QPointF &outPt = cgo.endPoint(PortType::Out);
    const auto c1c2 = cgo.pointsC1C2();

    QPainterPath path(outPt);
    path.cubicTo(c1c2.first, c1c2.second, inPt);

    painter->drawPath(path);
    painter->restore();
}

QPainterPath ExecutionAwareConnectionPainter::getPainterStroke(ConnectionGraphicsObject const &cgo) const
{
    // Build the cubic path directly (same logic as in paint method)
    const QPointF &inPt = cgo.endPoint(PortType::In);
    const QPointF &outPt = cgo.endPoint(PortType::Out);
    const auto c1c2 = cgo.pointsC1C2();

    QPainterPath path(outPt);
    path.cubicTo(c1c2.first, c1c2.second, inPt);
    
    return path;
}
