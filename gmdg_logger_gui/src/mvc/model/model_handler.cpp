#include "pch.h"

#include "model_handler.hpp"

#include "gmdg_logger.h"

using namespace GMDGLoggerGUI;

void ModelHandler::Initialize()
{
    m_error = GMDG_UNKNOWN;
}

void ModelHandler::Update()
{
    if (m_error != GMDG_SUCCESS) return;

    LogRecord record{};

    while (ReadRecord(record))
    {
        m_logs.emplace_back(std::move(record));
        m_threadNameSet.emplace(m_logs.back().ThreadName);
        m_categorySet.emplace(m_logs.back().Category);

        // std::println("[{}] [{}] [{}] : {}",
        //              m_logs.back().ThreadId,
        //              m_logs.back().Category,
        //              m_logs.back().Severity,
        //              m_logs.back().Message);
    }

    m_file.clear();
}

void ModelHandler::Shutdown() {}

void ModelHandler::LoadFile(const std::string& t_path)
{
    GMDG_ASSERT(!t_path.empty());

    if (m_file.is_open())
    {
        m_file.close();
    }
    m_file.clear();

    m_logs.clear();
    m_threadNameSet.clear();
    m_categorySet.clear();
    m_error = GMDG_UNKNOWN;
    m_hasAttemptedLoad = true;

    m_file.open(t_path, std::ios::binary);

    if (!m_file)
    {
        return;
    }

    GMDGLogFileHeader header{};

    if (!m_file.read(reinterpret_cast<char*>(&header), sizeof(header)))
    {
        return;
    }

    m_error = GMDG_Logger_Validate_File_Header(&header);
}

bool ModelHandler::ReadRecord(LogRecord& t_record)
{
    static uint32_t s_nextId = 1;

    GMDG_ASSERT(m_file.is_open());

    GMDGLogRecord record{};

    if (!m_file.read(reinterpret_cast<char*>(&record), sizeof(record)))
    {
        return false;
    }

    t_record.Id           = s_nextId++;
    t_record.Timestamp_Ns = record.timestamp_ns;
    t_record.ThreadId     = record.thread_id;
    t_record.Severity     = record.severity;

    t_record.ThreadName.resize(record.thread_name_len);
    t_record.Category.resize(record.category_len);
    t_record.Message.resize(record.message_len);

    if (!m_file.read(t_record.ThreadName.data(), static_cast<std::streamsize>(record.thread_name_len)))
    {
        return false;
    }

    if (!m_file.read(t_record.Category.data(), static_cast<std::streamsize>(record.category_len)))
    {
        return false;
    }

    if (!m_file.read(t_record.Message.data(), static_cast<std::streamsize>(record.message_len)))
    {
        return false;
    }

    return true;
}
