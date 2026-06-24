#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Enum types
typedef enum GMDGLogSeverity
{
    GMDG_LOG_DEBUG   = 0,
    GMDG_LOG_INFO    = 1,
    GMDG_LOG_WARNING = 2,
    GMDG_LOG_ERROR   = 3
} GMDGLogSeverity;

typedef enum GMDGLogFileValidationResult
{
    GMDG_SUCCESS                = 0,
    GMDG_INVALID_MAGIC          = 1,
    GMDG_UNUPPORTED_VERSION     = 2,
    GMDG_UNUPPORTED_FILE_HEADER = 3,
    GMDG_UNKNOWN                = 4
} GMDGLogFileValidationResult;

typedef enum GMDGBool
{
    GMDG_FALSE = 0,
    GMDG_TRUE  = 1
} GMDGBool;

// Structs
#pragma pack(push, 1)
typedef struct GMDGLogFileHeader
{
    char     magic[8];             // "GMDGLOG\0"
    uint32_t format_version;       // file format version
    uint32_t record_header_size;   // sizeof(log_record_header) used by writer
    uint32_t flags;                // reserved for future use
} GMDGLogFileHeader;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct GMDGLogRecord
{
    uint64_t timestamp_ns;
    uint32_t thread_id;
    GMDGLogSeverity severity;
    uint32_t category_len;
    uint32_t message_len;
} GMDGLogRecord;
#pragma pack(pop)

// Functions API
GMDGBool GMDG_Logger_Initialize(
    const char* t_path);

void GMDG_Logger_Shutdown();

void GMDG_Log(
    GMDGLogSeverity t_severity,
    const char*     t_category,
    uint32_t        t_category_length,
    const char*     t_message,
    uint32_t        t_message_length);

const char* GMDG_Logger_Severity_To_String(GMDGLogSeverity t_severity);
GMDGLogFileValidationResult GMDG_Logger_Validate_File_Header(const GMDGLogFileHeader* t_header);

#ifdef __cplusplus
}
#endif