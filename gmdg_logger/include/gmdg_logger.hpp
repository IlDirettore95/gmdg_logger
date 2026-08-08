#pragma once

// this file is intended to be included in C++ applications

#ifdef __cplusplus

#ifdef GMDG_LOGGER_ENABLED

#include "gmdg_logger.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <string_view>
#include <type_traits>
#include <utility>

namespace GMDGLogger
{
    // Stack buffer the formatted message is built into before being handed to GMDG_Log - keeps
    // the call site allocation-free even for formatted messages. Output longer than this is
    // truncated by std::format_to_n (it never overruns the buffer); GMDG_Log only ever sees the
    // bytes actually written.
    inline constexpr size_t MessageBufferSize = 1024;

    // Converts one argument to text for the fast formatting path below. Returns the number of
    // bytes written (0 is a valid result for an empty string argument), or SIZE_MAX if this type
    // isn't supported or the value doesn't fit in the remaining capacity - either way the caller
    // falls back to std::format_to_n, so this never needs to be exhaustive.
    template <typename T>
    inline size_t AppendArg(char* t_dest, size_t t_capacity, const T& t_value)
    {
        if constexpr (std::is_same_v<std::decay_t<T>, bool>)
        {
            // std::format prints bool as "true"/"false", not std::to_chars's "1"/"0".
            const std::string_view text = t_value ? "true" : "false";
            if (text.size() > t_capacity) return SIZE_MAX;
            std::memcpy(t_dest, text.data(), text.size());
            return text.size();
        }
        else if constexpr (std::is_same_v<std::decay_t<T>, char>)
        {
            // std::format prints plain `char` as the literal character, not its numeric value.
            // signed char/unsigned char/int8_t/uint8_t are NOT char (std::format formats those
            // numerically), so this must stay an exact type match, not an is_arithmetic check.
            if (t_capacity < 1) return SIZE_MAX;
            *t_dest = t_value;
            return 1;
        }
        else if constexpr (std::is_arithmetic_v<std::decay_t<T>>)
        {
            const auto result = std::to_chars(t_dest, t_dest + t_capacity, t_value);
            return (result.ec == std::errc{}) ? static_cast<size_t>(result.ptr - t_dest) : SIZE_MAX;
        }
        else if constexpr (std::is_convertible_v<T, std::string_view>)
        {
            const std::string_view text = t_value;
            if (text.size() > t_capacity) return SIZE_MAX;
            std::memcpy(t_dest, text.data(), text.size());
            return text.size();
        }
        else
        {
            return SIZE_MAX;
        }
    }

    // Fast path for "{}"-only placeholders with AppendArg-supported argument types. Recurses one
    // placeholder/argument at a time; t_fmt is the *remaining* unscanned format text. Bails out
    // (returns false) on anything outside that subset - a format spec, an escaped brace, an
    // unsupported argument type, or a placeholder/argument count mismatch - and the caller falls
    // back to std::format_to_n, which is always valid for anything std::format_string accepted at
    // compile time. Because of that fallback, a bug here can only make a call slower, never wrong.
    template <typename First, typename... Rest>
    inline bool TryFastFormatImpl(
        char* t_dest,
        size_t t_capacity,
        std::string_view t_fmt,
        size_t& t_written,
        const First& t_first,
        const Rest&... t_rest)
    {
        const size_t brace = t_fmt.find('{');
        if (brace == std::string_view::npos) return false;                    // more args than placeholders
        if (brace + 1 >= t_fmt.size() || t_fmt[brace + 1] != '}') return false; // spec or escaped brace

        if (brace > t_capacity - t_written) return false;
        std::memcpy(t_dest + t_written, t_fmt.data(), brace);
        t_written += brace;

        const size_t argLength = AppendArg(t_dest + t_written, t_capacity - t_written, t_first);
        if (argLength == SIZE_MAX) return false;
        t_written += argLength;

        if constexpr (sizeof...(Rest) > 0)
        {
            return TryFastFormatImpl(t_dest, t_capacity, t_fmt.substr(brace + 2), t_written, t_rest...);
        }
        else
        {
            const std::string_view tail = t_fmt.substr(brace + 2);
            if (tail.find('{') != std::string_view::npos) return false;       // more placeholders than args
            if (tail.size() > t_capacity - t_written) return false;
            std::memcpy(t_dest + t_written, tail.data(), tail.size());
            t_written += tail.size();
            return true;
        }
    }

    // Tags the calling thread with a runtime name (unlike category, this can't be a compile-time
    // literal - worker threads are spawned dynamically). Truncated to GMDG_THREAD_NAME_MAX_LENGTH
    // bytes if longer. If never called, logged records fall back to "Thread-<id>".
    inline void SetThreadName(std::string_view t_name)
    {
        GMDG_SetThreadName(t_name.data(), static_cast<uint32_t>(t_name.size()));
    }

    template <size_t N1, typename... Args>
    inline void Log(
        GMDGLogSeverity t_level,
        const char (&t_category)[N1],
        std::format_string<Args...> t_format,
        Args&&... t_args)
    {
        if constexpr (sizeof...(Args) == 0)
        {
            // No formatting work at all - the literal is already the final text. Matches what
            // every LOG_* call site compiled to before formatting support was added.
            const std::string_view text = t_format.get();
            GMDG_Log(t_level, t_category, static_cast<uint32_t>(N1 - 1), text.data(), static_cast<uint32_t>(text.size()));
        }
        else
        {
            char buffer[MessageBufferSize];
            size_t written = 0;
            const bool fastPathOk = TryFastFormatImpl(buffer, MessageBufferSize, t_format.get(), written, t_args...);

            uint32_t length;
            if (fastPathOk)
            {
                length = static_cast<uint32_t>(written);
            }
            else
            {
                const auto result = std::format_to_n(buffer, sizeof(buffer), t_format, std::forward<Args>(t_args)...);
                length = static_cast<uint32_t>(result.out - buffer);
            }

            GMDG_Log(t_level, t_category, static_cast<uint32_t>(N1 - 1), buffer, length);
        }
    }
}

#define LOG_DEBUG(cat, fmt, ...) \
    ::GMDGLogger::Log(GMDG_LOG_DEBUG, cat, fmt __VA_OPT__(,) __VA_ARGS__)

#define LOG_INFO(cat, fmt, ...) \
    ::GMDGLogger::Log(GMDG_LOG_INFO, cat, fmt __VA_OPT__(,) __VA_ARGS__)

#define LOG_WARNING(cat, fmt, ...) \
    ::GMDGLogger::Log(GMDG_LOG_WARNING, cat, fmt __VA_OPT__(,) __VA_ARGS__)

#define LOG_ERROR(cat, fmt, ...) \
    ::GMDGLogger::Log(GMDG_LOG_ERROR, cat, fmt __VA_OPT__(,) __VA_ARGS__)

#define LOG_SET_THREAD_NAME(name) \
    ::GMDGLogger::SetThreadName(name)

#else

#define LOG_DEBUG(cat, fmt, ...)
#define LOG_INFO(cat, fmt, ...)
#define LOG_WARNING(cat, fmt, ...)
#define LOG_ERROR(cat, fmt, ...)
#define LOG_SET_THREAD_NAME(name)

#endif

#endif