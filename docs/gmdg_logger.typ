#set document(title: "gmdg_logger — Library Guide")
#set page(margin: 2.2cm, numbering: "1")
#set text(font: "New Computer Modern", size: 10.5pt)
#set heading(numbering: "1.")
#set par(justify: true)

#align(center)[
  #text(size: 20pt, weight: "bold")[gmdg_logger]
  #v(0.3em)
  #text(size: 12pt, style: "italic")[Library guide]
]

#v(1em)

= What is this for?

`gmdg_logger` is a logging library for use inside a game engine. A log entry is scoped to exactly
four things: a *message*, a *category*, a *severity*, and the *thread* it came from. It is
deliberately *not* a profiler — there is no support for, and no intent to add, structured telemetry
such as counters, scoped timers/spans, or frame markers. If you need that, use a dedicated profiler
instead.

The library has two parts, built from `projects/gmdg_logger`:

- *`gmdg_logger`* (and its DLL twin `gmdg_logger_c`) — the writer. A small C API, with a C++
  convenience layer of macros on top, that appends log records to a binary file.
- *`gmdg_logger_gui`* — a companion ImGui viewer that reads that binary file and displays it as a
  filterable table (by severity and thread ID), currently tailing one fixed relative path.

The design priority is *speed*: logging calls must be cheap enough to leave in a running game, not
just a debug build.

= When should I use it?

Use `gmdg_logger` when you want low-overhead, thread-safe logging of discrete text events
(errors, warnings, state transitions, one-off diagnostics) from anywhere in an engine, with the
ability to inspect a capture afterwards — or, loosely, while the process is still running — through
`gmdg_logger_gui`.

Do not reach for it when you need:

- *Profiling data* (timings, counters, spans) — out of scope by design; use the separate profiler
  project instead.
- *Human-readable logs without any tooling* — the on-disk format is binary; you need
  `gmdg_logger_gui` (or a small script) to make sense of a capture, you cannot just open it in a
  text editor.
- *Cross-platform logging* — the current implementation reads the thread ID via the Windows API
  (`GetCurrentThreadId`) and has no platform abstraction.

= How do I use it correctly?

== Linking

Link against the `gmdg_logger` static library target (or `gmdg_logger_c`, the shared/DLL variant
built from the same source, if your consumer needs a C-ABI DLL instead), and add
`gmdg_logger/include` to your include path. Both targets are declared in
`projects/gmdg_logger/gmdg_logger/CMakeLists.txt`.

== Enabling logging at compile time

`gmdg_logger.hpp` (the C++ convenience layer) only emits real code when `GMDG_LOGGER_ENABLED` is
defined *before* it is included:

```cpp
#define GMDG_LOGGER_ENABLED
#include "gmdg_logger.hpp"
```

If that macro is not defined, every `LOG_*` macro below compiles to nothing — this is the
intended way to compile logging out of a build entirely (e.g. a shipping configuration), at zero
runtime cost.

== Basic usage

```cpp
#define GMDG_LOGGER_ENABLED
#include "gmdg_logger.hpp"

int main()
{
    GMDG_Logger_Initialize("app.log");

    LOG_DEBUG  ("PHYSICS", "Broadphase rebuilt");
    LOG_INFO   ("NETWORK", "Client connected");
    LOG_WARNING("AUDIO",   "Voice pool exhausted");
    LOG_ERROR  ("RENDER",  "Failed to compile shader");

    GMDG_Logger_Shutdown();
}
```

- `GMDG_Logger_Initialize(path)` opens (or appends to) the given file once, at startup, from a
  single thread.
- `LOG_DEBUG` / `LOG_INFO` / `LOG_WARNING` / `LOG_ERROR` take a category and a message and may be
  called from any thread, at any time between `Initialize` and `Shutdown`.
- `GMDG_Logger_Shutdown()` stops the background writer and closes the file. Call it once, and make
  sure no other thread is still calling `LOG_*` when you do (see @limitations).

== Passing dynamic strings

The `LOG_*` macros only accept *compile-time string literals* for both category and message — they
expand to a template that takes `const char (&)[N]`, so a `std::string`, a `const char*` variable,
or a formatted buffer will not compile through them. To log a runtime-built string, call the C API
directly with an explicit length:

