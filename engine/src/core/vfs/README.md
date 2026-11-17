# core/vfs

The virtual file system: one interface (`file_system`) over several backends, a scheme-based
mount table and lexical path helpers. Public headers live in `conjure/core/vfs/`; `vfs.h` is the
umbrella. The binding contract is `docs/design/10_core.md` section 2.9.

## Types

| Type | Header | Purpose |
|---|---|---|
| `file_stat`, `enumerate_flags`, `file`, `file_system` | `file_system.h` | the backend interface: `read_file`, `read_text_file`, `write_file`, `write_file_atomic`, `write_text_file`, `stat`, `exists`, `enumerate`, `rename`, `remove`, `remove_all`, `create_directories`, `open_read`, `is_read_only` |
| `native_file_system` | `native_file_system.h` | a directory on disk; `root()`, `to_native()` (checked: `result<path>`); rejects root escapes |
| `write_private_file`, `is_private_file` | `native_file_system.h` | free functions over an on-disk path: a file only the user the process runs as may open, and the check for it |
| `memory_file_system` | `memory_file_system.h` | an in-memory tree; `set_read_only()`, `write_counter()` |
| `mount_table` | `mount_table.h` | routes `scheme://path` to backends; `mount`, `unmount`, `find_mount`, `schemes`, `split` |
| `path_utils` | `path_utils.h` | `normalize`, `join`, `parent`, `filename`, `stem`, `extension`, `has_extension`, `is_absolute`, `relative_to`, `is_portable_name` |

## Path and error contract

Paths are UTF-8 and `/`-separated; backslashes and a leading `/` are accepted and normalised, and
`""` names the backend root. Anything that would leave the root (`../x`, `C:/x`, `//server/x`) is
`PERMISSION_DENIED`; `native_file_system::to_native` applies the same check, so the on-disk path it
returns is always the root or below it. Every backend returns the same generic `core::error_code`s, documented at the
top of `file_system.h`: `NOT_FOUND`, `INVALID_ARGUMENT` (wrong kind of entry, the root where an
entry is required), `ALREADY_EXISTS`, `INVALID_STATE` (non-empty directory), `PERMISSION_DENIED`
(read-only backend or escape), `OUT_OF_RANGE` (`read_at` past the end) and `IO_FAILURE` (anything the
operating system refused; the message carries its text). `enumerate` reports entries in ascending
path order on every backend so results are deterministic; its paths are root-relative, or full
URIs when enumerating through a `mount_table`.

Names of new entries are portable (code sweep F753, `docs/design/10_core.md` C12). Wherever a call
creates an entry (`write_file`, `write_file_atomic`, `write_text_file`, `create_directories`, the
destination of `rename`) every segment of the normalised path must be a name every platform takes:
no control character (U+0000 to U+001F, U+007F) and none of `: * ? " < > |`, no trailing dot or
space, no Windows device name (`CON`, `PRN`, `AUX`, `NUL`, `COM1` to `COM9`, `LPT1` to `LPT9`, in any
case and with any extension). Anything else is `INVALID_ARGUMENT` and nothing is written. The rule
is lexical and the same on every backend and platform: a segment that exists already is judged too,
and a refusal ranks after the escape and read-only checks (`PERMISSION_DENIED`) and before any
lookup, so `rename` of a missing source onto such a name is `INVALID_ARGUMENT`. `read_file`,
`read_text_file`, `open_read`, `stat`, `exists`, `enumerate`, `remove`, `remove_all` and the source
of `rename` judge no name: an entry that carries one already (made on another platform or by
another program) can be read, renamed to a portable name and removed. A path whose second character
is `:` is a drive letter and stays an escape. `path_utils::is_portable_name` is the public form of
the rule for one name.

`write_file_atomic` never shows a partial file, to a concurrent reader or after a crash, and on the
native backend it is durable (code sweep F243, C11): the new contents are flushed to the storage
device before they take the target's name, so after a power cut the path holds the old contents or
the new ones. That costs one device flush per call (milliseconds); `write_file` does not flush.

## Implementation notes

