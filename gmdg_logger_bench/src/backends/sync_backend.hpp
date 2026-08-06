#pragma once

#include "backend.hpp"

// Reference implementation with no background writer thread: every Log() call locks a mutex and
// writes + flushes directly on the caller's thread. Uses the same binary record/header format as
// the real gmdg_logger so the comparison isolates the threading/buffering strategy, not the file
// format - a file it produces validates against the real GMDG_Logger_Validate_File_Header.
extern const LoggerBackend SyncBackend;
