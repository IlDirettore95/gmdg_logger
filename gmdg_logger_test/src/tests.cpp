#define GMDG_LOGGER_ENABLED
#include "gmdg_logger.hpp"

#include <cstdio>
#include <format>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
    constexpr const char* kTestLogPath = "async_writer_test.log";
    constexpr int kThreadCount = 4;
    constexpr int kMessagesPerThread = 20000;

    void LogBurst()
    {
        for (int i = 0; i < kMessagesPerThread; ++i)
        {
            LOG_INFO("TEST", "Async writer burst message");
        }
    }

    // Reads a log file sequentially and returns the number of well-formed records found.
    // Returns 0 if the file doesn't exist yet or its header doesn't validate.
    size_t CountRecords(const char* path)
    {
        std::FILE* file = std::fopen(path, "rb");
        if (!file) return 0;

        GMDGLogFileHeader header{};
        if (std::fread(&header, sizeof(header), 1, file) != 1 ||
            GMDG_Logger_Validate_File_Header(&header) != GMDG_SUCCESS)
        {
            std::fclose(file);
            return 0;
        }

        size_t count = 0;
        GMDGLogRecord record{};
        while (std::fread(&record, sizeof(record), 1, file) == 1)
        {
            if (std::fseek(file, record.thread_name_len + record.category_len + record.message_len, SEEK_CUR) != 0) break;
            ++count;
        }

        std::fclose(file);
        return count;
    }

    // Verifies that logging from multiple threads while the async writer is running never
    // corrupts the file, and that every record is either written or accounted for as dropped
    // (the buffer-full overflow policy is drop-and-count, not block).
    bool TestAsyncWriterAccountsForEveryRecord()
    {
        const size_t recordsBefore = CountRecords(kTestLogPath);

        if (!GMDG_Logger_Initialize(kTestLogPath)) return false;

        std::vector<std::thread> threads;
        threads.reserve(kThreadCount);
        for (int i = 0; i < kThreadCount; ++i)
        {
            threads.emplace_back(LogBurst);
        }
        for (auto& thread : threads)
        {
            thread.join();
        }

        GMDG_Logger_Shutdown();

        const uint64_t dropped = GMDG_Logger_GetDroppedRecordCount();
        const size_t totalLogged = static_cast<size_t>(kThreadCount) * kMessagesPerThread;
        const size_t recordsWritten = CountRecords(kTestLogPath) - recordsBefore;

        return recordsWritten + dropped == totalLogged;
    }

    // Reads the record at t_index (0-based, write order) and returns its message bytes, or an
    // empty string if the file/header/index is invalid.
    std::string ReadRecordMessage(const char* path, size_t t_index)
    {
        std::FILE* file = std::fopen(path, "rb");
        if (!file) return {};

        GMDGLogFileHeader header{};
        if (std::fread(&header, sizeof(header), 1, file) != 1 ||
            GMDG_Logger_Validate_File_Header(&header) != GMDG_SUCCESS)
        {
            std::fclose(file);
            return {};
        }

        size_t currentIndex = 0;
        GMDGLogRecord record{};
        while (std::fread(&record, sizeof(record), 1, file) == 1)
        {
            if (currentIndex == t_index)
            {
                if (std::fseek(file, record.thread_name_len + record.category_len, SEEK_CUR) != 0)
                {
                    std::fclose(file);
                    return {};
                }

                std::string message(record.message_len, '\0');
                const size_t bytesRead = std::fread(message.data(), 1, record.message_len, file);
                std::fclose(file);
                return (bytesRead == record.message_len) ? message : std::string{};
            }

            if (std::fseek(file, record.thread_name_len + record.category_len + record.message_len, SEEK_CUR) != 0) break;
            ++currentIndex;
        }

        std::fclose(file);
        return {};
    }

    // Reads every record's (thread_id, thread_name) pair in write order.
    std::vector<std::pair<uint32_t, std::string>> ReadThreadIdentities(const char* path)
    {
        std::FILE* file = std::fopen(path, "rb");
        if (!file) return {};

        GMDGLogFileHeader header{};
        if (std::fread(&header, sizeof(header), 1, file) != 1 ||
            GMDG_Logger_Validate_File_Header(&header) != GMDG_SUCCESS)
        {
            std::fclose(file);
            return {};
        }

        std::vector<std::pair<uint32_t, std::string>> result;
        GMDGLogRecord record{};
        while (std::fread(&record, sizeof(record), 1, file) == 1)
        {
            std::string name(record.thread_name_len, '\0');
            if (std::fread(name.data(), 1, record.thread_name_len, file) != record.thread_name_len) break;
            if (std::fseek(file, record.category_len + record.message_len, SEEK_CUR) != 0) break;
            result.emplace_back(record.thread_id, std::move(name));
        }

        std::fclose(file);
        return result;
    }

    // Verifies that a pool of worker threads each naming itself via LOG_SET_THREAD_NAME produces
    // records carrying the right name per thread, and that within a single run each OS thread id
    // maps to exactly one name (all threads are joined before this is checked, so no two
    // concurrently-live threads can share a recycled OS id - this is exactly why thread names are
    // stored per-record rather than in a thread_id-keyed side table, which would NOT be safe
    // across separate runs/threads whose ids get reused over the process lifetime).
    bool TestThreadNamesRoundTripPerRecord()
    {
        constexpr const char* path = "thread_names_test.log";
        std::remove(path);

        if (!GMDG_Logger_Initialize(path)) return false;

        std::vector<std::thread> threads;
        threads.reserve(kThreadCount);
        for (int i = 0; i < kThreadCount; ++i)
        {
            threads.emplace_back([i] {
                LOG_SET_THREAD_NAME(std::format("WorkerPool-{}", i));
                LogBurst();
            });
        }
        for (auto& thread : threads)
        {
            thread.join();
        }

        GMDG_Logger_Shutdown();

        const auto identities = ReadThreadIdentities(path);
        std::remove(path);

        std::unordered_set<std::string> distinctNames;
        std::unordered_map<uint32_t, std::string> idToName;
        for (const auto& [threadID, name] : identities)
        {
            distinctNames.insert(name);

            auto it = idToName.find(threadID);
            if (it == idToName.end())
            {
                idToName.emplace(threadID, name);
            }
            else if (it->second != name)
            {
                return false;
            }
        }

        if (distinctNames.size() != static_cast<size_t>(kThreadCount)) return false;

        for (int i = 0; i < kThreadCount; ++i)
        {
            if (!distinctNames.contains(std::format("WorkerPool-{}", i))) return false;
        }

        return true;
    }

    // Verifies a thread name longer than GMDG_THREAD_NAME_MAX_LENGTH is silently truncated rather
    // than overflowing the fixed thread-local buffer or corrupting the record.
    bool TestThreadNameTruncatesAtBoundary()
    {
        constexpr const char* path = "thread_name_truncation_test.log";
        std::remove(path);

        if (!GMDG_Logger_Initialize(path)) return false;

        const std::string longName(100, 'A');
        LOG_SET_THREAD_NAME(longName);
        LOG_INFO("TEST", "truncation test message");

        GMDG_Logger_Shutdown();

        const auto identities = ReadThreadIdentities(path);
        std::remove(path);

        if (identities.size() != 1) return false;

        const std::string expected = longName.substr(0, GMDG_THREAD_NAME_MAX_LENGTH);
        return identities[0].second == expected;
    }

    // Verifies a thread that never calls LOG_SET_THREAD_NAME still gets a usable, deterministic
    // "Thread-<id>" label instead of an empty/unnamed record.
    bool TestThreadNameFallsBackToThreadId()
    {
        constexpr const char* path = "thread_name_fallback_test.log";
        std::remove(path);

        if (!GMDG_Logger_Initialize(path)) return false;

        std::thread thread([] {
            LOG_INFO("TEST", "fallback test message");
        });
        thread.join();

        GMDG_Logger_Shutdown();

        const auto identities = ReadThreadIdentities(path);
        std::remove(path);

        if (identities.size() != 1) return false;

        const auto& [threadID, name] = identities[0];
        return name == std::format("Thread-{}", threadID);
    }

    // Verifies a formatted LOG_INFO call round-trips through the writer and back out to disk with
    // the same bytes std::format would have produced directly. The "{:.2f}" spec on the third
    // placeholder makes gmdg_logger.hpp's fast path bail out immediately (it only handles bare
    // "{}"), so this specifically exercises the std::format_to_n fallback path end to end.
    bool TestFormattedMessageRoundTrips()
    {
        constexpr const char* path = "formatted_roundtrip_test.log";
        std::remove(path);

        if (!GMDG_Logger_Initialize(path)) return false;

        LOG_INFO("TEST", "value={} name={} pi={:.2f}", 42, "abc", 3.14159);

        GMDG_Logger_Shutdown();

        const std::string message = ReadRecordMessage(path, 0);
        std::remove(path);

        return message == "value=42 name=abc pi=3.14";
    }

    // Same idea, but every placeholder is a bare "{}" with an AppendArg-supported type, so this
    // one actually exercises gmdg_logger.hpp's hand-rolled fast path (int, negative int, double,
    // string, bool, char) rather than falling back to std::format_to_n like the test above does.
    bool TestFastPathFormattedMessageRoundTrips()
    {
        constexpr const char* path = "fast_path_roundtrip_test.log";
        std::remove(path);

        if (!GMDG_Logger_Initialize(path)) return false;

        LOG_INFO("TEST", "int={} neg={} dbl={} str={} bool={} ch={}", 42, -7, 2.5, "hello", true, 'X');

        GMDG_Logger_Shutdown();

        const std::string message = ReadRecordMessage(path, 0);
        std::remove(path);

        return message == "int=42 neg=-7 dbl=2.5 str=hello bool=true ch=X";
    }
}

int main()
{
    if (!TestAsyncWriterAccountsForEveryRecord())
    {
        std::fprintf(stderr, "TestAsyncWriterAccountsForEveryRecord failed\n");
        return 1;
    }

    if (!TestThreadNamesRoundTripPerRecord())
    {
        std::fprintf(stderr, "TestThreadNamesRoundTripPerRecord failed\n");
        return 1;
    }

    if (!TestThreadNameTruncatesAtBoundary())
    {
        std::fprintf(stderr, "TestThreadNameTruncatesAtBoundary failed\n");
        return 1;
    }

    if (!TestThreadNameFallsBackToThreadId())
    {
        std::fprintf(stderr, "TestThreadNameFallsBackToThreadId failed\n");
        return 1;
    }

    if (!TestFormattedMessageRoundTrips())
    {
        std::fprintf(stderr, "TestFormattedMessageRoundTrips failed\n");
        return 1;
    }

    if (!TestFastPathFormattedMessageRoundTrips())
    {
        std::fprintf(stderr, "TestFastPathFormattedMessageRoundTrips failed\n");
        return 1;
    }

    return 0;
}
