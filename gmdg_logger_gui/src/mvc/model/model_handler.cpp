#include "pch.h"

#include "model_handler.hpp"

#include "gmdg_logger.h"

using namespace GMDGLoggerGUI;

void ModelHandler::Initialize()
{
    mError = GMDG_UNKNOWN;
}

void ModelHandler::Update()
{
    if (mError != GMDG_SUCCESS) return;

    LogRecord record{};

    while (ReadRecord(record))
    {
        mLogs.emplace_back(std::move(record));
        mThreadIDSet.emplace(mLogs.back().thread_id);

        // std::println("[{}] [{}] [{}] : {}",
        //              mLogs.back().thread_id,
        //              mLogs.back().category,
        //              mLogs.back().severity,
        //              mLogs.back().message);
    }

    mFile.clear();
}

void ModelHandler::Shutdown() {}

void ModelHandler::LoadFile(const std::string& t_path)
{
    if (mFile.is_open())
    {
        mFile.close();
    }
    mFile.clear();

    mLogs.clear();
    mThreadIDSet.clear();
    mError = GMDG_UNKNOWN;
    mHasAttemptedLoad = true;

    mFile.open(t_path, std::ios::binary);

    if (!mFile)
    {
        return;
    }

    GMDGLogFileHeader header{};

    if (!mFile.read(reinterpret_cast<char*>(&header), sizeof(header)))
    {
        return;
    }

    mError = GMDG_Logger_Validate_File_Header(&header);
}

bool ModelHandler::ReadRecord(LogRecord& t_record)
{
    GMDGLogRecord record{};

    if (!mFile.read(reinterpret_cast<char*>(&record), sizeof(record)))
    {
        return false;
    }

    t_record.timestamp_ns = record.timestamp_ns;
    t_record.thread_id    = record.thread_id;
    t_record.severity     = record.severity;

    t_record.category.resize(record.category_len);
    t_record.message.resize(record.message_len);

    if (!mFile.read(t_record.category.data(), static_cast<std::streamsize>(record.category_len)))
    {
        return false;
    }

    if (!mFile.read(t_record.message.data(), static_cast<std::streamsize>(record.message_len)))
    {
        return false;
    }

    return true;
}