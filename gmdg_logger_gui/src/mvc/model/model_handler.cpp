#include "pch.h"

#include "model_handler.hpp"

using namespace GMDGLoggerGUI;

void ModelHandler::Initialize()
{
    mFile.open("../gmdg_logger_test/app.log", std::ios::binary);

    assert(mFile);
}

void ModelHandler::Update()
{
    LogRecord record;

    while (ReadRecord(record))
    {
        mLogs.emplace_back(std::move(record));

        std::println("[{}] [{}] [{}] : {}",
                     mLogs.back().thread_id,
                     mLogs.back().category,
                     mLogs.back().severity,
                     mLogs.back().message);
    }

    mFile.clear();
}

void ModelHandler::Shutdown() {}

bool ModelHandler::ReadRecord(LogRecord& record)
{
    log_record_header hdr{};

    if (!mFile.read(reinterpret_cast<char*>(&hdr), sizeof(hdr)))
    {
        return false;
    }

    record.timestamp_ns = hdr.timestamp_ns;
    record.thread_id = hdr.thread_id;
    record.severity     = hdr.level;

    record.category.resize(hdr.category_len);
    record.message.resize(hdr.message_len);

    if (!mFile.read(record.category.data(), static_cast<std::streamsize>(hdr.category_len)))
    {
        return false;
    }

    if (!mFile.read(record.message.data(), static_cast<std::streamsize>(hdr.message_len)))
    {
        return false;
    }

    return true;
}