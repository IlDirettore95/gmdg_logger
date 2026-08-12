#define GMDG_LOGGER_ENABLED
#include "gmdg_logger.hpp"

#include "mvc/view/table_layout.hpp"

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
            GMDG_LOG_INFO("TEST", "Async writer burst message");
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
            GMDG_LOG_VALIDATE_FILE_HEADER(&header) != GMDG_SUCCESS)
        {
            std::fclose(file);
            return 0;
        }

        size_t count = 0;
        GMDGLogRecord record{};
        while (std::fread(&record, sizeof(record), 1, file) == 1)
        {
            if (std::fseek(file, record.ThreadNameLength + record.CategoryLength + record.MessageLength, SEEK_CUR) != 0) break;
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

        if (!GMDG_LOG_INITIALIZE(kTestLogPath)) return false;

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

        GMDG_LOG_SHUTDOWN();

        const uint64_t dropped = GMDG_LOG_GET_DROPPED_RECORD_COUNT();
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
            GMDG_LOG_VALIDATE_FILE_HEADER(&header) != GMDG_SUCCESS)
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
                if (std::fseek(file, record.ThreadNameLength + record.CategoryLength, SEEK_CUR) != 0)
                {
                    std::fclose(file);
                    return {};
                }

                std::string message(record.MessageLength, '\0');
                const size_t bytesRead = std::fread(message.data(), 1, record.MessageLength, file);
                std::fclose(file);
                return (bytesRead == record.MessageLength) ? message : std::string{};
            }

            if (std::fseek(file, record.ThreadNameLength + record.CategoryLength + record.MessageLength, SEEK_CUR) != 0) break;
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
            GMDG_LOG_VALIDATE_FILE_HEADER(&header) != GMDG_SUCCESS)
        {
            std::fclose(file);
            return {};
        }

        std::vector<std::pair<uint32_t, std::string>> result;
        GMDGLogRecord record{};
        while (std::fread(&record, sizeof(record), 1, file) == 1)
        {
            std::string name(record.ThreadNameLength, '\0');
            if (std::fread(name.data(), 1, record.ThreadNameLength, file) != record.ThreadNameLength) break;
            if (std::fseek(file, record.CategoryLength + record.MessageLength, SEEK_CUR) != 0) break;
            result.emplace_back(record.ThreadId, std::move(name));
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

        if (!GMDG_LOG_INITIALIZE(path)) return false;

        std::vector<std::thread> threads;
        threads.reserve(kThreadCount);
        for (int i = 0; i < kThreadCount; ++i)
        {
            threads.emplace_back([i] {
                GMDG_LOG_SET_THREAD_NAME(std::format("WorkerPool-{}", i));
                LogBurst();
            });
        }
        for (auto& thread : threads)
        {
            thread.join();
        }

        GMDG_LOG_SHUTDOWN();

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

        if (!GMDG_LOG_INITIALIZE(path)) return false;

        const std::string longName(100, 'A');
        GMDG_LOG_SET_THREAD_NAME(longName);
        GMDG_LOG_INFO("TEST", "truncation test message");

        GMDG_LOG_SHUTDOWN();

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

        if (!GMDG_LOG_INITIALIZE(path)) return false;

        std::thread thread([] {
            GMDG_LOG_INFO("TEST", "fallback test message");
        });
        thread.join();

        GMDG_LOG_SHUTDOWN();

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

        if (!GMDG_LOG_INITIALIZE(path)) return false;

        GMDG_LOG_INFO("TEST", "value={} name={} pi={:.2f}", 42, "abc", 3.14159);

        GMDG_LOG_SHUTDOWN();

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

        if (!GMDG_LOG_INITIALIZE(path)) return false;

        GMDG_LOG_INFO("TEST", "int={} neg={} dbl={} str={} bool={} ch={}", 42, -7, 2.5, "hello", true, 'X');

        GMDG_LOG_SHUTDOWN();

        const std::string message = ReadRecordMessage(path, 0);
        std::remove(path);

        return message == "int=42 neg=-7 dbl=2.5 str=hello bool=true ch=X";
    }

    // Verifies the column-width floor is a true clamp: values already at or above the
    // minimum pass through unchanged -- this is the common case, a column not being
    // dragged small, and must not perturb a width the user hasn't touched.
    bool TestClampToMinWidthPassesThroughValuesAboveMinimum()
    {
        return GMDGLoggerGUI::TableLayout::ClampToMinWidth(200.0f, 150.0f) == 200.0f;
    }

    // Verifies a width dragged below the configured minimum is snapped exactly up to that
    // minimum -- this is the core behavior RenderTable() relies on to stop columns from
    // being resized down to a sliver.
    bool TestClampToMinWidthRaisesValuesBelowMinimum()
    {
        return GMDGLoggerGUI::TableLayout::ClampToMinWidth(10.0f, 150.0f) == 150.0f;
    }

    // Verifies the exact-equal boundary is treated as already satisfying the minimum (no
    // off-by-one clamp), matching the "> currentWidth + epsilon" comparison used at the
    // call site to avoid a snap-back feedback loop every frame.
    bool TestClampToMinWidthIsIdempotentAtExactBoundary()
    {
        return GMDGLoggerGUI::TableLayout::ClampToMinWidth(150.0f, 150.0f) == 150.0f;
    }

    // Verifies a single-line message (lineCount == 1) gets exactly one line of height plus
    // padding -- the "short-message rows stay compact" requirement, expressed as a
    // fabricated-input unit test independent of any live ImGui font measurement.
    bool TestComputeRowHeightForSingleLineMatchesLineHeightPlusPadding()
    {
        return GMDGLoggerGUI::TableLayout::ComputeRowHeight(1, 16.0f, 4.0f) == 20.0f;
    }

    // Verifies a multi-line message scales linearly with line count -- the "row grows to
    // fit the whole wrapped message" requirement, expressed as a fabricated-input unit test.
    bool TestComputeRowHeightForMultipleLinesScalesLinearly()
    {
        return GMDGLoggerGUI::TableLayout::ComputeRowHeight(5, 16.0f, 4.0f) == 84.0f; // 5 * 16 + 4
    }

    // Verifies a pathological lineCount of 0 (should never happen in production -- even an
    // empty message measures as 1 line -- but a defensive floor exists) still produces a
    // sane, non-zero row height rather than collapsing the row to just its padding.
    bool TestComputeRowHeightClampsNonPositiveLineCountToOne()
    {
        return GMDGLoggerGUI::TableLayout::ComputeRowHeight(0, 16.0f, 4.0f) == 20.0f; // treated as lineCount == 1
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

    if (!TestClampToMinWidthPassesThroughValuesAboveMinimum())
    {
        std::fprintf(stderr, "TestClampToMinWidthPassesThroughValuesAboveMinimum failed\n");
        return 1;
    }

    if (!TestClampToMinWidthRaisesValuesBelowMinimum())
    {
        std::fprintf(stderr, "TestClampToMinWidthRaisesValuesBelowMinimum failed\n");
        return 1;
    }

    if (!TestClampToMinWidthIsIdempotentAtExactBoundary())
    {
        std::fprintf(stderr, "TestClampToMinWidthIsIdempotentAtExactBoundary failed\n");
        return 1;
    }

    if (!TestComputeRowHeightForSingleLineMatchesLineHeightPlusPadding())
    {
        std::fprintf(stderr, "TestComputeRowHeightForSingleLineMatchesLineHeightPlusPadding failed\n");
        return 1;
    }

    if (!TestComputeRowHeightForMultipleLinesScalesLinearly())
    {
        std::fprintf(stderr, "TestComputeRowHeightForMultipleLinesScalesLinearly failed\n");
        return 1;
    }

    if (!TestComputeRowHeightClampsNonPositiveLineCountToOne())
    {
        std::fprintf(stderr, "TestComputeRowHeightClampsNonPositiveLineCountToOne failed\n");
        return 1;
    }

    return 0;
}
