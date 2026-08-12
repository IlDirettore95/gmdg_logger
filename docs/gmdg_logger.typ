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
    Low-overhead binary event logging library  ]
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
= Public API <public-api>

`gmdg_logger` exposes two layers: a C API (`gmdg_logger.h`, always compiled) and an optional C++
convenience layer of macros on top (`gmdg_logger.hpp`, `namespace GMDGLogger`).

== API reference — C API

#api-entry(
  "enum",
  "enum GMDGLogSeverity\n{\n    GMDG_LOG_DEBUG   = 0,\n    GMDG_LOG_INFO    = 1,\n    GMDG_LOG_WARNING = 2,\n    GMDG_LOG_ERROR   = 3,\n};",
)[
  The four built-in severity levels a record can carry.
]

#api-entry(
  "enum",
  "enum GMDGLogFileValidationResult\n{\n    GMDG_SUCCESS                  = 0,\n    GMDG_INVALID_MAGIC             = 1,\n    GMDG_UNUPPORTED_VERSION        = 2,\n    GMDG_UNUPPORTED_FILE_HEADER    = 3,\n    GMDG_UNKNOWN                   = 4,\n};",
)[
  Result of validating a `GMDGLogFileHeader` read back from disk. Note the `UNUPPORTED` spelling is
  as declared in the header, not a typo introduced here.
]

#api-entry(
  "enum",
  "enum GMDGBool\n{\n    GMDG_FALSE = 0,\n    GMDG_TRUE  = 1,\n};",
)[
  The C API's boolean type.
]

#api-entry("macro", "#define GMDG_THREAD_NAME_MAX_LENGTH 31u")[
  Maximum length, in bytes, of a thread name set via `GMDG_SetThreadName`; longer names are silently
  truncated.
]

#api-entry(
  "struct",
  "#pragma pack(push, 1)\nstruct GMDGLogFileHeader\n{\n    char     Magic[8];\n    uint32_t FormatVersion;\n    uint32_t RecordHeaderSize;\n    uint32_t Flags;\n};",
)[
  Fixed-size header written once at the start of a capture file.
]

#api-entry(
  "struct",
  "#pragma pack(push, 1)\nstruct GMDGLogRecord\n{\n    uint64_t Timestamp_Ns;\n    uint32_t ThreadId;\n    uint32_t Severity;\n    uint32_t ThreadNameLength;\n    uint32_t CategoryLength;\n    uint32_t MessageLength;\n};",
)[
  Fixed-size record header; followed on disk by the raw thread-name, category, and message bytes
  whose lengths this struct specifies (none of the three are null-terminated).
]

#api-entry("function", "GMDGBool GMDG_Logger_Initialize(const char* t_path);")[
  Opens (or creates) the log file at `t_path` for the process, writing a `GMDGLogFileHeader` if the
  file is new, and starts the background writer thread. Not reentrant.
]

#api-entry("function", "void GMDG_Logger_Shutdown();")[
  Stops the background writer thread and closes the file.
]

#api-entry(
  "function",
  "void GMDG_Log(\n    uint32_t t_severity,\n    const char* t_category, uint32_t t_categoryLength,\n    const char* t_message, uint32_t t_messageLength);",
)[
  Appends one record. The underlying entry point every `GMDG_LOG_*` macro funnels through.
]

#api-entry(
  "function",
  "void GMDG_SetThreadName(\n    const char* t_name, uint32_t t_nameLength);",
)[
  Tags the calling thread with a name; every subsequent `GMDG_Log` call on this thread carries it
  until changed. Thread-local. `t_nameLength` beyond `GMDG_THREAD_NAME_MAX_LENGTH` is silently
  truncated.
]

#api-entry("function", "const char* GMDG_Logger_Severity_To_String(GMDGLogSeverity t_severity);")[
  Returns a static string for one of the four severities (`"DEBUG"`, `"INFO"`, `"WARNING"`,
  `"ERROR"`).
]

#api-entry(
  "function",
  "GMDGLogFileValidationResult GMDG_Logger_Validate_File_Header(\n    const GMDGLogFileHeader* t_header);",
)[
  Validates the magic bytes, format version, and header size of a `GMDGLogFileHeader` read from
  disk.
]

