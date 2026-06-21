#pragma once

#include <vector>
#include <fstream>
#include <string>

namespace GMDGLoggerGUI
{  
    struct log_record_header
    {
        uint64_t timestamp_ns = 0;
        uint32_t thread_id    = 0;
        uint32_t level        = 0;
        uint32_t category_len = 0;
        uint32_t message_len  = 0;
    };

    struct LogRecord
    {
        uint64_t timestamp_ns   = 0;
        uint32_t thread_id   = 0;
        uint32_t severity       = 0;

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

        [[nodiscard]]
        inline const std::vector<LogRecord>& GetLogs() const { return mLogs; }

        inline void ShutdownApplication() { mIsApplicationRunning = false; }

        [[nodiscard]]
        inline bool IsApplicationRunning() const { return mIsApplicationRunning; }
        
    private:
        bool mIsApplicationRunning = true;
        
        std::ifstream mFile;
        std::vector<LogRecord> mLogs;

        [[nodiscard]]
        bool ReadRecord(LogRecord& record);
    };
}