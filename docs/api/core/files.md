# Files and the virtual file system

📘 **Public API wiki** · 🛠 Handbook: [Core module](../../handbook/modules/core.md)

Read and write game files through one interface, whether they live in a project folder on disk, in memory
or behind a `scheme://` name such as `project://scenes/main.scene`. Include `conjure/core/vfs/vfs.h` for
the whole area (`conjure::core::vfs`); it pulls in `conjure/core/vfs/file_system.h` (the interface),
`conjure/core/vfs/native_file_system.h` (a directory on disk), `conjure/core/vfs/memory_file_system.h`
(an in-memory tree), `conjure/core/vfs/mount_table.h` (routes schemes to backends) and
`conjure/core/vfs/path_utils.h` (lexical path helpers).

## At a glance

| You want to... | Use |
|---|---|
| Read a whole text or binary file | `files.read_text_file(path)`, `files.read_file(path)` |
| Write a file (and its parent directories) | `files.write_text_file(path, text)`, `files.write_file(path, bytes)` |
| Save without ever leaving a half-written file | `files.write_file_atomic(path, bytes)` |
| Check or describe an entry | `files.exists(path)`, `files.stat(path)` |
| List a directory | `files.enumerate(dir, flags, callback)` |
| Move, delete, create folders | `rename()`, `remove()`, `remove_all()`, `create_directories()` |
| Read parts of a big file | `files.open_read(path)` then `file::read_at(offset, buffer)` |
| Address files as `scheme://path` | `vfs::mount_table` with `mount("project", backend)` |
| Expose a folder on disk | `vfs::native_file_system(rootPath)` |
| Fake a file system in a test | `vfs::memory_file_system` |
| Split, join or normalise a path string | `vfs::path_utils::join()`, `normalize()`, `extension()`, ... |

## Quick example

```cpp
#include "conjure/core/vfs/vfs.h"

#include <filesystem>
#include <memory>

using namespace conjure;
using namespace conjure::core;

result<void> mount_project(vfs::mount_table &mounts, const std::filesystem::path &projectRoot) {
    CONJURE_TRY(mounts.mount("project", std::make_shared<vfs::native_file_system>(projectRoot)));
    CONJURE_TRY(mounts.mount("cooked", std::make_shared<vfs::memory_file_system>()));
    return {};
}

result<std::string> load_scene_text(vfs::file_system &files) {
    return files.read_text_file("project://scenes/main.scene");
}

result<void> save_settings(vfs::file_system &files, std::string_view jsonText) {
    return files.write_text_file("project://settings.json", jsonText);
}
```

The engine mounts `project://`, `assets://`, `engine://`, `cooked://` and `modules://` for you; game code
normally receives a `vfs::file_system &` (the mount table) and never touches `std::filesystem` directly.

## Paths and the error contract

Paths are UTF-8 and `/`-separated. Backslashes and a leading `/` are accepted and normalised, `""` names
the root of a backend, and a bare path is relative to that root. Every backend returns the same generic
error codes, so one error-handling path works everywhere:

| Code | Meaning |
|---|---|
| `NOT_FOUND` | the path (or the source of a rename, or an unmounted scheme) does not exist |
| `INVALID_ARGUMENT` | a directory where a file is required or vice versa; the root where an entry is required; a bare path given to a mount table |
| `ALREADY_EXISTS` | a file sits where `create_directories()` wants a directory; a directory sits where `rename()` wants to put a file |
| `INVALID_STATE` | `remove()` on a non-empty directory (use `remove_all()`) |
| `PERMISSION_DENIED` | the backend is read-only, or the path tries to escape the root |
| `OUT_OF_RANGE` | `file::read_at()` past the end of the file |
| `IO_FAILURE` | the operating system refused; the message carries its text |

> [!WARNING]
> Anything that would leave the backend root (`../x`, `/../x`, `C:/x`, `//server/x`) is
> `PERMISSION_DENIED`, even when the target exists on disk. A `native_file_system` is a sandbox: pass the
> directory you want reachable as its root instead of walking out of it.

## Reading and writing

`read_text_file()` drops a leading UTF-8 byte order mark and transcodes nothing else; `read_file()`
returns the raw bytes. Writes create missing parent directories and truncate an existing file.

```cpp
result<void> save_slot(vfs::file_system &files, u32 slot, std::span<const std::byte> state) {
    const std::string path = vfs::path_utils::join("project://saves", "slot" + std::to_string(slot) + ".bin");
    return files.write_file_atomic(path, state);
}

result<std::vector<std::byte>> load_slot(vfs::file_system &files, u32 slot) {
    const std::string path = vfs::path_utils::join("project://saves", "slot" + std::to_string(slot) + ".bin");
    return files.read_file(path);
}
```

> [!TIP]
> Use `write_file_atomic()` for anything a player would miss: save games, project files, settings. It
> writes to a temporary sibling and swaps it into place, so a crash or a concurrent reader never sees a
> partial file. `write_file()` is fine for scratch output and logs.

