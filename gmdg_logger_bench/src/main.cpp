#define GMDG_LOGGER_ENABLED
#include "gmdg_logger.hpp"

#include "gmdg_asserting.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <print>
#include <thread>
#include <vector>

namespace
{
    constexpr uint32_t WarmupIterations = 10000;

    // Individually timing a single LOG_INFO call is pointless on Windows: steady_clock wraps
    // QueryPerformanceCounter, which on most machines ticks once per 100 ns, so any call cheaper
    // than that rounds to 0 or 1 tick and the clock's own resolution becomes the whole signal.
    // Instead, time a batch of consecutive calls and divide by batch size, so the ~100 ns
    // quantization error is amortized across BatchSize calls rather than dominating a single one.
    constexpr uint32_t BatchSize = 1000;
    constexpr uint32_t BatchCount = 1000;

    constexpr uint32_t ThroughputMessagesPerThread = 50000;
    constexpr uint32_t ThreadCounts[] = { 1, 2, 4, 8 };

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

    // Single-threaded per-call latency of the logger's call site (lock + memcpy into the front
    // buffer), isolated from the background writer/flush cost — see the "How it works internally"
    // section of docs/gmdg_logger.typ.
    LatencyStats BenchmarkLoggerLatency()
    {
        GMDG_Logger_Initialize("bench_latency.log");

        for (uint32_t i = 0; i < WarmupIterations; ++i)
        {
            LOG_INFO("BENCH", "warmup message");
        }

        std::vector<double> samplesNs;
        samplesNs.reserve(BatchCount);
        for (uint32_t batch = 0; batch < BatchCount; ++batch)
        {
            const auto start = std::chrono::steady_clock::now();
            for (uint32_t i = 0; i < BatchSize; ++i)
            {
                LOG_INFO("BENCH", "latency message");
            }
            const auto end = std::chrono::steady_clock::now();
            const double batchNs = std::chrono::duration<double, std::nano>(end - start).count();
            samplesNs.push_back(batchNs / BatchSize);
        }

        GMDG_Logger_Shutdown();
        std::remove("bench_latency.log");

        return ComputeStats(samplesNs);
    }

    // Baseline: the cheapest realistic hand-rolled "logging" a caller could write without this
    // library — an fprintf under a mutex, no async writer. Gives LOG_INFO's latency a concrete
    // reference point instead of a bare, uncontextualized number.
    LatencyStats BenchmarkFprintfBaselineLatency()
    {
        std::FILE* file = std::fopen("bench_baseline.log", "wb");
        std::mutex fileMutex;

        for (uint32_t i = 0; i < WarmupIterations; ++i)
        {
            const std::lock_guard lock(fileMutex);
            std::fprintf(file, "[INFO][BENCH] warmup message\n");
        }

        std::vector<double> samplesNs;
        samplesNs.reserve(BatchCount);
        for (uint32_t batch = 0; batch < BatchCount; ++batch)
        {
            const auto start = std::chrono::steady_clock::now();
            for (uint32_t i = 0; i < BatchSize; ++i)
            {
                const std::lock_guard lock(fileMutex);
                std::fprintf(file, "[INFO][BENCH] latency message\n");
            }
            const auto end = std::chrono::steady_clock::now();
            const double batchNs = std::chrono::duration<double, std::nano>(end - start).count();
            samplesNs.push_back(batchNs / BatchSize);
        }

        std::fclose(file);
        std::remove("bench_baseline.log");

        return ComputeStats(samplesNs);
    }

    // Multi-threaded throughput: how many records/sec the logger absorbs from t_threadCount
    // producer threads before the background writer starts dropping records (see
    // GMDG_Logger_GetDroppedRecordCount in the library docs).
    void BenchmarkThroughput(const uint32_t t_threadCount)
    {
        GMDG_Logger_Initialize("bench_throughput.log");

        std::vector<std::thread> threads;
        threads.reserve(t_threadCount);

        const auto start = std::chrono::steady_clock::now();
        for (uint32_t i = 0; i < t_threadCount; ++i)
        {
            threads.emplace_back([]()
            {
                for (uint32_t i = 0; i < ThroughputMessagesPerThread; ++i)
                {
                    LOG_INFO("BENCH", "throughput message");
                }
            });
        }
        for (auto& thread : threads) thread.join();
        const auto end = std::chrono::steady_clock::now();

        GMDG_Logger_Shutdown();

        const double seconds = std::chrono::duration<double>(end - start).count();
        const uint64_t totalLogged = static_cast<uint64_t>(t_threadCount) * ThroughputMessagesPerThread;
        const uint64_t dropped = GMDG_Logger_GetDroppedRecordCount();

        std::println(
            "threads {:>2}   {:>12.0f} records/sec   dropped {:>6} / {:<6}",
            t_threadCount, static_cast<double>(totalLogged) / seconds, dropped, totalLogged);

        std::remove("bench_throughput.log");
    }
}

int main()
{
    std::println("== gmdg_logger benchmark ==");
    std::println("Run a Release build for meaningful numbers; a Debug build has no optimization and skewed results.\n");

    std::println("-- single-threaded call-site latency ({} batches x {} calls, {} warmup) --", BatchCount, BatchSize, WarmupIterations);
    PrintStats("LOG_INFO", BenchmarkLoggerLatency());
    PrintStats("fprintf baseline", BenchmarkFprintfBaselineLatency());

    std::println("\n-- multi-threaded throughput ({} messages/thread) --", ThroughputMessagesPerThread);
    for (const uint32_t threadCount : ThreadCounts)
    {
        BenchmarkThroughput(threadCount);
    }

    return 0;
}
