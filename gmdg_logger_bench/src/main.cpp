#include "gmdg_logger.h"

#define GMDG_LOGGER_ENABLED
#include "gmdg_logger.hpp"

#include "backends/backend.hpp"
#include "backends/async_backend.hpp"
#include "backends/sync_backend.hpp"
#include "backends/fprintf_backend.hpp"

#include "gmdg_asserting.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <print>
#include <thread>
#include <vector>

namespace
{
    constexpr uint32_t WarmupIterations = 10000;

    // Individually timing a single Log() call is pointless on Windows: steady_clock wraps
    // QueryPerformanceCounter, which on most machines ticks once per 100 ns, so any call cheaper
    // than that rounds to 0 or 1 tick and the clock's own resolution becomes the whole signal.
    // Instead, time a batch of consecutive calls and divide by batch size, so the ~100 ns
    // quantization error is amortized across BatchSize calls rather than dominating a single one.
    constexpr uint32_t BatchSize = 1000;
    constexpr uint32_t BatchCount = 1000;

    constexpr uint32_t ThroughputMessagesPerThread = 50000;
    constexpr uint32_t ThreadCounts[] = { 1, 2, 4, 8 };

    // Pairs a backend with the temp file paths its own latency/throughput runs use, so backends
    // never clobber each other's files. Adding a future backend to the comparison is: implement
    // LoggerBackend in backends/, then add one line here.
    struct BenchmarkTarget
    {
        const LoggerBackend& Backend;
        const char* LatencyPath;
        const char* ThroughputPath;
    };

    const BenchmarkTarget Targets[] = {
        { AsyncBackend,   "bench_latency_async.log",   "bench_throughput_async.log" },
        { SyncBackend,    "bench_latency_sync.log",    "bench_throughput_sync.log" },
        { FprintfBackend, "bench_latency_fprintf.log", "bench_throughput_fprintf.log" },
    };

    template <size_t N1, size_t N2>
    void BackendLog(const LoggerBackend& t_backend, GMDGLogSeverity t_severity, const char (&t_category)[N1], const char (&t_message)[N2])
    {
        t_backend.Log(t_severity, t_category, static_cast<uint32_t>(N1 - 1), t_message, static_cast<uint32_t>(N2 - 1));
    }

    // Calls GMDG_Log directly with compile-time literal lengths, same shape as the pre-formatting
    // GMDGLogger::Log overload gmdg_logger.hpp used to have. Used as the "raw" baseline in the
    // formatting-overhead section below so it's compared against LOG_INFO at the same call depth
    // (both reach GMDG_Log directly, no LoggerBackend function-pointer hop either side).
    template <size_t N1, size_t N2>
    void RawGmdgLog(GMDGLogSeverity t_severity, const char (&t_category)[N1], const char (&t_message)[N2])
    {
        GMDG_Log(t_severity, t_category, static_cast<uint32_t>(N1 - 1), t_message, static_cast<uint32_t>(N2 - 1));
    }

    struct LatencyStats
    {
        double MinNs;
        double MedianNs;
        double P90Ns;
        double P99Ns;
        double MaxNs;
        double MeanNs;
    };

    LatencyStats ComputeStats(std::vector<double>& t_samplesNs)
    {
        GMDG_ASSERT_WITH_MESSAGE(!t_samplesNs.empty(), "ComputeStats called with no samples");

        std::sort(t_samplesNs.begin(), t_samplesNs.end());

        const size_t count = t_samplesNs.size();
        double sum = 0.0;
        for (const double sample : t_samplesNs) sum += sample;

        return LatencyStats{
            .MinNs    = t_samplesNs.front(),
            .MedianNs = t_samplesNs[count / 2],
            .P90Ns    = t_samplesNs[static_cast<size_t>(count * 0.90)],
            .P99Ns    = t_samplesNs[static_cast<size_t>(count * 0.99)],
            .MaxNs    = t_samplesNs.back(),
            .MeanNs   = sum / static_cast<double>(count),
        };
    }

    void PrintStats(const char* t_label, const LatencyStats& t_stats)
    {
        std::println(
            "{:<28} min {:>8.1f} ns   median {:>8.1f} ns   p90 {:>8.1f} ns   p99 {:>8.1f} ns   max {:>10.1f} ns   mean {:>8.1f} ns",
            t_label, t_stats.MinNs, t_stats.MedianNs, t_stats.P90Ns, t_stats.P99Ns, t_stats.MaxNs, t_stats.MeanNs);
    }

    // Single-threaded per-call latency of a backend's call site, isolated from any background
    // writer/flush cost — see the "How it works internally" section of docs/gmdg_logger.typ.
    // t_logOnce(callIndex) performs exactly one log call; parametrized so this same warmup/timing
    // scaffolding is reused for both the plain-literal lanes and the formatting-overhead lanes.
    template <typename LogOnceFn>
    LatencyStats BenchmarkLatency(const LoggerBackend& t_backend, const char* t_path, LogOnceFn&& t_logOnce)
    {
        t_backend.Initialize(t_path);

        for (uint32_t i = 0; i < WarmupIterations; ++i)
        {
            t_logOnce(i);
        }

        std::vector<double> samplesNs;
        samplesNs.reserve(BatchCount);
        for (uint32_t batch = 0; batch < BatchCount; ++batch)
        {
            const auto start = std::chrono::steady_clock::now();
            for (uint32_t i = 0; i < BatchSize; ++i)
            {
                t_logOnce(i);
            }
            const auto end = std::chrono::steady_clock::now();
            const double batchNs = std::chrono::duration<double, std::nano>(end - start).count();
            samplesNs.push_back(batchNs / BatchSize);
        }

        t_backend.Shutdown();
        std::remove(t_path);

        return ComputeStats(samplesNs);
    }