- `native_file_system::to_native` is the public, checked mapping: it returns `resolve(path)`, the very
  resolution `read_file`, `stat` and `open_read` start with, so an escape (`..` above the root, a
  drive letter, a UNC prefix) is `PERMISSION_DENIED`; a single leading separator means the root, so
  `/a/b.txt` and a POSIX absolute path map below it. The caller may hand the result to code outside
  the file system (a process, a dynamic library). It is lexical: the path need not exist and a
  read-only instance maps like any other. The unchecked mapping is the private `native_path_of`,
  which only ever receives a path `internal::resolve_backend_path` accepted (code sweep F241: the
  public mapper used to skip the check, an open door in a class that advertises a sandbox).
- One validator serves every backend: `internal::validate_new_path` (`vfs_internal.h`) walks the
  segments of a normalised path without allocating and returns `INVALID_ARGUMENT` naming the path as
  the caller wrote it and the first segment `internal::portable_name_problem` objects to, with the
  reason. The native backend calls it in `resolve_writable` (both writes), `create_directories` and
  `rename` (destination only), the memory backend in the same three places, and the mount table's
  cross-backend `rename` before it looks up either file. Before F753 only a drive
  letter or UNC prefix was refused: on Windows `dir/notes.txt:draft` wrote an alternate data stream
  (leaving a 0-byte `notes.txt` when the file was new), `report.` landed as `report`, `nul` went
  to the device, and the memory backend took all of them, so a test could pass with a name no
  project folder on Windows can hold. A device name is recognised the way Win32 does: the part
  before the first dot, trailing spaces dropped, case ignored.
