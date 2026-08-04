#pragma once

// this file is intended to be included in C++ applications

#ifdef __cplusplus

#ifdef GMDG_LOGGER_ENABLED

#include "gmdg_logger.h"

namespace GMDGLogger
{
    template <size_t N1, size_t N2>
    inline void Log(
        GMDGLogSeverity t_level,
        const char (&t_category)[N1],
        const char (&t_message)[N2])
    {
        GMDG_Log(
            t_level,
            t_category,
            static_cast<uint32_t>(N1 - 1),
            t_message,
            static_cast<uint32_t>(N2 - 1));
    }
}

#define LOG_DEBUG(cat, msg) \
    ::GMDGLogger::Log(GMDG_LOG_DEBUG, cat, msg)

#define LOG_INFO(cat, msg) \
    ::GMDGLogger::Log(GMDG_LOG_INFO, cat, msg)

#define LOG_WARNING(cat, msg) \
    ::GMDGLogger::Log(GMDG_LOG_WARNING, cat, msg)

#define LOG_ERROR(cat, msg) \
    ::GMDGLogger::Log(GMDG_LOG_ERROR, cat, msg)

#else

#define LOG_DEBUG(cat, msg)
#define LOG_INFO(cat, msg)
#define LOG_WARNING(cat, msg)
#define LOG_ERROR(cat, msg)

#endif

#endif