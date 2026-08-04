#define GMDG_LOGGER_ENABLED
#include "gmdg_logger.hpp"

#include <cstdio>
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
}

int main()
{
    if (!TestAsyncWriterAccountsForEveryRecord())
    {
        std::fprintf(stderr, "TestAsyncWriterAccountsForEveryRecord failed\n");
        return 1;
    }

    return 0;
}
