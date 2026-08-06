#include "async_backend.hpp"

#include "gmdg_logger.h"

namespace
{
    void Initialize(const char* t_path)
    {
        GMDG_Logger_Initialize(t_path);
    }

    void Log(
        GMDGLogSeverity t_severity,
        const char*     t_category,
        uint32_t        t_categoryLength,
        const char*     t_message,
        uint32_t        t_messageLength)
    {
        GMDG_Log(t_severity, t_category, t_categoryLength, t_message, t_messageLength);
    }

    void Shutdown()
    {
        GMDG_Logger_Shutdown();
    }

    uint64_t GetDroppedRecordCount()
    {
        return GMDG_Logger_GetDroppedRecordCount();
    }
}

const LoggerBackend AsyncBackend{
    "async (writer thread)",
    &Initialize,
    &Log,
    &Shutdown,
    &GetDroppedRecordCount
};
