#pragma once

#include <vector>
#include <fstream>
#include <string>
#include <unordered_set>

#include "gmdg_logger.h"

namespace GMDGLoggerGUI
{  
    struct LogRecord
    {
        uint64_t timestamp_ns = 0;
        uint32_t thread_id    = 0;
        uint32_t severity     = 0;

        std::string thread_name;
        std::string category;
        std::string message;
    };

    class ModelHandler
    {
    public:
        ModelHandler() = default;
        ~ModelHandler() = default;

        void Initialize();
        void Update();
        void Shutdown();

        void LoadFile(const std::string& t_path);

        [[nodiscard]]
        inline const std::vector<LogRecord>& GetLogs() const { return mLogs; }

        [[nodiscard]]
        inline const std::unordered_set<std::string>& GetThreadNames() const { return mThreadNameSet; }

        [[nodiscard]]
        inline const std::unordered_set<std::string>& GetCategories() const { return mCategorySet; }

        [[nodiscard]]
        inline GMDGLogFileValidationResult GetFileValidationError() const { return mError; }

        [[nodiscard]]
        inline bool HasAttemptedLoad() const { return mHasAttemptedLoad; }

        inline void ShutdownApplication() { mIsApplicationRunning = false; }

        [[nodiscard]]
        inline bool IsApplicationRunning() const { return mIsApplicationRunning; }

    private:
        bool mIsApplicationRunning = true;
        bool mHasAttemptedLoad = false;

        std::ifstream mFile;
        std::vector<LogRecord> mLogs;
        std::unordered_set<std::string> mThreadNameSet;
        std::unordered_set<std::string> mCategorySet;
        GMDGLogFileValidationResult mError = GMDG_UNKNOWN;

        [[nodiscard]]
        bool ReadRecord(LogRecord& record);
    };
}