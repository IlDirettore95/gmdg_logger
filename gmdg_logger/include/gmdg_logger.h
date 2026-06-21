#pragma once

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LogSeverity
{
    GMDG_LOG_DEBUG,
    GMDG_LOG_INFO,
    GMDG_LOG_WARNING,
    GMDG_LOG_ERROR
} LogSeverity;

int32_t GMDG_Logger_Initialize(const char* t_path);

void GMDG_Logger_Shutdown();

void GMDG_Log(
    uint32_t    t_severity,
    const char* t_category,
    uint32_t    t_category_length,
    const char* t_message,
    uint32_t    t_message_length);

const char* GMDG_Logger_Severity_To_String(uint32_t t_severity);

#ifdef __cplusplus
}
#endif