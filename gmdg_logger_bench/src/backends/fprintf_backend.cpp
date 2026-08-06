#include "fprintf_backend.hpp"

#include "gmdg_asserting.hpp"

#include <cstdio>
#include <mutex>

namespace
{
    std::FILE* g_file = nullptr;
    std::mutex g_mutex;

    void Initialize(const char* t_path)
    {
        GMDG_ASSERT_WITH_MESSAGE(t_path, "path is null");

        g_file = std::fopen(t_path, "wb");
    }

    void Log(
        GMDGLogSeverity t_severity,
        const char*     t_category,
        uint32_t        t_categoryLength,
        const char*     t_message,
        uint32_t        t_messageLength)
    {
        std::lock_guard lock(g_mutex);

        if (!g_file) return;

        std::fprintf(g_file, "[%s][%.*s] %.*s\n",
            GMDG_Logger_Severity_To_String(t_severity),
            static_cast<int>(t_categoryLength), t_category,
            static_cast<int>(t_messageLength), t_message);
    }

    void Shutdown()
    {
        std::lock_guard lock(g_mutex);

        if (g_file)
        {
            std::fclose(g_file);
            g_file = nullptr;
        }
    }

    uint64_t GetDroppedRecordCount()
    {
        // Never drops: blocks the caller instead.
        return 0;
    }
}

const LoggerBackend FprintfBackend{
    "fprintf baseline",
    &Initialize,
    &Log,
    &Shutdown,
    &GetDroppedRecordCount
};
