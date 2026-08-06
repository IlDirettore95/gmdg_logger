#pragma once

#include "backend.hpp"

// The cheapest realistic hand-rolled "logging" a caller could write without this library - an
// fprintf under a mutex, no async writer, no binary format (it was never meant to be file
// compatible with gmdg_logger). Gives the other backends' numbers a concrete reference point.
extern const LoggerBackend FprintfBackend;