#api-entry("function", "uint64_t GMDG_Logger_GetDroppedRecordCount(void);")[
  Number of records dropped so far because the async write buffer was full at the moment
  `GMDG_Log` was called.
]

== API reference — C++ convenience layer

Only compiled when `GMDG_LOGGER_ENABLED` is defined before `#include "gmdg_logger.hpp"` — see
@configuration.

#api-entry("constant", "inline constexpr size_t MessageBufferSize = 1024;")[
  Size, in bytes, of the fixed stack buffer a formatted message is written into before being handed
  to the C API — no heap allocation, and output longer than the buffer is safely truncated.
]

#api-entry("function", "inline void SetThreadName(std::string_view t_name);")[
  C++ wrapper over `GMDG_SetThreadName`; takes a runtime string, since worker threads are spawned
  dynamically and can't be tagged with a compile-time literal.
]

#api-entry(
  "function template",
  "template <size_t N1, typename... Args>\ninline void Log(\n    GMDGLogSeverity t_level,\n    const char (&t_category)[N1],\n    std::format_string<Args...> t_format,\n    Args&&... t_args);",
)[
  Formats `t_format`/`t_args...` into the `MessageBufferSize`-byte stack buffer and forwards to
  `GMDG_Log`. `t_category` must be a string-literal array (`const char (&)[N1]`) — a compile-time
  subsystem tag, not runtime data.
]

#api-entry(
  "macro",
  "GMDG_LOG_DEBUG(t_category, t_format, ...)\nGMDG_LOG_INFO(t_category, t_format, ...)\nGMDG_LOG_WARNING(t_category, t_format, ...)\nGMDG_LOG_ERROR(t_category, t_format, ...)",
)[
  Call `GMDGLogger::Log` with the corresponding `GMDGLogSeverity`. Compile to nothing when
  `GMDG_LOGGER_ENABLED` is undefined.
]

#api-entry("macro", "GMDG_LOG_SET_THREAD_NAME(t_name)")[
  Calls `GMDGLogger::SetThreadName`. Compiles to nothing when `GMDG_LOGGER_ENABLED` is undefined.
]

#api-entry("macro", "GMDG_LOG_INITIALIZE(t_path)")[
  Calls `GMDG_Logger_Initialize`. Compiles to `GMDG_TRUE` when `GMDG_LOGGER_ENABLED` is undefined,
  so `if (!GMDG_LOG_INITIALIZE(...))`-style call sites still compile and never spuriously fail just
  because logging is compiled out.
]

#api-entry("macro", "GMDG_LOG_SHUTDOWN()")[
  Calls `GMDG_Logger_Shutdown`. Compiles to nothing when `GMDG_LOGGER_ENABLED` is undefined.
]

#api-entry("macro", "GMDG_LOG_SEVERITY_TO_STRING(t_severity)")[
  Calls `GMDG_Logger_Severity_To_String`. Compiles to `""` when `GMDG_LOGGER_ENABLED` is undefined.
]

#api-entry("macro", "GMDG_LOG_VALIDATE_FILE_HEADER(t_header)")[
  Calls `GMDG_Logger_Validate_File_Header`. Compiles to `GMDG_UNKNOWN` when `GMDG_LOGGER_ENABLED` is
  undefined.
]

#api-entry("macro", "GMDG_LOG_GET_DROPPED_RECORD_COUNT()")[
  Calls `GMDG_Logger_GetDroppedRecordCount`. Compiles to `0` when `GMDG_LOGGER_ENABLED` is
  undefined.
]

== Configuration <configuration>

`GMDG_LOGGER_ENABLED` is a plain preprocessor macro — not a CMake option or cache variable, and not
wired in any `CMakeLists.txt`. The *consumer* `#define`s it before `#include "gmdg_logger.hpp"`:

```cpp
#define GMDG_LOGGER_ENABLED
#include "gmdg_logger.hpp"
```

