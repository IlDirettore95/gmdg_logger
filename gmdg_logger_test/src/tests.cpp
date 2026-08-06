#define GMDG_LOGGER_ENABLED
#include "gmdg_logger.hpp"

#include <cstdio>
#include <string>
#include <thread>
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
            if (std::fseek(file, record.category_len + record.message_len, SEEK_CUR) != 0) break;
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
                if (std::fseek(file, record.category_len, SEEK_CUR) != 0)
                {
                    std::fclose(file);
                    return {};
                }

                std::string message(record.message_len, '\0');
                const size_t bytesRead = std::fread(message.data(), 1, record.message_len, file);
                std::fclose(file);
                return (bytesRead == record.message_len) ? message : std::string{};
            }

            if (std::fseek(file, record.category_len + record.message_len, SEEK_CUR) != 0) break;
            ++currentIndex;
        }

        std::fclose(file);
        return {};
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
