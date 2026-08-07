#include "sync_backend.hpp"

#include "gmdg_asserting.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>

#include <windows.h>

namespace
{
    // Same format constants as gmdg_logger.cpp - duplicated deliberately: this is a standalone
    // reference implementation, not a consumer of the library's internals, and keeping the same
    // on-disk format is what makes the latency/throughput comparison meaningful.
    constexpr uint32_t GMDG_LOG_FORMAT_VERSION = 2;
    constexpr std::array<char, 8> GMDG_LOG_MAGIC = {'G', 'M', 'D', 'G', 'L', 'O', 'G', '\0'};

    std::FILE* g_file = nullptr;
    std::mutex g_mutex;

    uint64_t GetTimeNanoseconds()
    {
        using namespace std::chrono;

        return duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
    }

    uint32_t GetThreadID()
    {
        return GetCurrentThreadId();
    }

    void WriteFileHeader(std::FILE* t_file)
    {
        GMDGLogFileHeader header{};
        std::memcpy(header.magic, GMDG_LOG_MAGIC.data(), sizeof(header.magic));
        header.format_version = GMDG_LOG_FORMAT_VERSION;
        header.record_header_size = sizeof(GMDGLogFileHeader);
        header.flags = 0;

        std::fwrite(&header, sizeof(header), 1, t_file);
    }

    void Initialize(const char* t_path)
    {
        GMDG_ASSERT_WITH_MESSAGE(t_path, "path is null");

        std::lock_guard lock(g_mutex);

        g_file = std::fopen(t_path, "ab+");
        if (!g_file) return;

        std::fseek(g_file, 0, SEEK_END);
        if (std::ftell(g_file) == 0)
        {
            std::fseek(g_file, 0, SEEK_SET);
            WriteFileHeader(g_file);
            std::fseek(g_file, 0, SEEK_END);
        }
    }

    void Log(
        GMDGLogSeverity t_severity,
        const char*     t_category,
        uint32_t        t_categoryLength,
        const char*     t_message,
        uint32_t        t_messageLength)
    {
        const GMDGLogRecord record{
            GetTimeNanoseconds(),
            GetThreadID(),
            static_cast<uint32_t>(t_severity),
            0u,                 // thread_name_len - sync backend has no thread-naming support
            t_categoryLength,
            t_messageLength
        };

        std::lock_guard lock(g_mutex);

        if (!g_file) return;

        std::fwrite(&record, sizeof(record), 1, g_file);
        std::fwrite(t_category, 1, t_categoryLength, g_file);
        std::fwrite(t_message, 1, t_messageLength, g_file);

        // No background thread to defer this to: durability happens inline, on the caller's thread.
        std::fflush(g_file);
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
        // Never drops: a full disk or a slow write just blocks the caller instead.
        return 0;
    }
}

const LoggerBackend SyncBackend{
    "sync (no writer thread)",
    &Initialize,
    &Log,
    &Shutdown,
    &GetDroppedRecordCount
};