Every function of the public C API (`gmdg_logger.h`) has a matching `GMDG_LOG_*` macro in the C++
convenience layer (`gmdg_logger.hpp`). If `GMDG_LOGGER_ENABLED` is left undefined, every one of
those macros compiles to nothing or a safe sentinel value (see each macro's entry above) — the
intended way to compile logging out of a build entirely (e.g. a shipping configuration), at zero
runtime cost. This switch only gates the C++ macro layer; the raw C API in `gmdg_logger.h` is
unaffected and always compiled — a consumer that calls it directly instead of going through the
`GMDG_LOG_*` macros (e.g. `gmdg_logger_bench`'s backend, which never defines
`GMDG_LOGGER_ENABLED`) always gets the real behavior.

#callout[
  *Build prerequisite, not part of this library's own API:* the top-level `CMakeLists.txt` also
  declares `GMDG_ASSERT_ENABLE`, which belongs to the sibling `gmdg_asserting` project
  (`add_subdirectory(../gmdg_asserting)`) that `gmdg_logger`'s implementation links against
  `PRIVATE`. The `Debug` preset turns it `ON`, `Release` turns it `OFF` — see @compiling.
]

== Compiling the library <compiling>

Requirements:

- `cmake_minimum_required(VERSION 4.1.1)`; C++23, fixed via presets (`CMAKE_CXX_STANDARD=23`,
  `CMAKE_CXX_STANDARD_REQUIRED=ON`, `CMAKE_CXX_EXTENSIONS=OFF`).
- *Windows only* — `gmdg_logger.cpp` includes `<windows.h>` directly for thread IDs and high-
  resolution timestamps; there is no platform abstraction layer.
- The sibling `gmdg_asserting` project checked out at `../gmdg_asserting`, relative to this
  project's top-level source directory (referenced via `add_subdirectory`).

`CMakePresets.json` (single top-level file, version 4):

#table(
  columns: (auto, auto, auto, auto),
  stroke: 0.5pt + rgb("#dddddd"),
  inset: 6pt,
  [*Preset*], [*`GMDG_ASSERT_ENABLE`*], [*Binary dir*], [*Install prefix*],
  [`Debug`], [`ON`], [`build/debug`], [`setup/debug`],
  [`Release`], [`OFF`], [`build/release`], [`setup/release`],
)

Built standalone, from the `gmdg_logger` directory:

```
cmake --preset Debug
cmake --build --preset Debug
```

(substitute `Release` for the release configuration). This builds every target: `gmdg_logger`
(static library), `gmdg_logger_c` (shared/DLL variant), `gmdg_logger_gui` (an ImGui capture
viewer), `gmdg_logger_test` / the `gmdg_logger_tests` CTest executable, and `gmdg_logger_bench`.

Compiler flags: MSVC gets `/W4 /EHsc`; GCC/Clang get `-Wall -Wextra -Wpedantic` plus
`-static -static-libgcc -static-libstdc++`; non-MSVC builds additionally link `stdc++exp` for the
test/bench/GUI executables.

== Including in a future project

*Static target (`gmdg_logger`)* — the common case:

+ `add_subdirectory()` this repository into your build.
+ `target_link_libraries(your_target PRIVATE gmdg_logger)` — `gmdg_logger/include` is a `PUBLIC`
  include directory on the target, so it propagates automatically; no separate
  `target_include_directories()` call is needed.
+ `#define GMDG_LOGGER_ENABLED` before `#include "gmdg_logger.hpp"` to get the `GMDG_LOG_*` macros
  (see @configuration); omit the define to compile them out entirely.

*Shared/DLL target (`gmdg_logger_c`)* — for a C-ABI consumer that needs a DLL instead:

+ Its include directory is `PRIVATE`, so it does *not* propagate through
  `target_link_libraries()` — manually add `gmdg_logger/include` to your own target's include
  directories.
+ Link against, or load, the built DLL directly.

// ===========================================================================
= Dependencies

== What this library depends on

#table(
  columns: (auto, auto, auto),
  stroke: 0.5pt + rgb("#dddddd"),
  inset: 6pt,
  [*Dependency*], [*Kind*], [*Visibility*],
  [C++23 standard library], [language/runtime], [n/a],
  [`gmdg_asserting`], [sibling GMDG library], [`PRIVATE`],
  [`<windows.h>`], [platform API], [n/a — no abstraction layer],
)

