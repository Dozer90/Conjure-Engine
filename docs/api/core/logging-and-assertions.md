# Logging and assertions

📘 **Public API wiki** · 🛠 Handbook: [Core module](../../handbook/modules/core.md)

Two tools for two kinds of problem. **Logging** (`conjure/core/log.h`) reports what the program is doing, through
named categories with adjustable levels, to any number of sinks. **Assertions** (`conjure/core/assert.h`) catch
programmer errors (a precondition that can never be false in a correct program) and stop in the debugger. Runtime
failures that a correct program must expect (a missing file, bad input) are neither: they are
[results](errors-and-results.md).

## At a glance

| You want to... | Use |
|---|---|
| Print a message from your system | `CONJURE_LOG_INFO(LOG_GAME, "spawned {} enemies", count)` |
| Give your system its own channel | `CONJURE_DECLARE_LOG_CATEGORY(LOG_GAME);` in a header, `CONJURE_DEFINE_LOG_CATEGORY(LOG_GAME, "game");` in one `.cpp` |
| Hide chatter from one channel | `LOG_GAME.set_level(log_level::WARN)` or `log::set_category_level("game", log_level::WARN)` |
| Hide everything below a level | `log::set_global_level(log_level::INFO)` |
| Write the log to a file | `log::add_sink(std::make_shared<file_log_sink>(path))` |
| Show messages in your own UI | `callback_log_sink` with a lambda |
| Check an invariant | `CONJURE_ASSERT(index < size, "index {} out of range", index)` |
| Check a call that must also run in shipping builds | `CONJURE_VERIFY(device.close())` |
| Mark code that must never execute | `CONJURE_UNREACHABLE()` |
| Catch assertion failures yourself | `set_assert_handler(&my_handler)` |

## Quick example

```cpp
// game/log_category.h
#include "conjure/core/log.h"

CONJURE_DECLARE_LOG_CATEGORY(LOG_GAME);

// game/log_category.cpp
#include "game/log_category.h"

CONJURE_DEFINE_LOG_CATEGORY(LOG_GAME, "game");

// game/level.cpp
#include "game/log_category.h"

#include "conjure/core/assert.h"

#include <string_view>

void load_level(std::string_view name, int enemyCount) {
    CONJURE_ASSERT(enemyCount >= 0, "enemy count {} must not be negative", enemyCount);
    CONJURE_LOG_INFO(LOG_GAME, "loading {} with {} enemies", name, enemyCount);
    if (enemyCount == 0) {
        CONJURE_LOG_WARN(LOG_GAME, "level {} has no enemies", name);
    }
}
```

## Categories

A `log_category` is a named channel with its own minimum level. Categories are global objects that register
themselves on construction, so tools can list and adjust them at runtime (`log::categories()`,
`log::find_category("game")`, `log::set_category_level("game", level)`). The engine's own messages use `LOG_CORE`
and one category per module (`"renderer"`, `"ecs"`, ...); your game declares its own the same way.

> [!IMPORTANT]
> Declare a category once in a header (`CONJURE_DECLARE_LOG_CATEGORY(NAME);`) and define it in exactly one source
> file (`CONJURE_DEFINE_LOG_CATEGORY(NAME, "display name");`). The display name is what appears in log lines and
> what `find_category` looks up.

> [!NOTE]
> The engine's own modules declare their categories with `CONJURE_DECLARE_ENGINE_LOG_CATEGORY(NAME);` in their public
> `log_category.h`. It is `CONJURE_DECLARE_LOG_CATEGORY` plus the `CONJURE_API` export attribute, so a game module or
> an executable that links the engine's shared library can log to `LOG_ENGINE`, `LOG_SCENE` and the other engine
> channels. Your own categories, defined in your own binary, use the plain macro.

## Levels

| Level | Macro | Typical use | In Distribution builds |
|---|---|---|---|
| `TRACE` | `CONJURE_LOG_TRACE` | per-frame detail | compiled out |
| `DEBUG` | `CONJURE_LOG_DEBUG` | state changes useful while developing | compiled out |
| `INFO` | `CONJURE_LOG_INFO` | milestones: level loaded, device created | kept |
| `WARN` | `CONJURE_LOG_WARN` | something odd that the program recovered from | kept |
| `ERR` | `CONJURE_LOG_ERROR` | an operation failed | kept |
| `FATAL` | `CONJURE_LOG_FATAL` | the program cannot continue | kept |
| `OFF` | - | as a filter level: silence a category | - |

