#define GMDG_LOGGER_ENABLED
#include "gmdg_logger.hpp"

#include <cstdio>
#include <format>
#include <thread>
#include <vector>

namespace
{
    constexpr int kThreadCount = 4;
    constexpr int kMessagesPerThread = 20000;

    void LogBurst(int t_index)
    {
        LOG_SET_THREAD_NAME(std::format("WORKER-{}", t_index));

        for (int i = 0; i < kMessagesPerThread; ++i)
        {
            LOG_INFO("APPLICATION", "Burst log message");
        }
    }

    // Reads app.log sequentially and returns the number of well-formed records found.
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
}

int main()
{
    // app.log accumulates across runs (Initialize appends rather than truncating), so measure
    // this run's contribution as a delta rather than assuming the file starts empty.
    const size_t recordsBefore = CountRecords("app.log");

    GMDG_Logger_Initialize("app.log");

    LOG_SET_THREAD_NAME("MAIN");

    LOG_DEBUG("APPLICATION.PHYSICS", "This is a debug");
    LOG_INFO("APPLICATION.GRAPHICS", "This is an info");
    LOG_WARNING("APPLICATION.AI", "This is a warning");
    LOG_ERROR("APPLICATION.UI", "This is an error");

    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);
    for (int i = 0; i < kThreadCount; ++i)
    {
        threads.emplace_back(LogBurst, i);
    }
    for (auto& thread : threads)
    {
        thread.join();
    }

    GMDG_Logger_Shutdown();

    const uint64_t dropped = GMDG_Logger_GetDroppedRecordCount();
    const size_t totalLogged = 4 + static_cast<size_t>(kThreadCount) * kMessagesPerThread;
    const size_t recordsAfter = CountRecords("app.log");
    const size_t recordsWritten = recordsAfter - recordsBefore;

    std::printf(
        "logged: %zu, written: %zu, dropped: %llu\n",
        totalLogged,
        recordsWritten,
        static_cast<unsigned long long>(dropped));

    return (recordsWritten + dropped == totalLogged) ? 0 : 1;
}
