#include "gmdg_logger.h"

#include "gmdg_asserting.hpp"

#include <atomic>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
#include <cstring>

#include <windows.h>

namespace
{
    static constexpr uint32_t GMDG_LOG_FORMAT_VERSION = 1;
    static constexpr std::array<char, 8> GMDG_LOG_MAGIC = {'G', 'M', 'D', 'G', 'L', 'O', 'G', '\0'};

    // Async double-buffered writer: GMDG_Log() only ever memcpy's into the "front" buffer under
    // g_bufferMutex and returns - no file I/O on the calling thread. A background thread wakes up
    // every GMDG_LOG_FLUSH_INTERVAL, swaps front/back under the same lock, and writes the
    // swapped-out buffer to disk outside the lock, so producers keep appending to the new front
    // buffer while the flush is in flight.
    //
    // Capacity vs. flush cadence bounds how much of a burst the logger can absorb before it starts
    // dropping records (visible via GMDG_Logger_GetDroppedRecordCount): benchmarking showed the
    // previous 1 MB / 20 ms combo dropping the majority of records under bursty multi-threaded load.
    // Raised here to widen that burst window; sustained overload still drops rather than blocking
    // the caller, which is the deliberate non-blocking contract of this design.
    static constexpr size_t GMDG_LOG_BUFFER_CAPACITY = 8 * 1024 * 1024; // 8 MB per buffer
    static constexpr std::chrono::milliseconds GMDG_LOG_FLUSH_INTERVAL{5};

    std::FILE* g_file = nullptr;
    std::mutex g_mutex;

    std::array<std::array<uint8_t, GMDG_LOG_BUFFER_CAPACITY>, 2> g_buffers;
    std::mutex g_bufferMutex;
    size_t g_frontIndex = 0;
    size_t g_frontOffset = 0;

    std::atomic<uint64_t> g_droppedRecordCount{0};
    std::atomic<bool> g_running{false};
    std::condition_variable g_wakeCv;
    std::thread g_writerThread;

