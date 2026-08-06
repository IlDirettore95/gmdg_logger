// ---------------------------------------------------------------------------
// Shared PDF template for docs generated via .claude/skills/pdf/SKILL.md
// Source of the shared styling: docs/pdf_template.typ (repo root)
// ---------------------------------------------------------------------------

#let accent = rgb("#2b5b84")
#let accent-light = rgb("#eef3f7")
#let mono = "Cascadia Code"
#let body-font = "Libertinus Serif"
#let head-font = "Segoe UI"

#set document(title: "gmdg_logger — Library Guide", author: "GMDG")
#set page(paper: "a4", margin: (x: 2.6cm, y: 2.6cm))
#set text(font: body-font, size: 10.5pt, lang: "en")
#set par(justify: true, leading: 0.65em)

#show raw: set text(font: mono, size: 9pt, features: (calt: 0, liga: 0))
#show raw.where(block: true): it => block(
  fill: accent-light,
  stroke: (left: 2.5pt + accent),
  inset: (x: 10pt, y: 8pt),
  radius: 3pt,
  width: 100%,
  above: 12pt,
  below: 12pt,
  it,
)
#show raw.where(block: false): box.with(
  fill: accent-light,
  inset: (x: 3pt, y: 0pt),
  outset: (y: 2pt),
  radius: 2pt,
)

#set heading(numbering: "1.1")
#show heading: it => {
  set text(font: head-font, fill: accent)
  if it.level == 1 {
    v(18pt)
    block(text(size: 18pt, weight: "bold", it))
    v(2pt)
    line(length: 100%, stroke: 0.6pt + accent)
    v(6pt)
  } else {
    v(10pt)
    text(size: 12.5pt, weight: "bold", it)
    v(4pt)
  }
}

// Callout box for notes/warnings that should stand out from body text.
#let callout(body) = block(
  fill: accent-light, stroke: (left: 2.5pt + accent),
  inset: (x: 10pt, y: 8pt), radius: 3pt, width: 100%,
)[#body]

// One function/macro/type per entry.
#let api-entry(kind, signature, body) = block(
  width: 100%,
  above: 14pt,
  below: 10pt,
  breakable: false,
  [
    #block(
      fill: white,
      stroke: 0.6pt + accent.lighten(20%),
      inset: (x: 10pt, y: 7pt),
      radius: 3pt,
      width: 100%,
      [
        #text(font: head-font, size: 8pt, weight: "bold", fill: accent.darken(10%), tracking: 0.5pt)[#upper(kind)]
        #v(3pt)
        #raw(signature, lang: "cpp", block: true)
        #body
      ]
    )
  ]
)

// ---------------------------------------------------------------------------
// Title page
// ---------------------------------------------------------------------------
#align(center + horizon)[
  #text(font: head-font, size: 34pt, weight: "bold", fill: accent)[gmdg_logger]
  #v(8pt)
  #text(font: head-font, size: 14pt, fill: rgb("#555555"))[
    Low-overhead binary event logging for a game engine
  ]
  #v(40pt)
  #text(font: head-font, size: 12pt, fill: rgb("#777777"))[Library Guide]
  #v(4pt)
  #text(font: head-font, size: 10pt, fill: rgb("#999999"))[
    Generated with Typst · #datetime.today().display("[month repr:long] [day], [year]")
  ]
]

#pagebreak()

// ---------------------------------------------------------------------------
// Table of contents
// ---------------------------------------------------------------------------
#outline(title: [Contents], indent: auto, depth: 2)

#pagebreak()

#set page(
  footer: context [
    #set text(size: 8pt, fill: rgb("#999999"), font: head-font)
    #line(length: 100%, stroke: 0.4pt + rgb("#dddddd"))
    #v(4pt)
    #grid(
      columns: (1fr, 1fr),
      align(left)[gmdg_logger — Library Guide],
      align(right)[#counter(page).display("1")],
    )
  ]
)

// ===========================================================================
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

#pagebreak()

// ===========================================================================
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

#pagebreak()

// ===========================================================================
= How do I use it correctly?

== Getting started

Link against the `gmdg_logger` static library target (or `gmdg_logger_c`, the shared/DLL variant
built from the same source, if your consumer needs a C-ABI DLL instead), and add
`gmdg_logger/include` to your include path. Both targets are declared in
`projects/gmdg_logger/gmdg_logger/CMakeLists.txt`.

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
- `LOG_DEBUG` / `LOG_INFO` / `LOG_WARNING` / `LOG_ERROR` take a category and a `std::format`-style
  message (format string plus optional arguments) and may be called from any thread, at any time
  between `Initialize` and `Shutdown`.
- `GMDG_Logger_Shutdown()` stops the background writer and closes the file. Call it once, and make
  sure no other thread is still calling `LOG_*` when you do (see @limitations).

== Formatted messages

