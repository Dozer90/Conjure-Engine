# Processes, libraries and images

📘 **Public API wiki** · 🛠 Handbook: [Core module](../../handbook/modules/core.md)

Three small operating-system services, all in `conjure::core`: run external tools with
`conjure/core/process.h`, load plugins and modules at runtime with `conjure/core/dynamic_library.h`, and
load, inspect and save PNG images with `conjure/core/image.h`. Nothing here needs a window, a GPU or a
shell.

## At a glance

| You want to... | Use |
|---|---|
| Run a tool and collect its output | `process::run(desc)` |
| Run a tool while the editor keeps drawing | `process::start(desc)`, `is_running()`, `read_output()`, `wait()` |
| Stop a tool and everything it spawned | `process.kill()` or `process_desc::timeoutSeconds` |
| Load a plugin and call into it | `dynamic_library::load(path)`, `symbol_as<Fn>(name)` |
| Name a library file portably | `dynamic_library::decorate_name("conjure_foo")` |
| Load or save a PNG | `image::load_file(path)`, `image.save_png(path)` |
| Work with pixels | `image.pixel(x, y)`, `set_pixel()`, `fill()`, `pixels()` |
| Compare a screenshot with a golden image | `image.compare(other)` |

## Quick example

```cpp
#include "conjure/core/process.h"

using namespace conjure;
using namespace conjure::core;

result<bool> build_scripts(const std::filesystem::path &projectRoot) {
    process_desc desc = {};
    desc.executable = "dotnet";                           // bare name: searched in PATH
    desc.arguments = {"build", "Game.csproj", "-c", "Release"};
    desc.workingDirectory = projectRoot;
    desc.timeoutSeconds = 120.0;

    CONJURE_TRY_ASSIGN(const process_result built, process::run(desc));
    if (built.timedOut) {
        return error(error_code::TIMEOUT, "dotnet build did not finish in 120 seconds");
    }
    return built.exitCode == 0;                           // a non-zero exit code is not an error
}
```

## Child processes

### Describing the launch

`process_desc` holds everything about the launch. Arguments are an array and never go through a shell,
so you do not quote anything: `{"--name", "my level"}` arrives in the child as exactly those two
`argv` entries on every platform. `environment` entries are added to the inherited environment; an
empty value removes a variable. Standard input is always the null device, and children inherit only
the three standard streams (on Linux this needs glibc 2.34 or later; an older C library still keeps
close-on-exec descriptors out).

> [!WARNING]
> Windows runs `.bat` and `.cmd` files through `cmd.exe`, which would parse the arguments again with its
> own rules. To keep the "no shell" promise, starting one is `INVALID_ARGUMENT`; set `executable` to
> `cmd.exe` and pass `/c` and the script yourself when you really mean to run a batch file; `cmd.exe`
> then parses every argument with its own rules (`&`, `|`, `<`, `>`, `^`, `%`), which the quoting does
> not protect against, so pass only trusted text that way. A NUL character in the executable, an
> argument or an environment entry is `INVALID_ARGUMENT` everywhere.

