#pragma once

#include "backend.hpp"

// Thin wrapper over the real gmdg_logger: async, double-buffered, dedicated writer thread.
extern const LoggerBackend AsyncBackend;