The message argument to `LOG_DEBUG` / `LOG_INFO` / `LOG_WARNING` / `LOG_ERROR` is a `std::format`
string plus optional arguments, formatted into a fixed 1024-byte stack buffer
(`GMDGLogger::kMessageBufferSize`) before being handed to the writer — no heap allocation, and
output longer than the buffer is safely truncated rather than overrunning it:

```cpp
LOG_INFO ("AI",  "Enemy count: {}", count);
LOG_ERROR("NET", "Failed after {} retries ({})", retries, reason);
```

A plain literal with no arguments, as in the example above, still works exactly the same way — a
format string with no placeholders is just its own text.

Because the format call lives inside the macro's own arguments, disabling the logger
(`GMDG_LOGGER_ENABLED` undefined) drops the formatting work entirely along with everything else.
This is the reason to prefer `LOG_*` over building a string yourself: a `std::format` call made in
a separate statement *before* a macro invocation always runs, regardless of whether logging is
enabled — only work that's textually inside the macro's arguments gets compiled out with it.

The category must still be a compile-time string literal (it's a static subsystem tag, not runtime
data). For a message that's already in an owned buffer, or longer than the 1024-byte formatting
buffer, call the C API directly with an explicit length to skip the formatting step entirely:

```cpp
std::string msg = ReadFromNetwork();
GMDG_Log(GMDG_LOG_INFO, "NET", 3, msg.c_str(), static_cast<uint32_t>(msg.size()));
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

== API reference

#api-entry("function", "GMDGBool GMDG_Logger_Initialize(const char* t_path);")[
  Opens (or creates) the log file at `t_path` for the process, writing a `GMDGLogFileHeader` if the
  file is new, and starts the background writer thread. Not reentrant — calling it a second time
  while already initialized fails (and asserts in an assert-enabled build) rather than reopening.
]

#api-entry("function", "void GMDG_Logger_Shutdown();")[
  Stops the background writer thread and closes the file. Not synchronized against concurrent
  `GMDG_Log` calls on other threads — see @limitations.
]

#api-entry(
  "function",
  "void GMDG_Log(\n    uint32_t t_severity,\n    const char* t_category, uint32_t t_category_length,\n    const char* t_message, uint32_t t_message_length);",
)[
  The underlying C entry point every `LOG_*` macro funnels through. Appends one record to the
  in-memory front buffer under a short lock; never touches the filesystem on the calling thread.
  Prefer the `LOG_*` macros unless you already have an unformatted, explicitly-lengthed buffer
  (see "Formatted messages" above).
]

#api-entry("function", "const char* GMDG_Logger_Severity_To_String(GMDGLogSeverity t_severity);")[
  Returns a static string for one of the four severities (`"DEBUG"`, `"INFO"`, `"WARNING"`,
  `"ERROR"`). Mainly useful for tooling that reads a capture back, such as `gmdg_logger_gui`.
]

#api-entry(
  "function",
  "GMDGLogFileValidationResult GMDG_Logger_Validate_File_Header(\n    const GMDGLogFileHeader* t_header);",
)[
  Validates the magic bytes, format version, and header size of a `GMDGLogFileHeader` read from
  disk, before a reader trusts the records that follow it.
]

#api-entry("function", "uint64_t GMDG_Logger_GetDroppedRecordCount(void);")[
  Number of records dropped so far because the async write buffer was full at the moment
  `GMDG_Log` was called. Monotonically increasing for the lifetime of one `Initialize`/`Shutdown`
  session; reset to zero on the next `Initialize`.
]

#pagebreak()

// ===========================================================================
= How it works internally <async-writer>

`GMDG_Log` never touches the filesystem. It takes a short lock, memcpy's the serialized record
into a "front" in-memory buffer, and returns. A background thread wakes every fixed interval
(currently 5 ms), swaps the front buffer with an idle "back" buffer, and writes the swapped-out
buffer to disk outside the lock — so producer threads keep appending to the new front buffer while
the previous one is being flushed.

Each buffer has a fixed capacity (currently 8 MiB). If a record does not fit in the remaining space
before the next swap, it is *dropped and counted* rather than blocking the calling thread — this
capacity/interval combination was raised from an earlier 1 MiB / 20 ms pairing after benchmarking
showed the smaller buffers dropping the majority of records under bursty multi-threaded load. Real
usage patterns that get anywhere close to that burstiness should either tolerate drops (and monitor
`GMDG_Logger_GetDroppedRecordCount()`) or have the buffer size / flush interval tuned further.

#pagebreak()

// ===========================================================================
= Assumptions <limitations>

- *Single process-wide logger.* All state is a set of global/static variables — there is no
  per-instance logger object, and `Initialize` / `Shutdown` are not reentrant or safe to call
  concurrently with each other.
- *Windows only.* Thread identification uses `GetCurrentThreadId()` directly, and high-resolution
  timestamps use `GetSystemTimePreciseAsFileTime()`; there is no platform abstraction layer.
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

#pagebreak()

// ===========================================================================
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
