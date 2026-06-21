#include "gmdg_logger.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>

#include <windows.h>

namespace
{
    struct log_record_header
    {
        uint64_t timestamp_ns;
        uint32_t thread_id;
        uint32_t level;

        uint32_t category_len;
        uint32_t message_len;
    };

    std::FILE* g_file = nullptr;
    std::mutex g_mutex;

    uint64_t GetTimeNanoseconds()
    {
        using namespace std::chrono;

        return duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
    }

    uint32_t GMDG_GetThreadID()
    {
        return GetCurrentThreadId();
    }
}

extern "C" int32_t GMDG_Logger_Initialize(const char* path)
{
    std::lock_guard lock(g_mutex);

    g_file = std::fopen(path, "ab");

    return g_file != nullptr;
}

extern "C" void GMDG_Logger_Shutdown()
{
    std::lock_guard lock(g_mutex);

    if (g_file)
    {
        std::fclose(g_file);
        g_file = nullptr;
    }
}

extern "C" void GMDG_Log(
    uint32_t    t_severity,
    const char* t_category,
    uint32_t    t_category_length,
    const char* t_message,
    uint32_t    t_message_length)
{
    std::lock_guard lock(g_mutex);

    if (!g_file)
        return;

    log_record_header hdr{
        GetTimeNanoseconds(),
        GMDG_GetThreadID(),
        t_severity,
        t_category_length,
        t_message_length
    };

    std::fwrite(&hdr, sizeof(hdr), 1, g_file);
    std::fwrite(t_category, 1, t_category_length, g_file);
    std::fwrite(t_message, 1, t_message_length, g_file);

    std::fflush(g_file);
}

extern "C" const char* GMDG_Logger_Severity_To_String(uint32_t t_severity)
{
    switch (t_severity)
    {
    case GMDG_LOG_DEBUG:
    {
        return "DEBUG";
    }
    case GMDG_LOG_INFO:
    {
        return "INFO";
    }
    case GMDG_LOG_WARNING:
    {
        return "WARNING";
    }
    case GMDG_LOG_ERROR:
    {
        return "ERROR";
    }    
    default:
    {
        return "UNKNOWN";
    }
    }
}