- `write_file_atomic` writes its temporary through `internal::write_file_flushed`
  (`native_platform.h`): `CreateFileW(CREATE_ALWAYS)`, `WriteFile` in pieces of at most 1 GiB and
  `FlushFileBuffers` on Windows; `open(O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC)`, `write` and `fsync`
  on every POSIX platform (on macOS `fsync` hands the data to the drive without waiting for the
  drive's own write cache; the stronger `fcntl(F_FULLFSYNC)` is not used, decision C11). Only then
  does `internal::replace_file` rename the
  temporary over the target. Before F243 the temporary was a closed `std::ofstream`: the rename
  could reach the disk before the data, and a power cut in between left an empty or torn file under
  the target's name with the old contents gone. The directory entry is not flushed (decision C11):
  if the rename is lost the old file is still there, which is one of the two allowed outcomes; the
  temporary may then be left behind (`<path>.tmp-<uuid>`, which the asset scan never takes for a
  source). A flush the driver cannot do at all (`internal::flush_is_unsupported`:
  `ERROR_INVALID_FUNCTION`, `ERROR_NOT_SUPPORTED`; `EINVAL`, `ENOTSUP`) keeps the file without the
  guarantee and logs one WARN per process through `internal::note_unflushable_volume`, so a project
  on such a volume can still be saved; every other flush error is `IO_FAILURE`, the temporary is
  removed and the target is untouched. `internal::flushed_file_count()` is the number of files the
  process flushed, kept in `native_platform.cpp` for the tests and the benchmark, next to
  `internal::unflushable_volume_reported()`, which tells a test whether that one WARN was already
  spent in its process. Plain `write_file`
  still goes through `std::ofstream` and does not flush. The measured price is in
  `tests/benchmarks/README.md` (`assets.first_scan_100`).
- `native_file_system` uses only the `std::error_code` overloads of `std::filesystem` for
  directory work; nothing throws. Reading goes through `native_platform.h`: `open_read_handle`
  (`CreateFileW` with `FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE` on Windows,
  `open(O_RDONLY | O_CLOEXEC)` on POSIX) and positional `read_at` (`ReadFile` with an `OVERLAPPED`
  offset, `pread`). The delete sharing is the point: a `std::ifstream` reader would make every
  rename over or removal of the file fail on Windows for as long as it is open. `write_file_atomic`
  writes `<path>.tmp-<uuid>` next to the target (the temporary goes through `extend_native_path` on
  its own: it is 41 characters longer than the target, so a target just under the classic Windows
  limit has a temporary beyond it) and swaps it in with `internal::replace_file`:
  `SetFileInformationByHandle(FileRenameInfoEx)` with `FILE_RENAME_FLAG_POSIX_SEMANTICS` (falling
  back to `MoveFileExW(MOVEFILE_REPLACE_EXISTING)` on volumes without it, recognised by the codes in
  `internal::rename_falls_back_to_move`: `ERROR_INVALID_FUNCTION` from SMB/NAS redirectors and some
  FAT/exFAT drivers, `ERROR_INVALID_PARAMETER`, `ERROR_NOT_SUPPORTED`; never `ERROR_ACCESS_DENIED`,
  which also means an ACL refusal that `MoveFileExW` would hit too) in
  `native_platform_windows.cpp`, `std::filesystem::rename` in `native_platform_posix.cpp` (CMake
  picks the file). `rename` onto an existing file goes through the same helper; a rename onto
  another spelling of the same entry (a case-only rename on a case-insensitive volume) is handed
  to `std::filesystem::rename` directly. While a file's last name is being removed and a reader
  still holds it open, Windows answers lookups with "access denied" for an instant; `stat` and
  `open_read` recognise that state (a zero-access probe open fails too) and report `NOT_FOUND`, so
  readers racing a `remove` see the same codes as on POSIX. UTF-8 paths are converted with
  `std::u8string` so Windows receives wide paths.
- Paths beyond the classic Windows limit: single-entry calls go through `extend_native_path`, which
  switches to the extended-length form (`\\?\`) from 248 characters on. A path with a segment that
  is not a portable name takes that form whatever its length (`native_path_of` asks
  `internal::validate_new_path`), because Win32 takes only that form literally: in the classic one
  it drops a trailing dot or space, so `stat`, `read_file` and `rename` of an existing `report.`
  acted on its neighbour `report` (`NOT_FOUND` without one) and `remove_all` of `logs.` removed
  `logs`; the leniency C12 promises held only for a root given in the extended form. Measured on
  Windows 11, which reaches a file named `aux` or `nul.txt` inside a directory in either form;
  earlier versions open the device for such a name in the classic form. The root is extended first
  and the relative part appended afterwards: `std::filesystem::absolute` on the whole path would do
  the same rewriting. `to_native` returns that form for such a path too. Directory walks
  (`enumerate`, `remove_all`) append components below their root as they descend, so they start
  from `always_extended_native_path` instead, whatever the root's length: one asset nested deeper
  than the limit below a short project root cannot abort the whole scan. POSIX returns both paths
  unchanged.
- `file_time_to_nanoseconds` converts with `clock_cast` (MSVC, libstdc++) or, on libc++ whose
  file clock counts from the Unix epoch, with `file_clock::to_sys`. Neither reads a clock, so an
  unchanged file reports the same write time on every `stat` and `enumerate`; consumers compare
  write times for equality.
- `memory_file_system` stores a tree of `node`s. File contents are immutable
  `shared_ptr<const vector<byte>>`s: a write swaps the pointer, so files opened with `open_read`
  keep reading the snapshot they were opened on. Write times are the value of a per-instance
  counter incremented on every successful mutation.
- `mount_table` validates schemes (letters, digits, `-`, `_`, `.`, `+`), accepts `assets` and
  `assets://` interchangeably for `mount`/`unmount`/`find_mount`, refuses to mount a mount table
  that already contains it (directly or through other tables), and moves files between mounts by
  copy-and-remove (directories are `UNSUPPORTED`). One backend may sit behind several schemes,
  directly or through nested mount tables. Before deciding, `rename` follows both URIs down to the
  backends that store them (`route_to_storage`), because a nested table hides its backends behind
  URIs; it then decides "same backend" by identity, not by scheme, so such a rename is a real
  rename and a file renamed onto itself through two routes is never copied and removed.
- The cross-backend `rename` keeps the base contract and checks it, against those storing
  backends, before copying and in the order of a backend's own `rename`, so one call gives one code
  whichever route it takes: an escape at either end, then a read-only backend at either end
  (`PERMISSION_DENIED`; for the source because the copy would otherwise land and only the removal
  fail, and a nested table's own `is_read_only()` would be false while any of its mounts is
  writable), then the root of a backend at either end (`INVALID_ARGUMENT`; `stat` reports a root as
  a directory), then the name of the destination (`internal::validate_new_path`,
  `INVALID_ARGUMENT`). Only after those come the lookups: a missing source is `NOT_FOUND`, a
  directory source `UNSUPPORTED`, a directory at `to` `ALREADY_EXISTS` and a missing parent of `to`
  `NOT_FOUND` (`write_file_atomic` would answer `INVALID_ARGUMENT` and create the parent;
  `look_up_copy_destination`). A missing source renamed onto a name that is not portable is
  therefore `INVALID_ARGUMENT` across backends as within one.
  The parent is taken from the normalised path, because `path_utils::parent` cannot take a URI
  apart. The copy goes through `write_file_atomic`. When removing the source fails, a copy that
  created a new file is removed again (a `WARN` on `LOG_CORE` if that fails too); a replaced file
  is not restored.
- `path_utils::parent` keeps a root's separator: `"/a"` -> `"/"`, `"C:/a"` -> `"C:/"` (the bare
  `"C:"` would mean "the current directory of drive C").

## Private files

`write_private_file(path, data)` is for a secret that other accounts of the machine must not read; its first
caller is the editor's AI-protocol token (`docs/design/04_editor.md` D23, code sweep F740,
`09_cross_area_resolutions.md` R66). It takes an on-disk path, not a VFS path: the caller is outside any
mount. The data never passes through a file with wider access:

