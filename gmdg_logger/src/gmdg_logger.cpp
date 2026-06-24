#include "gmdg_logger.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>
#include <cstring>
#include <cassert>

#include <windows.h>

namespace
{
    static constexpr uint32_t GMDG_LOG_FORMAT_VERSION = 1;
    static constexpr std::array<char, 8> GMDG_LOG_MAGIC = {'G', 'M', 'D', 'G', 'L', 'O', 'G', '\0'};

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

    GMDGBool WriteLogFileHeader(std::FILE* t_file)
    {
        if (!t_file) return GMDG_FALSE;

        GMDGLogFileHeader header{};
        std::memcpy(header.magic, GMDG_LOG_MAGIC.data(), sizeof(header.magic));
        header.format_version = GMDG_LOG_FORMAT_VERSION;
        header.record_header_size = sizeof(GMDGLogFileHeader);
        header.flags = 0;

        return std::fwrite(&header, sizeof(header), 1, t_file) == 1 ? GMDG_TRUE : GMDG_FALSE;
    }
}

extern "C" GMDGBool GMDG_Logger_Initialize(const char* path)
{
    if (!path) return GMDG_FALSE;

    std::lock_guard lock(g_mutex);

    // Open file
    g_file = std::fopen(path, "ab+");
    if (!g_file) return GMDG_FALSE;

    // Move at the end
    if (std::fseek(g_file, 0, SEEK_END) != 0)
    {
        std::fclose(g_file);
        g_file = nullptr;
        return GMDG_FALSE;
    }

    // Check file size
    const long file_size = std::ftell(g_file);
    if (file_size < 0)
    {
        std::fclose(g_file);
        g_file = nullptr;
        return GMDG_FALSE;
    }

    if (file_size == 0)
    {
        if (std::fseek(g_file, 0, SEEK_SET) != 0)
        {
            std::fclose(g_file);
            g_file = nullptr;
            return GMDG_FALSE;
        }

        if (!WriteLogFileHeader(g_file))
        {
            std::fclose(g_file);
            g_file = nullptr;
            return GMDG_FALSE;
        }
    }

    if (std::fseek(g_file, 0, SEEK_END) != 0)
    {
        std::fclose(g_file);
        g_file = nullptr;
        return GMDG_FALSE;
    }

    return GMDG_TRUE;
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
    GMDGLogSeverity t_severity,
    const char*     t_category,
    uint32_t        t_category_length,
    const char*     t_message,
    uint32_t        t_message_length)
{
    std::lock_guard lock(g_mutex);

    if (!g_file) return;

    GMDGLogRecord record{
        GetTimeNanoseconds(),
        GMDG_GetThreadID(),
        t_severity,
        t_category_length,
        t_message_length
    };

    std::fwrite(&record, sizeof(record), 1, g_file);
    std::fwrite(t_category, 1, t_category_length, g_file);
    std::fwrite(t_message, 1, t_message_length, g_file);

    std::fflush(g_file);
}

extern "C" const char* GMDG_Logger_Severity_To_String(GMDGLogSeverity t_severity)
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

GMDGLogFileValidationResult GMDG_Logger_Validate_File_Header(const GMDGLogFileHeader* t_header)
{
    assert(t_header);

    if (std::memcmp(t_header->magic, GMDG_LOG_MAGIC.data(), GMDG_LOG_MAGIC.size()) != 0)
    {
        return GMDG_INVALID_MAGIC;
    }

    if (t_header->format_version != GMDG_LOG_FORMAT_VERSION)
    {
        return GMDG_UNUPPORTED_VERSION;
    }

    if (t_header->record_header_size != sizeof(GMDGLogFileHeader))
    {
        return GMDG_UNUPPORTED_FILE_HEADER;
    }

    return GMDG_SUCCESS;
}