    uint64_t GetTimeNanoseconds()
    {
        // std::chrono::system_clock::now() round-trips through winpthread's clock_gettime64
        // (FILETIME -> timespec -> ns, via two non-inlinable call hops) to reach this exact same
        // Windows API call. Calling it directly skips that and the redundant timespec conversion.
        FILETIME ft;
        GetSystemTimePreciseAsFileTime(&ft);

        const uint64_t ticksSince1601 = (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;

        // FILETIME is in 100ns intervals since 1601-01-01; convert to ns since the Unix epoch (1970-01-01).
        constexpr uint64_t EPOCH_DIFFERENCE_100NS = 116444736000000000ULL;
        return (ticksSince1601 - EPOCH_DIFFERENCE_100NS) * 100;
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

    void WriterThreadLoop()
    {
        for (;;)
        {
            std::unique_lock<std::mutex> lock(g_bufferMutex);
            g_wakeCv.wait_for(lock, GMDG_LOG_FLUSH_INTERVAL, [] {
                return !g_running.load(std::memory_order_acquire);
            });

            const size_t flushIndex = g_frontIndex;
            const size_t bytesToFlush = g_frontOffset;
            g_frontIndex = 1 - g_frontIndex;
            g_frontOffset = 0;

            const bool stillRunning = g_running.load(std::memory_order_acquire);
            lock.unlock();

            if (bytesToFlush > 0 && g_file)
            {
                std::fwrite(g_buffers[flushIndex].data(), 1, bytesToFlush, g_file);
                std::fflush(g_file);
            }

            if (!stillRunning) break;
        }
    }
}

extern "C" GMDGBool GMDG_Logger_Initialize(const char* path)
{
    GMDG_ASSERT_WITH_MESSAGE(path, "path is null");
    if (!path)
    {
        return GMDG_FALSE;
    }

    // lock for thread safeness
    std::lock_guard lock(g_mutex);

    if (g_running.load(std::memory_order_acquire))
    {
        GMDG_ASSERT_WITH_MESSAGE(false, "logger already initialized");
        return GMDG_FALSE;
    }

    // open/create the file in append (a), binary (b) and read/write mode (+)
    g_file = std::fopen(path, "ab+");
    if (!g_file) return GMDG_FALSE;

    // move at the end and check file size
    if (std::fseek(g_file, 0, SEEK_END) != 0)
    {
        std::fclose(g_file);
        g_file = nullptr;
        return GMDG_FALSE;
    }

    const long file_size = std::ftell(g_file);
    if (file_size < 0)
    {
        std::fclose(g_file);
        g_file = nullptr;
        return GMDG_FALSE;
    }

    if (file_size == 0)
    {
        // if the file is new, the logger will write an header
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

    // move again to the end (after potentially writing the header)
    if (std::fseek(g_file, 0, SEEK_END) != 0)
    {
        std::fclose(g_file);
        g_file = nullptr;
        return GMDG_FALSE;
    }

    {
        // reset indicies
        std::lock_guard bufferLock(g_bufferMutex);
        g_frontIndex = 0;
        g_frontOffset = 0;
    }

    // std::memory_order_relaxed is used to assure atomicity only (CPU and compiler could reorder precedent instructions)
    g_droppedRecordCount.store(0, std::memory_order_relaxed);

    // std::memory_order_release is used to assure CPU and compiler won't reorder this and precedent write instructions
    g_running.store(true, std::memory_order_release);

    // start writer thread
    g_writerThread = std::thread(WriterThreadLoop);

    return GMDG_TRUE;
}

// NOTE: callers must stop invoking GMDG_Log on every other thread before calling Shutdown() -
// a log call racing with shutdown is not synchronized against the writer thread's teardown.
extern "C" void GMDG_Logger_Shutdown()
{
    // atomic change of g_running
    g_running.store(false, std::memory_order_release);

    // immediatly awake the thread (writer) waiting on g_wakeCv 
    g_wakeCv.notify_one();

    // wait for the writer thread to finish
    if (g_writerThread.joinable())
    {
        g_writerThread.join();
    }

    std::lock_guard lock(g_mutex);

    if (g_file)
    {
        std::fclose(g_file);
        g_file = nullptr;
    }
}

extern "C" void GMDG_Log(
    uint32_t        t_severity,
    const char*     t_category,
    uint32_t        t_category_length,
    const char*     t_message,
    uint32_t        t_message_length)
{
    GMDG_ASSERT_WITH_MESSAGE(t_category != nullptr || t_category_length == 0,
        "t_category is null but t_category_length is {}", t_category_length);
    GMDG_ASSERT_WITH_MESSAGE(t_message != nullptr || t_message_length == 0,
        "t_message is null but t_message_length is {}", t_message_length);

    const size_t record_size = sizeof(GMDGLogRecord) + t_category_length + t_message_length;

    std::lock_guard lock(g_bufferMutex);

    if (!g_file) return;

    auto& buffer = g_buffers[g_frontIndex];
    if (g_frontOffset + record_size > buffer.size())
    {
        g_droppedRecordCount.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const GMDGLogRecord record{
        GetTimeNanoseconds(),
        GMDG_GetThreadID(),
        t_severity,
        t_category_length,
        t_message_length
    };

    uint8_t* dest = buffer.data() + g_frontOffset;
    std::memcpy(dest, &record, sizeof(record));
    dest += sizeof(record);
    std::memcpy(dest, t_category, t_category_length);
    dest += t_category_length;
    std::memcpy(dest, t_message, t_message_length);

    g_frontOffset += record_size;
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
        GMDG_ASSERT_WITH_MESSAGE(false, "unknown severity: {}", static_cast<int>(t_severity));
        return "UNKNOWN";
    }
    }
}

extern "C" uint64_t GMDG_Logger_GetDroppedRecordCount()
{
    return g_droppedRecordCount.load(std::memory_order_relaxed);
}

GMDGLogFileValidationResult GMDG_Logger_Validate_File_Header(const GMDGLogFileHeader* t_header)
{
    GMDG_ASSERT(t_header);

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
