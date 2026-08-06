#pragma once

#include "gmdg_logger.h"

#include <cstdint>

// Common interface every logger implementation being compared in the benchmark must satisfy.
// Plain function pointers match this codebase's existing global-state style: every backend keeps
// its state in globals rather than an instance, so there's nothing to bind a `this` to. Adding a
// future backend only requires a new .hpp/.cpp pair implementing these five members and one line
// in main.cpp's Targets[] registry.
struct LoggerBackend
{
    const char* Name;

    void (*Initialize)(const char* t_path);

    void (*Log)(
        GMDGLogSeverity t_severity,
        const char*     t_category,
        uint32_t        t_categoryLength,
        const char*     t_message,
        uint32_t        t_messageLength);

    void (*Shutdown)();

    // Backends that never drop records (they block the caller instead) always return 0.
    uint64_t (*GetDroppedRecordCount)();
};
