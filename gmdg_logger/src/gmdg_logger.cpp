#include "gmdg_logger.h"

#include "gmdg_asserting.hpp"

#include <atomic>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <format>
#include <mutex>
#include <thread>
#include <cstring>

#include <windows.h>

namespace
{
    static constexpr uint32_t LogFormatVersion = 2;
    static constexpr std::array<char, 8> LogMagic = {'G', 'M', 'D', 'G', 'L', 'O', 'G', '\0'};

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
    static constexpr size_t LogBufferCapacity = 8 * 1024 * 1024; // 8 MB per buffer
    static constexpr std::chrono::milliseconds LogFlushInterval{5};

    std::FILE* s_file = nullptr;
    std::mutex s_mutex;

    std::array<std::array<uint8_t, LogBufferCapacity>, 2> s_buffers;
    std::mutex s_bufferMutex;
    size_t s_frontIndex = 0;
    size_t s_frontOffset = 0;

    std::atomic<uint64_t> s_droppedRecordCount{0};
    std::atomic<bool> s_running{false};
    std::condition_variable s_wakeCv;
    std::thread s_writerThread;

    // Per-thread name tag, set via GMDG_SetThreadName. Left unset, GMDG_Log lazily fills it with
    // a "Thread-<id>" fallback the first time this thread logs, so every record always carries a
    // usable label without requiring callers to opt in.
    thread_local char     s_threadNameBuffer[GMDG_THREAD_NAME_MAX_LENGTH + 1] = {};
    thread_local uint32_t s_threadNameLength = 0;
    thread_local bool     s_threadNameIsSet = false;