> [!NOTE]
> To turn text into bytes for `write_file()`, use `std::as_bytes(std::span<const char>(text))`; or call
> `write_text_file()`, which does exactly that.

## Listing directories

`enumerate()` reports entries in ascending path order on every backend, so listings are deterministic.
Each callback receives the entry's path (relative to the backend root, or a full URI when going through a
mount table) and its `file_stat`. With neither `FILES` nor `DIRECTORIES` set both kinds are reported;
`RECURSIVE` descends into subdirectories.

```cpp
result<std::vector<std::string>> list_scenes(vfs::file_system &files) {
    std::vector<std::string> scenes = {};
    const result<void> listed = files.enumerate(
      "project://scenes",
      vfs::enumerate_flags::RECURSIVE | vfs::enumerate_flags::FILES,
      [&scenes](std::string_view path, const vfs::file_stat &) {
          if (vfs::path_utils::has_extension(path, "scene")) {
              scenes.emplace_back(path);
          }
      }
    );
    if (!listed.has_value()) {
        return listed.get_error();
    }
    return scenes;
}
```

`file_stat` carries `size` (bytes, 0 for directories), `writeTimeNanoseconds` (comparable within one
backend), `isDirectory` and `isRegular`.

## Random access reads

`open_read()` returns a `std::unique_ptr<vfs::file>` whose `size()` is fixed at open time and whose
`read_at(offset, buffer)` may be called from several threads at once. An offset equal to the size reads
zero bytes; one past it is `OUT_OF_RANGE`.

```cpp
#include <algorithm>

result<std::vector<std::byte>> read_header(vfs::file_system &files, std::string_view uri, usize bytes) {
    CONJURE_TRY_ASSIGN(const std::unique_ptr<vfs::file> handle, files.open_read(uri));
    std::vector<std::byte> header(std::min<u64>(handle->size(), bytes));
    CONJURE_TRY_ASSIGN(const usize read, handle->read_at(0, header));
    header.resize(read);
    return header;
}
```

> [!NOTE]
> An open handle never blocks anyone: the file can be rewritten, replaced atomically, renamed over or
> removed while you hold it, on every platform. The handle keeps reading the contents it was opened on.

## Mounts and schemes

A `vfs::mount_table` is itself a `file_system` whose paths are URIs: `scheme://path`. `mount()` binds a
scheme (`"assets"` and `"assets://"` are interchangeable) to any backend held by `std::shared_ptr`; one
backend may sit behind several schemes, and a mount table may be mounted inside another. A bare path is
`INVALID_ARGUMENT` and an unmounted scheme is `NOT_FOUND`.

| Scheme | Content |
|---|---|
| `project://` | the open project: scenes, settings, source assets |
| `assets://` | imported assets |
| `engine://` | engine-provided content |
| `cooked://` | cooked output for the runtime |
| `modules://<name>/` | content shipped by a module |

```cpp
result<void> stage_scene(vfs::mount_table &mounts) {
    // Like every rename, this needs the destination folder to exist already.
    CONJURE_TRY(mounts.create_directories("cooked://scenes"));
    // Same backend: a real rename. Different backends: copy, then remove the source.
    CONJURE_TRY(mounts.rename("project://scenes/main.scene", "cooked://scenes/main.scene"));
    return {};
}
```

> [!WARNING]
> `rename()` across two different backends copies the file and removes the source; it works for files
> only (a directory is `UNSUPPORTED`). Within one backend, including one backend mounted under two
> schemes or reached through a nested mount table, it is a real rename and moves directories too.

Across backends `rename()` keeps the error contract of a real rename and checks it before copying
anything, against the backends that actually store the two files: the root of a backend at either end
is `INVALID_ARGUMENT`, a directory at the destination is `ALREADY_EXISTS`, a missing destination folder
is `NOT_FOUND` and a read-only source is `PERMISSION_DENIED`. The copy is written with
`write_file_atomic()`, so readers never see half of it.

> [!CAUTION]
> A move between backends is two steps, not one atomic rename. If the source cannot be removed after the
> copy, `rename()` returns that error and removes a copy that created a new file. A file the copy replaced
> is not restored: the destination then holds the new contents and the source is still there.

> [!WARNING]
> `find_mount()` returns a raw `file_system *` that is valid only until the scheme is unmounted. Prefer
> going through the mount table itself; operations in flight keep their backend alive even when another
> thread unmounts it.

`is_read_only()` on a mount table is true when at least one backend is mounted and all of them are
read-only. `schemes()` lists the mounted schemes in ascending order.

## A file system for tests

`vfs::memory_file_system` keeps the tree in memory, is safe to share between threads and never touches the
disk. Write times come from a counter that increases with every successful mutation (`write_counter()`),
so tests are deterministic. `set_read_only(true)` freezes a fixture once it is populated.

