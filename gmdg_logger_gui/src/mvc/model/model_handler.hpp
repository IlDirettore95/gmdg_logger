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
        uint32_t Id           = 0;
        uint64_t Timestamp_Ns = 0;
        uint32_t ThreadId     = 0;
        uint32_t Severity     = 0;

        std::string ThreadName;
        std::string Category;
        std::string Message;
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
        void ClearLoadedFile();

        [[nodiscard]]
        inline const std::vector<LogRecord>& GetLogs() const { return m_logs; }

        [[nodiscard]]
        inline const std::unordered_set<std::string>& GetThreadNames() const { return m_threadNameSet; }

        [[nodiscard]]
        inline const std::unordered_set<std::string>& GetCategories() const { return m_categorySet; }

        [[nodiscard]]
        inline GMDGLogFileValidationResult GetFileValidationError() const { return m_error; }

        [[nodiscard]]
        inline bool HasAttemptedLoad() const { return m_hasAttemptedLoad; }

        inline void ShutdownApplication() { m_isApplicationRunning = false; }

        [[nodiscard]]
        inline bool IsApplicationRunning() const { return m_isApplicationRunning; }

    private:
        bool m_isApplicationRunning = true;
        bool m_hasAttemptedLoad = false;
        uint32_t m_nextId = 1;

        std::ifstream m_file;
        std::vector<LogRecord> m_logs;
        std::unordered_set<std::string> m_threadNameSet;
        std::unordered_set<std::string> m_categorySet;
        GMDGLogFileValidationResult m_error = GMDG_UNKNOWN;

        [[nodiscard]]
        bool ReadRecord(LogRecord& t_record);
    };
}