    // Multi-threaded throughput: how many records/sec a backend absorbs from t_threadCount
    // producer threads. Only backends with a bounded async buffer (currently just AsyncBackend)
    // can report a nonzero dropped count - the others block the caller instead of dropping, so
    // their dropped count is always 0 by design, not because they "kept up" better.
    template <typename LogOnceFn>
    void BenchmarkThroughput(const LoggerBackend& t_backend, const char* t_path, const uint32_t t_threadCount, const char* t_label, LogOnceFn&& t_logOnce)
    {
        t_backend.Initialize(t_path);

        std::vector<std::thread> threads;
        threads.reserve(t_threadCount);

        const auto start = std::chrono::steady_clock::now();
        for (uint32_t i = 0; i < t_threadCount; ++i)
        {
            threads.emplace_back([&t_logOnce]()
            {
                for (uint32_t i = 0; i < ThroughputMessagesPerThread; ++i)
                {
                    t_logOnce(i);
                }
            });
        }
        for (auto& thread : threads) thread.join();
        const auto end = std::chrono::steady_clock::now();

        t_backend.Shutdown();

        const double seconds = std::chrono::duration<double>(end - start).count();
        const uint64_t totalLogged = static_cast<uint64_t>(t_threadCount) * ThroughputMessagesPerThread;
        const uint64_t dropped = t_backend.GetDroppedRecordCount();

        std::println(
            "{:<24} threads {:>2}   {:>12.0f} records/sec   dropped {:>6} / {:<6}",
            t_label, t_threadCount, static_cast<double>(totalLogged) / seconds, dropped, totalLogged);

        std::remove(t_path);
    }
}

int main()
{
    std::println("== gmdg_logger benchmark ==");
    std::println("Run a Release build for meaningful numbers; a Debug build has no optimization and skewed results.\n");

    std::println("-- single-threaded call-site latency ({} batches x {} calls, {} warmup) --", BatchCount, BatchSize, WarmupIterations);
    for (const BenchmarkTarget& target : Targets)
    {
        PrintStats(target.Backend.Name, BenchmarkLatency(target.Backend, target.LatencyPath,
            [&target](uint32_t)
            {
                BackendLog(target.Backend, GMDG_LOG_INFO, "BENCH", "latency message");
            }));
    }

    std::println("\n-- multi-threaded throughput ({} messages/thread) --", ThroughputMessagesPerThread);
    std::println("(dropped count is only meaningful for the async backend - others block instead of dropping)");
    for (const uint32_t threadCount : ThreadCounts)
    {
        for (const BenchmarkTarget& target : Targets)
        {
            BenchmarkThroughput(target.Backend, target.ThroughputPath, threadCount, target.Backend.Name,
                [&target](uint32_t)
                {
                    BackendLog(target.Backend, GMDG_LOG_INFO, "BENCH", "throughput message");
                });
        }
    }

    // Isolates the gmdg_logger.hpp formatting layer's own cost from the write-mechanism comparison
    // above. All three lanes reach GMDG_Log at the same call depth (no LoggerBackend indirection
    // either side), so any delta is attributable to std::format_to_n, not indirection differences:
    //   raw              - RawGmdgLog: compile-time literal length, no std::format_to_n at all
    //                      (what every LOG_* call site used to compile to before unification)
    //   formatted, 0 args - LOG_INFO with a static format string and no placeholders (isolates the
    //                      pure std::format_to_n overhead added over the old literal path)
    //   formatted, 2 args - LOG_INFO with real per-call dynamic content (actual formatting cost)
    std::println("\n-- formatting overhead (gmdg_logger.hpp, AsyncBackend only) --");

    PrintStats("async (raw)", BenchmarkLatency(AsyncBackend, "bench_latency_fmt_raw.log",
        [](uint32_t)
        {
            RawGmdgLog(GMDG_LOG_INFO, "BENCH", "latency message");
        }));
    PrintStats("async (formatted, 0 args)", BenchmarkLatency(AsyncBackend, "bench_latency_fmt_0.log",
        [](uint32_t)
        {
            GMDG_LOG_INFO("BENCH", "latency message");
        }));
    PrintStats("async (formatted, 2 args)", BenchmarkLatency(AsyncBackend, "bench_latency_fmt_2.log",
        [](uint32_t i)
        {
            GMDG_LOG_INFO("BENCH", "latency message {} val {}", i, i * 2);
        }));

    std::println();
    for (const uint32_t threadCount : ThreadCounts)
    {
        BenchmarkThroughput(AsyncBackend, "bench_throughput_fmt_raw.log", threadCount, "async (raw)",
            [](uint32_t)
            {
                RawGmdgLog(GMDG_LOG_INFO, "BENCH", "throughput message");
            });
        BenchmarkThroughput(AsyncBackend, "bench_throughput_fmt_0.log", threadCount, "async (formatted, 0 args)",
            [](uint32_t)
            {
                GMDG_LOG_INFO("BENCH", "throughput message");
            });
        BenchmarkThroughput(AsyncBackend, "bench_throughput_fmt_2.log", threadCount, "async (formatted, 2 args)",
            [](uint32_t i)
            {
                GMDG_LOG_INFO("BENCH", "throughput message {} val {}", i, i * 2);
            });
    }

    return 0;
}