```cpp
std::string msg = std::format("Enemy count: {}", count);
GMDG_Log(GMDG_LOG_INFO, "AI", 2, msg.c_str(), static_cast<uint32_t>(msg.size()));
```

== Checking for dropped records

Under sustained, extreme burst logging the async writer can drop records rather than block the
calling thread (see @async-writer). Call `GMDG_Logger_GetDroppedRecordCount()` if you need to know
whether that happened:

```cpp
uint64_t dropped = GMDG_Logger_GetDroppedRecordCount();
```

== Reading a capture back

A capture is a binary file: one `GMDGLogFileHeader`, followed by a sequence of
`GMDGLogRecord` + raw category bytes + raw message bytes. `GMDG_Logger_Validate_File_Header`
checks the magic bytes, format version, and header size before you trust a file. In practice, use
`gmdg_logger_gui` to view a capture rather than writing your own reader, unless you have a specific
tooling need.

= How it works internally <async-writer>

`GMDG_Log` never touches the filesystem. It takes a short lock, memcpy's the serialized record
into a "front" in-memory buffer, and returns. A background thread wakes every fixed interval
(currently 20 ms), swaps the front buffer with an idle "back" buffer, and writes the swapped-out
buffer to disk outside the lock — so producer threads keep appending to the new front buffer while
the previous one is being flushed.

Each buffer has a fixed capacity (currently 1 MiB). If a record does not fit in the remaining space
before the next swap, it is *dropped and counted* rather than blocking the calling thread —
under a synthetic worst-case burst (multiple threads logging in a tight loop with no throttling),
this can mean the large majority of records are dropped. That is expected behavior for that kind of
unrealistic load, not a bug — but it means real usage patterns that get anywhere close to that
burstiness should either tolerate drops (and monitor
`GMDG_Logger_GetDroppedRecordCount()`) or have the buffer size / flush interval tuned upward.

= Assumptions <limitations>

- *Single process-wide logger.* All state is a set of global/static variables — there is no
  per-instance logger object, and `Initialize` / `Shutdown` are not reentrant or safe to call
  concurrently with each other.
- *Windows only.* Thread identification uses `GetCurrentThreadId()` directly; there is no
  platform abstraction layer.
- *Category and message are raw byte blobs, not null-terminated strings* on disk — `category_len`
  and `message_len` in `GMDGLogRecord` are the only source of truth for their extents.
- *No framing/resync in the file format.* A file truncated mid-write (e.g. a crash) desyncs any
  reader walking records sequentially from that point on — the length-prefixed record after a
  truncation cannot be located without extra recovery logic.
- *Severities are a fixed, built-in 4-level enum* (`GMDG_LOG_DEBUG` / `INFO` / `WARNING` /
  `ERROR`) today — there is currently no mechanism for a consuming application to define its own
  severity set.
- *`GMDG_Log` calls are not synchronized against `GMDG_Logger_Shutdown`.* A log call racing with
  shutdown on another thread can access the writer mid-teardown.

= Mistakes to avoid

- *Forgetting `#define GMDG_LOGGER_ENABLED` before including `gmdg_logger.hpp`.* All four `LOG_*`
  macros silently vanish (no compile error, no runtime effect) — easy to miss for a first-time
  integrator.
- *Passing a `std::string` or any non-literal to `LOG_*`.* It will not compile; use `GMDG_Log`
  directly with an explicit pointer and length instead (see above).
- *Calling `LOG_*` before `GMDG_Logger_Initialize` or after `GMDG_Logger_Shutdown`.* The call is
  silently dropped (it checks the internal file handle and returns early) rather than asserting or
  erroring — do not rely on it to fail loudly.
- *Assuming zero log loss under heavy burst logging.* The async writer's overflow policy is
  drop-and-count, not block — check `GMDG_Logger_GetDroppedRecordCount()` if guaranteed delivery
  matters for your use case.
- *Calling `GMDG_Logger_Shutdown()` while other threads may still log.* Quiesce logging on other
  threads first (see @limitations).
- *Opening the binary log file in a text editor expecting readable text.* It is a binary format;
  use `gmdg_logger_gui`, or the record layout above if you need to write your own reader.