- `internal::create_private_file` (`native_platform.h`) creates a temporary beside the target with the
  restriction in place from the first instant. POSIX: `open(O_CREAT | O_EXCL, 0600)`, then `fchmod(0600)` so a
  strict umask does not take the owner's own bits away. Windows: `CreateFileW(CREATE_NEW)` with a security
  descriptor built from the process token's user: owner that user, one access-allowed entry for that user,
  `SE_DACL_PROTECTED` so the folder's inheritable entries stay out.
- The result is then checked on the open handle, before a byte of data is written: POSIX `fstat` (regular
  file, owned by the effective user, no group or other bits); Windows `FILE_PERSISTENT_ACLS` on the volume and
  the descriptor read back with `GetSecurityInfo`. A volume that dropped the restriction (FAT, exFAT, a mount
  with fixed permissions) answers `UNSUPPORTED` and the temporary is removed.
- `internal::replace_file` renames the temporary over the target, so a reader sees the old file or the new
  one, and a symbolic link or a file with wider permissions at the target is replaced, not written through.

`is_private_file(path)` applies the same check to an existing entry without following a link. What neither
function covers: accounts the operating system lets pass any permission (root, administrators, backup
operators), access control lists beyond the mode bits on POSIX (macOS), and the folder, which whoever may write
it can use to replace or delete the file. Nothing is flushed to the device: a secret that is regenerated at
every start need not survive a power cut.

## Threading

- `native_file_system` holds no mutable state; one instance can be used from any thread (the flush
  statistics of `native_platform.cpp` are two process-wide atomics). `file`
  handles returned by `open_read` read positionally and need no lock. An open handle never blocks
  `write_file_atomic`, `rename` or `remove` of the same file on any platform (the handle keeps the
  old contents readable).
- `memory_file_system` guards every operation with one mutex; `enumerate` collects entries under
  the lock and invokes the callback after releasing it, so callbacks may call back into the file
  system. `set_read_only` is an atomic flag.
- `mount_table` guards its map with a mutex and copies the backend `shared_ptr` out before
  forwarding, so an operation in flight survives a concurrent `unmount`; no backend method is ever
  called while the mutex is held (`is_read_only` snapshots the backends first). `find_mount`
  returns a raw pointer that is valid only until the scheme is unmounted.

## Testing

`tests/core/vfs/` builds `conjure_core_vfs_tests`:

```
python tools/build.py --build-dir build/core_vfs --target conjure_core_vfs_tests --test -R core_vfs
```

`test_file_system_conformance.cpp` runs every behavioural case against both backends through
`TEST_CASE_TEMPLATE`, including readers holding `open_read` handles while the file is replaced,
renamed over and removed, case-only renames and the names of new entries (every refused form for
every creating call, the names that only resemble one, the leniency of the reading and removing
calls, the rank of the refusal); the other files cover the interface defaults, the native specifics
(root escapes, UTF-8 names, atomic writes leaving no temporary and flushing exactly once, the
flushing writer itself, entries with names that cannot be created being read, moved and removed
under an ordinary root and under one in the extended-length form, a dotted name next to its plain
neighbour, write times, private files: content, the restriction read back, a file open to others and a
symbolic link being replaced, the refusals), `path_utils::is_portable_name`, the
memory specifics (counter semantics, snapshots, concurrent writers and readers) and the mount
table (routing, error pass-through, cross-mount rename with its error codes and their order, which is
that of a rename within one backend, nested tables and the
undo of a copy whose source cannot be removed, one backend under two schemes or reached through a
nested table, mount cycles, concurrent mount changes).