`gmdg_asserting` is linked `PRIVATE` on both the `gmdg_logger` and `gmdg_logger_c` targets, so it
does not propagate to a consumer. `gmdg_logger.cpp` includes `<windows.h>` directly on both
targets — there is no platform abstraction layer, so this library is Windows-only.

// ===========================================================================
= Examples

== Basic usage (C++ layer)

```cpp
#define GMDG_LOGGER_ENABLED
#include "gmdg_logger.hpp"

int main()
{
    GMDG_LOG_INITIALIZE("app.log");

    GMDG_LOG_DEBUG  ("PHYSICS", "Broadphase rebuilt");
    GMDG_LOG_INFO   ("NETWORK", "Client connected");
    GMDG_LOG_WARNING("AUDIO",   "Voice pool exhausted");
    GMDG_LOG_ERROR  ("RENDER",  "Failed to compile shader");

    GMDG_LOG_SHUTDOWN();
}
```

`GMDG_LOG_INITIALIZE(path)` opens (or appends to) the given file once, at startup, from a single
thread. `GMDG_LOG_DEBUG`/`GMDG_LOG_INFO`/`GMDG_LOG_WARNING`/`GMDG_LOG_ERROR` take a category and a
`std::format`-style message and may be called from any thread, at any time between
`GMDG_LOG_INITIALIZE` and `GMDG_LOG_SHUTDOWN`. `GMDG_LOG_SHUTDOWN()` stops the background writer and
closes the file — call it once, and make sure no other thread is still calling `GMDG_LOG_*` when
you do.

== Formatted messages

```cpp
GMDG_LOG_INFO ("AI",  "Enemy count: {}", count);
GMDG_LOG_ERROR("NET", "Failed after {} retries ({})", retries, reason);
```

A plain literal with no arguments (as in the basic-usage example above) still works exactly the
same way — a format string with no placeholders is just its own text. Because the format call
lives inside the macro's own arguments, disabling the logger (`GMDG_LOGGER_ENABLED` undefined)
drops the formatting work entirely along with everything else — this is why `GMDG_LOG_*` is
preferred over building a string yourself before the call.

For a message that's already in an owned buffer, or longer than the `MessageBufferSize` (1024-byte)
formatting buffer, call the C API directly with an explicit length to skip formatting entirely:

```cpp
std::string msg = ReadFromNetwork();
GMDG_Log(GMDG_LOG_INFO, "NET", 3, msg.c_str(), static_cast<uint32_t>(msg.size()));
```

== Naming threads

```cpp
void WorkerThreadMain(int index)
{
    GMDG_LOG_SET_THREAD_NAME(std::format("WorkerPool-{}", index));
    // ... GMDG_LOG_* calls from here on carry "WorkerPool-<index>" instead of a raw, run-specific id
}
```

Every record carries a thread name. By default it's an automatic `"Thread-<id>"` fallback derived
from the OS thread id the first time that thread logs. `GMDG_LOG_SET_THREAD_NAME`, typically called
once at the top of a thread's entry function, gives it a stable, meaningful name instead — unlike
category, this takes a runtime string, since worker threads are spawned dynamically and can't be
tagged with a compile-time literal.

== Checking for dropped records

```cpp
uint64_t dropped = GMDG_LOG_GET_DROPPED_RECORD_COUNT();
```

Under sustained, extreme burst logging the async writer can drop records rather than block the
calling thread. Call this if you need to know whether that happened for the current
`Initialize`/`Shutdown` session.

== Reading a capture file's header directly (C API)

```cpp
#include "gmdg_logger.h"
#include <cstdio>

GMDGLogFileHeader header{};
FILE* file = std::fopen("app.log", "rb");
std::fread(&header, sizeof(header), 1, file);

if (GMDG_Logger_Validate_File_Header(&header) != GMDG_SUCCESS)
{
    std::fprintf(stderr, "app.log: not a valid gmdg_logger capture\n");
}

std::fclose(file);
```

`GMDG_Logger_Validate_File_Header` checks the magic bytes, format version, and header size before a
reader trusts the records that follow. In practice, prefer `gmdg_logger_gui` to view a capture
rather than writing your own reader, unless you have a specific tooling need — the record layout in
@public-api (`GMDGLogRecord` plus raw thread-name/category/message bytes) is what a custom reader
would need to walk.