| Field | Default | Meaning |
|---|---|---|
| `executable` | `""` | a path, or a bare name searched in `PATH` (a relative path is resolved against the parent's current directory, not `workingDirectory`) |
| `arguments` | `{}` | `argv[1..]`, unquoted |
| `workingDirectory` | inherit | must exist (`NOT_FOUND` otherwise) |
| `environment` | `{}` | `{name, value}` pairs; `""` removes |
| `captureStdout`, `captureStderr` | `true` | `false` lets the child write to the parent's console |
| `mergeStderr` | `false` | interleave stderr into `standardOutput` (requires `captureStdout`) |
| `timeoutSeconds` | `0` (none) | wall-clock limit for the whole run; the tree is killed when it elapses |

`run()` and `start()` fail with `INVALID_ARGUMENT` for an inconsistent description, `NOT_FOUND` for a
missing executable or working directory, and `PERMISSION_DENIED` or `BACKEND_FAILURE` when the operating
system refuses to start the child. The result carries `exitCode`, `standardOutput`, `standardError`,
`timedOut` and `elapsedSeconds`.

### Running in the background

`start()` returns immediately with a move-only `process`. Poll it from the main thread once per frame:
`read_output()` and `read_error()` append whatever arrived since the last call and never block, and
`wait()` returns the remainder once the child has exited.

```cpp
#include "conjure/core/assert.h"

result<process> start_cook(const std::filesystem::path &projectRoot) {
    process_desc desc = {};
    desc.executable = "conjure_cook";
    desc.arguments = {"--project", projectRoot.generic_string()};
    desc.timeoutSeconds = 600.0;
    return process::start(desc);
}

// Called once per frame while the cook runs; returns the result once the tool has finished.
std::optional<process_result> poll_cook(process &cooker, std::string &log) {
    cooker.read_output(log);                               // never blocks
    if (cooker.is_running()) {
        return std::nullopt;
    }
    result<process_result> finished = cooker.wait();       // the output not yet handed out
    // wait() fails only when it was already called (INVALID_STATE): a programmer error, not a runtime one.
    CONJURE_ASSERT(finished.has_value(), "poll_cook() called again after the result was handed out");
    if (!finished.has_value()) {
        return std::nullopt;
    }
    log += finished->standardOutput;
    return std::move(finished).value();
}
```

> [!IMPORTANT]
> `wait()` may be called exactly once per started process (`INVALID_STATE` afterwards). Call it even after
> `kill()`: that is how the exit code and the remaining output are collected.

> [!WARNING]
> Destroying a `process` that was never waited for **kills the child and its whole process tree**. Keep
> the `process` alive for as long as the tool should run, and move it rather than letting a temporary die.

> [!WARNING]
> Without a timeout, `wait()` blocks until every descendant that inherited the output pipe has closed it.
> A tool that forks a daemon keeps you waiting. Set `timeoutSeconds` for anything you do not fully control;
> when it elapses the tree is killed, `timedOut` is set and a child that already exited keeps its own exit
> code.

`kill()` terminates the child and every process it spawned, including grandchildren that outlived it, and
is harmless once the child is gone. A killed child reports `process::KILLED_EXIT_CODE` (-1), and so does a
child whose exit status could not be obtained (Linux and macOS: the host ignores `SIGCHLD` or reaped the
child itself), so a lost status never reads as success. Timeouts above
`process::MAX_TIMEOUT_SECONDS` behave like that maximum; negative or non-finite values are `INVALID_ARGUMENT`.

> [!NOTE]
> Threading: `read_output()`, `read_error()`, `is_running()` and `kill()` may be called from any thread,
> including while another thread blocks in `wait()`. `start()`, `wait()` and destruction belong to one
> thread at a time. While a child with captured output runs, the engine drains its pipes on threads of its
> own, which a debugger or profiler lists as `process-stdout` and `process-stderr` on Windows and as
> `process-pipes` on Linux and macOS. They end with the child and are gone once `wait()` returns.

## Shared libraries

`dynamic_library` is an RAII handle over `LoadLibrary` / `dlopen`. `load()` takes a path or a bare file
name (resolved through the operating system's library search path) and fails with `INVALID_ARGUMENT` for
an empty path, `NOT_FOUND` when the file does not exist and `BACKEND_FAILURE` (with the loader's message)
when it exists but cannot be loaded, for example because one of its own dependencies is missing.

```cpp
#include "conjure/core/dynamic_library.h"

using plugin_name_fn = const char *(*)();

result<std::string> read_plugin_name(const std::filesystem::path &pluginDirectory) {
    const std::string fileName = dynamic_library::decorate_name("conjure_my_plugin");   // .dll / .so / .dylib
    CONJURE_TRY_ASSIGN(const dynamic_library plugin, dynamic_library::load(pluginDirectory / fileName));

    const plugin_name_fn getName = plugin.symbol_as<plugin_name_fn>("conjure_plugin_name");
    if (getName == nullptr) {
        return error(error_code::NOT_FOUND, "the plugin does not export conjure_plugin_name");
    }
    return std::string(getName());   // copied before 'plugin' unloads at the end of the scope
}
```

> [!TIP]
> Always build file names with `decorate_name()`: `"conjure_foo"` becomes `conjure_foo.dll`,
> `libconjure_foo.so` or `libconjure_foo.dylib`, a directory part is kept in front of the prefix
> (`modules/conjure_foo` -> `modules/libconjure_foo.so`), and a name that already carries the platform
> extension is returned unchanged. `extension()` and `prefix()` expose the two pieces.

> [!CAUTION]
> Every pointer obtained through `symbol()` or `symbol_as<Fn>()` becomes dangling when the library is
> unloaded (`unload()`, destruction, or move-assigning another library over it). Calling it afterwards is
> undefined behaviour. Keep the `dynamic_library` alive for as long as any symbol from it is in use.

`symbol_as<Fn>()` requires `Fn` to be a plain pointer type (a function pointer or `T *`) and returns
`nullptr` for an unknown name.

> [!WARNING]
> On Windows a library loaded by absolute path finds its dependencies in its own directory first, so a
> plugin finds the libraries shipped next to it regardless of the current directory. On Linux and macOS
> the loader does **not** do this: link your plugin with an `$ORIGIN` run path (CMake
> `BUILD_RPATH_USE_ORIGIN` / `INSTALL_RPATH "$ORIGIN"`) or give the dependency an `@loader_path` install
> name so libraries shipped next to it are found.

## Images

`image` is a row-major RGBA8 bitmap (4 bytes per pixel, rows top to bottom, no padding) with PNG support
through `load_png()` / `encode_png()` for memory and `load_file()` / `save_png()` for files. It is a value
type: copies are deep. Grey, grey+alpha, RGB, palette and 16-bit PNGs are expanded to RGBA8 on load (alpha
255 when absent).

```cpp
#include "conjure/core/image.h"

result<void> make_thumbnail_border(const std::filesystem::path &source, const std::filesystem::path &target) {
    CONJURE_TRY_ASSIGN(image picture, image::load_file(source));
    const rgba8 border = {255, 255, 255, 255};
    for (u32 x = 0; x < picture.width(); ++x) {
        picture.set_pixel(x, 0, border);
        picture.set_pixel(x, picture.height() - 1, border);
    }
    picture.flip_vertical();            // GPU read-backs are often bottom-up
    return picture.save_png(target);    // replaces an existing file
}

result<bool> matches_golden(const image &actual, const std::filesystem::path &goldenPath) {
    CONJURE_TRY_ASSIGN(const image expected, image::load_file(goldenPath));
    return actual.compare(expected) < 0.002;   // mean absolute difference, 0 identical .. 1
}
```

| Failure | Code |
|---|---|
| `load_png()` with empty data / not a PNG | `INVALID_ARGUMENT` / `PARSE_FAILURE` |
| `load_file()` missing / unreadable / not a PNG | `NOT_FOUND` / `IO_FAILURE` / `PARSE_FAILURE` |
| `encode_png()` or `save_png()` of an empty image | `INVALID_STATE` |
| `encode_png()` of an image whose bytes exceed `image::MAX_ENCODED_BYTES` | `UNSUPPORTED` |
| `save_png()` into a missing directory | `IO_FAILURE` |

> [!WARNING]
> `pixel()` and `set_pixel()` assert on out-of-range coordinates (and return a default `rgba8` or change
> nothing). `image(width, height)` with a zero or oversized dimension (above `image::MAX_DIMENSION`), or
> whose pixel data `width * height * 4` would exceed `image::MAX_PIXEL_BYTES` (INT_MAX, what the PNG codecs
> can handle), asserts for the oversized cases and yields the empty image. Check `is_empty()` before encoding.

> [!NOTE]
> This is the small image type core needs for icons, screenshots and golden-image tests. Textures and
> their formats belong to the renderer; do not build a texture pipeline on `image`.

`compare()` returns the mean absolute byte difference normalised to `0.0` (identical) .. `1.0`; images of
different dimensions compare as `1.0`, two empty images as `0.0`. `operator==` requires equal dimensions
and identical bytes. `pixels()` exposes the raw bytes; pixel `(x, y)` starts at `(y * width + x) * 4`.

## API summary

| Signature (short) | Purpose |
|---|---|
| `static result<process_result> process::run(const process_desc &)` | start and wait |
| `static result<process> process::start(const process_desc &)` | start and return immediately |
| `bool process::is_running() const` | child alive and not yet waited for |
| `result<process_result> process::wait()` | exit code and remaining output; once only |
| `void process::kill()` | kill the child and its process tree |
| `usize process::read_output(std::string &out)` / `read_error(std::string &out)` | append new output, never blocks |
| `u32 process::process_id() const` | operating-system id, 0 when not started |
| `process::KILLED_EXIT_CODE` / `process::MAX_TIMEOUT_SECONDS` | -1 / 1e9 |
| `static result<dynamic_library> dynamic_library::load(const path &)` | load a library |
| `void dynamic_library::unload()` / `bool is_loaded() const` / `const path &path() const` | lifetime and identity |
| `void *dynamic_library::symbol(const char *) const` / `Fn symbol_as<Fn>(const char *) const` | exported symbols |
| `static std::string dynamic_library::decorate_name(string_view)` / `extension()` / `prefix()` | platform file names |
| `image()` / `image(u32 width, u32 height)` | empty / zero-filled image |
| `u32 image::width() const` / `height()` / `bool is_empty()` | dimensions |
| `rgba8 image::pixel(u32 x, u32 y) const` / `void set_pixel(x, y, rgba8)` / `void fill(rgba8)` | pixel access |
| `span<std::byte> image::pixels()` | raw RGBA8 bytes |
| `static result<image> image::load_png(span<const std::byte>)` / `load_file(const path &)` | decode |
| `result<std::vector<std::byte>> image::encode_png() const` / `result<void> save_png(const path &) const` | encode |
| `void image::flip_vertical()` / `f64 compare(const image &) const` | helpers |
| `image::BYTES_PER_PIXEL` / `MAX_DIMENSION` / `MAX_ENCODED_BYTES` / `MAX_PIXEL_BYTES` | 4 / 2^24 / INT_MAX / INT_MAX |

## See also

- 📘 API: [Errors and results](errors-and-results.md) for `result<T>`, `CONJURE_TRY_ASSIGN` and the codes above
- 📘 API: [Files and the virtual file system](files.md) for reading and writing game files
- 📘 API: [Jobs and threading](jobs.md) when a tool's output should be processed off the main thread
- 🛠 Handbook: [Core module](../../handbook/modules/core.md) for the platform backends (job objects, process groups, pipe draining)