A message is delivered when its level is at or above both the category's level (`log_category::set_level`) and the
global level (`log::set_global_level`). `log::is_enabled(category, level)` answers that question up front, and the
macros only format the message (with `std::format`) when it will be delivered, so arguments to a filtered message
cost nothing.

> [!NOTE]
> The enumerator is `log_level::ERR`, not `ERROR`: `ERROR` is a Windows macro. The macro is still spelled
> `CONJURE_LOG_ERROR`. `to_string(level)` and `parse_log_level("warn")` convert levels to and from text.
> `parse_log_level` is case-insensitive and returns `log_level::OFF` for an unknown name, so validate configuration
> input before passing the result to `set_level`.

## Sinks

Every delivered message goes to every registered `log_sink`. Three are built in:

| Sink | What it does |
|---|---|
| `console_log_sink` | stdout below `WARN`, stderr from `WARN` up, optional ANSI colour. One is active by default; `log::set_console_enabled(false)` turns it off |
| `file_log_sink(path, truncate = true)` | writes formatted lines to a file, creating parent directories. The file is emptied first; pass `truncate = false` to append to an existing log. `is_open()` is false when the file could not be opened and messages are then dropped silently |
| `callback_log_sink(callback)` | forwards each `log_message` to a callable (an in-game console, a test) |

Sinks are added and removed as `std::shared_ptr<log_sink>` so you can keep the handle for removal:

```cpp
#include "conjure/core/log.h"

#include <memory>
#include <string>
#include <vector>

using namespace conjure::core;

class console_panel {
    private:
        std::vector<std::string> mLines = {};
        std::shared_ptr<log_sink> mSink = nullptr;

    public:
        console_panel() {
            mSink = std::make_shared<callback_log_sink>([this](const log_message &message) {
                mLines.push_back(format_log_line(message));
            });
            log::add_sink(mSink);
        }

        ~console_panel() {
            log::remove_sink(mSink);
        }
};
```

`format_log_line(message)` produces the same `"HH:MM:SS.mmm [LEVEL] [category] text"` layout the console and file
sinks use. `log::flush()` flushes every sink; `log::clear_sinks()` removes them all.

> [!WARNING]
> `write()` is called from whichever thread logged, possibly from several at once, and without any lock held by the
> logger. A sink must synchronise itself (the example above is only safe if `mLines` is guarded or used from one
> thread). The `std::string_view`s inside a `log_message` are valid only during the `write()` call; copy them if you
> keep the message.

> [!TIP]
> Because the logger holds no lock while delivering, a sink may itself log, or add and remove sinks, from inside
> `write()`. Use that for "log once then unsubscribe" patterns rather than tracking state elsewhere.

## Assertions

| Macro | Checks in Debug / Development | In Distribution |
|---|---|---|
| `CONJURE_ASSERT(condition, fmt, ...)` | yes | removed entirely; the condition is not evaluated |
| `CONJURE_VERIFY(condition, fmt, ...)` | yes | the condition is still evaluated, the check is dropped |
| `CONJURE_FATAL_ASSERT(condition, fmt, ...)` | yes | still checks (memory corruption, data that must be sane) |
| `CONJURE_UNREACHABLE()` | asserts, then aborts | tells the compiler the path is impossible |

The message is optional and uses `std::format` syntax; it is only formatted when the assertion fails. The handler
always receives the condition's text in `assert_info::expression`; `message` is empty when none was given.

```cpp
#include "conjure/core/assert.h"

#include <span>

using namespace conjure;

f32 sample(std::span<const f32> curve, usize index) {
    CONJURE_ASSERT(index < curve.size(), "sample index {} out of range (size {})", index, curve.size());
    return curve[index];
}
```

> [!TIP]
> Never put a call with side effects inside `CONJURE_ASSERT`: it vanishes in Distribution builds and the call with
> it. Use `CONJURE_VERIFY(connection.close())` for "this must succeed, and must also run when shipping".

> [!WARNING]
> A condition that contains an unparenthesised template comma (`is_same_v<A, B>`) must be wrapped in an extra pair
> of parentheses. The macros expand to statements, so they cannot be used at namespace scope; use `static_assert`
> there.

