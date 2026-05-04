#pragma once

#include "ScopeRuntime.h"

class NodeGraphModel;

class ScopeSubgraphExecutor {
public:
    static ScopeBodyResult run(NodeGraphModel* graph,
                               ScopeBodyKind kind,
                               const ScopeFrame& frame,
                               const DataPacket& parentInputs);
};
