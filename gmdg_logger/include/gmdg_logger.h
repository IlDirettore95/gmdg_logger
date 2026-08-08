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

// Max bytes of a thread name GMDG_SetThreadName will retain (NUL not included); a longer
// t_name_length is silently truncated to this many bytes.
#define GMDG_THREAD_NAME_MAX_LENGTH 31u

// Structs
#pragma pack(push, 1)
typedef struct GMDGLogFileHeader
{
    char     Magic[8];             // "GMDGLOG\0"
    uint32_t FormatVersion;        // file format version
    uint32_t RecordHeaderSize;     // sizeof(log_record_header) used by writer
    uint32_t Flags;                // reserved for future use
} GMDGLogFileHeader;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct GMDGLogRecord
{
    uint64_t Timestamp_Ns;
    uint32_t ThreadId;
    uint32_t Severity;
    uint32_t ThreadNameLength;
    uint32_t CategoryLength;
    uint32_t MessageLength;
} GMDGLogRecord;
#pragma pack(pop)

// Functions API
GMDGBool GMDG_Logger_Initialize(
    const char* t_path);

void GMDG_Logger_Shutdown();

void GMDG_Log(
    uint32_t        t_severity,
    const char*     t_category,
    uint32_t        t_categoryLength,
    const char*     t_message,
    uint32_t        t_messageLength);

// Tags the calling thread with a name; every subsequent GMDG_Log call on this thread carries it
// until changed. If never called, GMDG_Log auto-assigns "Thread-<id>" the first time this thread
// logs. t_nameLength beyond GMDG_THREAD_NAME_MAX_LENGTH is silently truncated. Thread-local.
void GMDG_SetThreadName(
    const char* t_name,
    uint32_t    t_nameLength);

const char* GMDG_Logger_Severity_To_String(
    GMDGLogSeverity t_severity);

GMDGLogFileValidationResult GMDG_Logger_Validate_File_Header(
    const GMDGLogFileHeader* t_header);

// Number of records dropped so far because the async write buffer was full.
uint64_t GMDG_Logger_GetDroppedRecordCount(void);

#ifdef __cplusplus
}
#endif