> [!CAUTION]
> In Distribution `CONJURE_UNREACHABLE()` becomes a compiler hint. Reaching it is undefined behaviour, so only mark
> paths that are impossible by construction (the `default` of a `switch` over a fully handled enumeration), never
> paths that bad data could reach.

### The assert handler

When an assertion fails, the active `assert_handler` receives an `assert_info` (expression, file, line, function,
formatted message) and returns an `assert_action`: `BREAK` (break into the debugger, or trap when none is attached),
`CONTINUE` (carry on as if it had passed) or `ABORT` (terminate). The default handler logs a `FATAL` message on
`LOG_CORE` and breaks. `set_assert_handler(handler)` installs a replacement and returns the previous one; passing
`nullptr` restores the default. The handler pointer is atomic, so it may be swapped from any thread.

```cpp
#include "conjure/core/assert.h"

using namespace conjure::core;

assert_action continue_and_count(const assert_info &info) {
    CONJURE_UNUSED(info);
    return assert_action::CONTINUE;
}

void run_without_breaking() {
    const assert_handler previous = set_assert_handler(&continue_and_count);
    // ... code whose assertions should not stop the process ...
    set_assert_handler(previous);
}
```

> [!NOTE]
> Some failures are fatal whatever the handler returns: `CONJURE_UNREACHABLE()`, `result::value()` on an error
> result and the containers' `at()` on a missing element all terminate after reporting, because there is nothing
> valid to continue with.

## API summary

| Signature (short) | Purpose |
|---|---|
| `CONJURE_DECLARE_LOG_CATEGORY(name);` / `CONJURE_DEFINE_LOG_CATEGORY(name, "display");` | declare / define a category |
| `CONJURE_DECLARE_ENGINE_LOG_CATEGORY(name);` | declare an engine module's category (exported from the engine's shared library) |
| `CONJURE_LOG_TRACE/DEBUG/INFO/WARN/ERROR/FATAL(category, fmt, ...)` | log at a level |
| `CONJURE_LOG(category, level, fmt, ...)` | log at a runtime level |
| `enum class log_level : u8 { TRACE, DEBUG, INFO, WARN, ERR, FATAL, OFF }` | levels |
| `std::string_view to_string(log_level)` / `log_level parse_log_level(std::string_view)` | level names |
| `log_category::level()` / `set_level(log_level)` / `is_enabled(log_level)` / `name()` | per-category filter |
| `log::set_global_level(log_level)` / `log::global_level()` | global filter |
| `log::is_enabled(const log_category &, log_level)` | would a message be delivered |
| `log::add_sink(std::shared_ptr<log_sink>)` / `remove_sink(...)` / `clear_sinks()` / `flush()` | sink management |
| `log::set_console_enabled(bool)` / `log::console_enabled()` | the default console sink |
| `log::categories()` / `log::find_category(std::string_view)` / `log::set_category_level(std::string_view, log_level)` | category registry |
| `struct log_message { level, category, text, timestamp, threadId, file, line }` | what a sink receives |
| `class log_sink { virtual void write(const log_message &) = 0; virtual void flush(); }` | sink base |
| `console_log_sink(bool useColour = true)` / `file_log_sink(path, bool truncate = true)` / `callback_log_sink(callback)` | built-in sinks |
| `std::string format_log_line(const log_message &)` | standard line layout |
| `extern log_category LOG_CORE` | the engine's own category |
| `CONJURE_ASSERT(condition, ...)` / `CONJURE_VERIFY(condition, ...)` / `CONJURE_FATAL_ASSERT(condition, ...)` | assertions |
| `CONJURE_UNREACHABLE()` | impossible path |
| `assert_handler set_assert_handler(assert_handler)` / `current_assert_handler()` / `default_assert_handler(info)` | handler control |
| `struct assert_info { expression, file, line, function, message }` / `enum class assert_action { BREAK, CONTINUE, ABORT }` | handler types |

## See also

- 📘 API: [Errors and results](errors-and-results.md): the third tool, for failures a correct program must expect
- 📘 API: [Data types](data-types.md): `CONJURE_ASSERTS_ENABLED` and the build configurations
- 📘 API: [Glossary](../glossary.md)
