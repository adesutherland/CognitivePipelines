#pragma once

#include <QString>
#include <QMap>
#include <QVariant>

enum class PinDirection { Input, Output };

struct PinDefinition {
    PinDirection direction;
    QString id;
    QString name;
    QString type;
};

struct NodeDescriptor {
    QString id;
    QString name;
    QString category;
    QMap<QString, PinDefinition> inputPins;
    QMap<QString, PinDefinition> outputPins;
};

using DataPacket = QMap<QString, QVariant>;
using PropertyData = QMap<QString, QVariant>;