    uint64_t GetTimeNanoseconds()
    {
        // std::chrono::system_clock::now() round-trips through winpthread's clock_gettime64
        // (FILETIME -> timespec -> ns, via two non-inlinable call hops) to reach this exact same
        // Windows API call. Calling it directly skips that and the redundant timespec conversion.
        FILETIME ft;
        GetSystemTimePreciseAsFileTime(&ft);

        const uint64_t ticksSince1601 = (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;

        // FILETIME is in 100ns intervals since 1601-01-01; convert to ns since the Unix epoch (1970-01-01).
        constexpr uint64_t EpochDifferenceTicks = 116444736000000000ULL;
        return (ticksSince1601 - EpochDifferenceTicks) * 100;
    }

    uint32_t GetThreadId()
    {
        return GetCurrentThreadId();
    }

    GMDGBool WriteLogFileHeader(std::FILE* t_file)
    {
        GMDG_ASSERT_WITH_MESSAGE(t_file, "t_file is null");

        GMDGLogFileHeader header{};
        std::memcpy(header.Magic, LogMagic.data(), sizeof(header.Magic));
        header.FormatVersion = LogFormatVersion;
        header.RecordHeaderSize = sizeof(GMDGLogFileHeader);
        header.Flags = 0;

        return std::fwrite(&header, sizeof(header), 1, t_file) == 1 ? GMDG_TRUE : GMDG_FALSE;
    }

    void WriterThreadLoop()
    {
        for (;;)
        {
            std::unique_lock<std::mutex> lock(s_bufferMutex);
            s_wakeCv.wait_for(lock, LogFlushInterval, [] {
                return !s_running.load(std::memory_order_acquire);
            });

            const size_t flushIndex = s_frontIndex;
            const size_t bytesToFlush = s_frontOffset;
            s_frontIndex = 1 - s_frontIndex;
            s_frontOffset = 0;

            const bool stillRunning = s_running.load(std::memory_order_acquire);
            lock.unlock();

            if (bytesToFlush > 0 && s_file)
            {
                std::fwrite(s_buffers[flushIndex].data(), 1, bytesToFlush, s_file);
                std::fflush(s_file);
            }

            if (!stillRunning) break;
        }
    }
}

extern "C" GMDGBool GMDG_Logger_Initialize(const char* t_path)
{
    GMDG_ASSERT_WITH_MESSAGE(t_path, "t_path is null");
    if (!t_path)
    {
        return GMDG_FALSE;
    }

    // lock for thread safeness
    std::lock_guard lock(s_mutex);

    if (s_running.load(std::memory_order_acquire))
    {
        GMDG_ASSERT_WITH_MESSAGE(false, "logger already initialized");
        return GMDG_FALSE;
    }

    // open/create the file in append (a), binary (b) and read/write mode (+)
    s_file = std::fopen(t_path, "ab+");
    if (!s_file) return GMDG_FALSE;

    // move at the end and check file size
    if (std::fseek(s_file, 0, SEEK_END) != 0)
    {
        std::fclose(s_file);
        s_file = nullptr;
        return GMDG_FALSE;
    }

    const long fileSize = std::ftell(s_file);
    if (fileSize < 0)
    {
        std::fclose(s_file);
        s_file = nullptr;
        return GMDG_FALSE;
    }

    if (fileSize == 0)
    {
        // if the file is new, the logger will write an header
        if (std::fseek(s_file, 0, SEEK_SET) != 0)
        {
            std::fclose(s_file);
            s_file = nullptr;
            return GMDG_FALSE;
        }

        if (!WriteLogFileHeader(s_file))
        {
            std::fclose(s_file);
            s_file = nullptr;
            return GMDG_FALSE;
        }
    }

    // move again to the end (after potentially writing the header)
    if (std::fseek(s_file, 0, SEEK_END) != 0)
    {
        std::fclose(s_file);
        s_file = nullptr;
        return GMDG_FALSE;
    }

    {
        // reset indicies
        std::lock_guard bufferLock(s_bufferMutex);
        s_frontIndex = 0;
        s_frontOffset = 0;
    }

    // std::memory_order_relaxed is used to assure atomicity only (CPU and compiler could reorder precedent instructions)
    s_droppedRecordCount.store(0, std::memory_order_relaxed);

    // std::memory_order_release is used to assure CPU and compiler won't reorder this and precedent write instructions
    s_running.store(true, std::memory_order_release);

    // start writer thread
    s_writerThread = std::thread(WriterThreadLoop);

    return GMDG_TRUE;
}

// NOTE: callers must stop invoking GMDG_Log on every other thread before calling Shutdown() -
// a log call racing with shutdown is not synchronized against the writer thread's teardown.
extern "C" void GMDG_Logger_Shutdown()
{
    // atomic change of s_running
    s_running.store(false, std::memory_order_release);

    // immediatly awake the thread (writer) waiting on s_wakeCv
    s_wakeCv.notify_one();

    // wait for the writer thread to finish
    if (s_writerThread.joinable())
    {
        s_writerThread.join();
    }

    std::lock_guard lock(s_mutex);

    if (s_file)
    {
        std::fclose(s_file);
        s_file = nullptr;
    }
}

extern "C" void GMDG_SetThreadName(const char* t_name, uint32_t t_nameLength)
{
    GMDG_ASSERT_WITH_MESSAGE(t_name != nullptr || t_nameLength == 0,
        "t_name is null but t_nameLength is {}", t_nameLength);

    const uint32_t clampedLength = t_nameLength < GMDG_THREAD_NAME_MAX_LENGTH
        ? t_nameLength
        : GMDG_THREAD_NAME_MAX_LENGTH;

    std::memcpy(s_threadNameBuffer, t_name, clampedLength);
    s_threadNameLength = clampedLength;
    s_threadNameIsSet = true;
}

extern "C" void GMDG_Log(
    uint32_t        t_severity,
    const char*     t_category,
    uint32_t        t_categoryLength,
    const char*     t_message,
    uint32_t        t_messageLength)
{
    GMDG_ASSERT_WITH_MESSAGE(t_category != nullptr || t_categoryLength == 0,
        "t_category is null but t_categoryLength is {}", t_categoryLength);
    GMDG_ASSERT_WITH_MESSAGE(t_message != nullptr || t_messageLength == 0,
        "t_message is null but t_messageLength is {}", t_messageLength);

    const uint32_t threadId = GetThreadId();

    if (!s_threadNameIsSet)
    {
        const auto result = std::format_to_n(s_threadNameBuffer, GMDG_THREAD_NAME_MAX_LENGTH, "Thread-{}", threadId);
        s_threadNameLength = static_cast<uint32_t>(result.out - s_threadNameBuffer);
        s_threadNameIsSet = true;
    }

    const size_t recordSize = sizeof(GMDGLogRecord) + s_threadNameLength + t_categoryLength + t_messageLength;

    std::lock_guard lock(s_bufferMutex);

    if (!s_file) return;

    auto& buffer = s_buffers[s_frontIndex];
    if (s_frontOffset + recordSize > buffer.size())
    {
        s_droppedRecordCount.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const GMDGLogRecord record{
        GetTimeNanoseconds(),
        threadId,
        t_severity,
        s_threadNameLength,
        t_categoryLength,
        t_messageLength
    };

    uint8_t* dest = buffer.data() + s_frontOffset;
    std::memcpy(dest, &record, sizeof(record));
    dest += sizeof(record);
    std::memcpy(dest, s_threadNameBuffer, s_threadNameLength);
    dest += s_threadNameLength;
    std::memcpy(dest, t_category, t_categoryLength);
    dest += t_categoryLength;
    std::memcpy(dest, t_message, t_messageLength);

    s_frontOffset += recordSize;
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
    return s_droppedRecordCount.load(std::memory_order_relaxed);
}

GMDGLogFileValidationResult GMDG_Logger_Validate_File_Header(const GMDGLogFileHeader* t_header)
{
    GMDG_ASSERT(t_header);

    if (std::memcmp(t_header->Magic, LogMagic.data(), LogMagic.size()) != 0)
    {
        return GMDG_INVALID_MAGIC;
    }

    if (t_header->FormatVersion != LogFormatVersion)
    {
        return GMDG_UNUPPORTED_VERSION;
    }

    if (t_header->RecordHeaderSize != sizeof(GMDGLogFileHeader))
    {
        return GMDG_UNUPPORTED_FILE_HEADER;
    }

    return GMDG_SUCCESS;
}
