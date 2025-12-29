// Centralized Qt logging categories for CognitivePipelines
#pragma once

#include <QLoggingCategory>

// Registry load/resolve traces
Q_DECLARE_LOGGING_CATEGORY(cp_registry)

// Lifecycle events across nodes/backends/widgets
Q_DECLARE_LOGGING_CATEGORY(cp_lifecycle)

// Parameter shaping / behavior decisions
Q_DECLARE_LOGGING_CATEGORY(cp_params)

// Endpoint routing diagnostics
Q_DECLARE_LOGGING_CATEGORY(cp_endpoint)

// Dynamic discovery / model list fetching
Q_DECLARE_LOGGING_CATEGORY(cp_discovery)

// Capability baseline/ad-hoc capability logs
Q_DECLARE_LOGGING_CATEGORY(cp_caps)