```cpp
TEST_CASE("manifest_loader - reads the project name") {
    vfs::memory_file_system files = {};
    CONJURE_REQUIRE_OK(files.write_text_file("manifest.json", R"({"name":"demo"})"));
    files.set_read_only(true);

    const result<std::string> text = files.read_text_file("manifest.json");
    CONJURE_REQUIRE_OK(text);
    CHECK(*text == R"({"name":"demo"})");
    CHECK(files.write_counter() == 1);
}
```

> [!TIP]
> Test file I/O against a `memory_file_system`, or against a `native_file_system` rooted in a
> `testing::temp_directory`. Code that takes `vfs::file_system &` works unchanged with either.

## Path utilities

`vfs::path_utils` is lexical only: nothing here touches the disk, and input backslashes come out as
forward slashes.

| Function | Example |
|---|---|
| `normalize(path)` | `"./a//b\\..\\c/"` -> `"a/c"`; drive letters and `//server/share` are preserved |
| `join(left, right)` | `join("a/", "b")` -> `"a/b"`; an absolute right side replaces the left |
| `parent(path)` | `"a/b/c.txt"` -> `"a/b"`; a root keeps its separator: `"/a"` -> `"/"`, `"C:/a"` -> `"C:/"` |
| `filename(path)` | `"a/b/c.txt"` -> `"c.txt"` |
| `stem(path)` | `"archive.tar.gz"` -> `"archive.tar"`; `".gitignore"` stays whole |
| `extension(path)` | `"texture.PNG"` -> `".png"` (lower-cased, with the dot) |
| `has_extension(path, ext)` | `has_extension("a.PNG", "png")` is true; the dot is optional |
| `is_absolute(path)` | true for `/a`, `C:/a`, `//server/share` |
| `relative_to(path, base)` | `relative_to("a/b/c", "a/x")` -> `"../b/c"` |

> [!NOTE]
> `relative_to()` compares path segments exactly; only a drive letter is compared ignoring case. A lexical
> helper cannot know whether the volume a path will be used on is case-sensitive.

## Threading

`native_file_system` holds no mutable state: one instance serves any number of threads. `memory_file_system`
and `mount_table` guard themselves with a mutex. `enumerate()` callbacks may call back into the same
file system. `file::read_at()` is safe from several threads on one handle.

## API summary

| Signature (short) | Purpose |
|---|---|
| `result<std::vector<std::byte>> file_system::read_file(string_view path)` | whole file as bytes |
| `result<std::string> file_system::read_text_file(string_view path)` | whole file as text, BOM dropped |
| `result<void> file_system::write_file(string_view path, span<const std::byte>)` | create or truncate |
| `result<void> file_system::write_file_atomic(string_view path, span<const std::byte>)` | write via a temporary and swap |
| `result<void> file_system::write_text_file(string_view path, string_view text)` | `write_file` for UTF-8 text |
| `result<file_stat> file_system::stat(string_view path)` / `bool exists(path)` | describe or test an entry |
| `result<void> file_system::enumerate(string_view dir, enumerate_flags, const enumerate_callback &)` | list entries in path order |
| `result<void> file_system::rename(string_view from, string_view to)` | move or rename |
| `result<void> file_system::remove(path)` / `remove_all(path)` / `create_directories(path)` | delete, delete recursively, mkdir -p |
| `result<std::unique_ptr<file>> file_system::open_read(string_view path)` | random-access handle |
| `bool file_system::is_read_only() const` | every mutation would be `PERMISSION_DENIED` |
| `result<usize> file::read_at(u64 offset, span<std::byte> out)` / `u64 file::size() const` | positional read |
| `native_file_system(std::filesystem::path root, bool readOnly = false)` | a directory on disk |
| `const path &native_file_system::root() const` / `path to_native(string_view) const` | map back to disk paths |
| `memory_file_system()` / `void set_read_only(bool)` / `u64 write_counter() const` | in-memory tree |
| `result<void> mount_table::mount(string_view scheme, shared_ptr<file_system>)` / `unmount(scheme)` | bind or unbind a scheme |
| `file_system *mount_table::find_mount(string_view scheme) const` / `vector<string> schemes() const` | inspect mounts |
| `static result<pair<string_view, string_view>> mount_table::split(string_view uri)` | `"assets://a/b"` -> `{"assets", "a/b"}` |
| `std::string path_utils::normalize(string_view)` / `join(left, right)` / `extension(path)` | lexical helpers |
| `string_view path_utils::parent(path)` / `filename(path)` / `stem(path)` | lexical helpers (views) |
| `bool path_utils::has_extension(path, ext)` / `is_absolute(path)` / `std::string relative_to(path, base)` | lexical helpers |

## See also

- 📘 API: [Errors and results](errors-and-results.md) for `result<T>`, `CONJURE_TRY` and the error codes above
- 📘 API: [JSON](json.md) to parse what you read and serialise what you write
- 📘 API: [Processes, libraries and images](system.md) for the other operating-system services in core
- 🛠 Handbook: [Core module](../../handbook/modules/core.md) for the backend internals, atomic replace and sharing semantics
