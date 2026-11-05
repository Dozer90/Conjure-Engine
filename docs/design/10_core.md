# 10 - Core module design

Status: **binding** (owner: lead architect). Implements `ARCHITECTURE.md` 4.1 and the "Needs from core"
paragraphs of `01_ecs.md` ... `08_testing.md`, reconciled with the code that already exists in
`engine/src/core/foundation/`. Where an area document spells a core name differently, **this document
and the code win**; the area documents are corrected by the wave 1 documentation pass.

## 1. Overview and decisions

| Id | Decision |
|---|---|
| C1 | Core is one module (`conjure::core`) made of independently buildable **areas**, each a static library `conjure::core_<area>` with its own test executable. Areas: `foundation`, `math`, `containers`, `strings`, `memory`, `time`, `jobs`, `json`, `vfs`, `system`, `reflect`. Public headers live flat in `engine/include/conjure/core/` except the large areas `jobs`, `json`, `vfs`, `reflect`, `memory`, `containers`, `math`, which use `conjure/core/<area>/<name>.h`. |
| C2 | Area dependencies (public link): every area -> `foundation`; `memory` -> none else; `strings` -> none else; `containers` -> none else; `math` -> glm; `time` -> none else; `jobs` -> `containers`, `memory`; `json` -> yyjson (private), `containers`; `vfs` -> `containers`, `strings`; `system` -> stb (private); `reflect` -> `json`, `strings`, `math`, `containers`. No cycles. |
| C3 | Errors: `core::error {category, code, message}`; each module binds `enum class <module>_error : u16` with `CONJURE_DECLARE_ERROR_CATEGORY(enum, "name", name_fn)`. Core areas that fail at runtime (`json`, `vfs`, `system`, `jobs`) use the generic `core::error_code` and do **not** declare their own category. (Already implemented in `error.h`.) |
| C4 | No exceptions, no `new`/`delete` outside `memory`, no `std::thread` outside `jobs` (and `system::process`'s pipe reader, which is a `jobs::dedicated_thread`). |
| C5 | Hashing is FNV-1a (`hash.h`, implemented). `string_id` is the 64-bit FNV-1a of the name; interning is debug-only metadata. |
| C6 | Public headers of core never include a third-party header; glm is the single exception through `conjure/core/math/math.h`. yyjson, stb, OS headers stay in `src/`. |
| C7 | Every API below is headless and deterministic given the same inputs; `jobs` with 0 workers runs everything inline on the caller. |
| C8 | Struct fields camelCase, class members `mCamelCase` (R31). Flags are `enum class <x>_flags` + `CONJURE_ENUM_FLAGS(...)`. |

## 2. Area contracts

Each subsection lists the public headers (all under `engine/include/conjure/core/`), the types with their
key signatures, behaviour that is binding, and the mandatory test file names (`tests/core/<area>/test_<header>.cpp`,
one per public header).

### 2.1 foundation (implemented; wave 1 only maintains it)

Headers: `platform_detect.h`, `types.h`, `api.h`, `build_info.h` (generated), `hash.h`, `error.h`, `result.h`,
`assert.h`, `log.h`, `uuid.h`, `enum_flags.h`, `non_copyable.h`. See the headers; they are documented.
Known mapping to the area documents: `add_log_sink/remove_log_sink/log_sink_handle` (04, 08) = `log::add_sink(std::shared_ptr<log_sink>)`
/ `log::remove_sink(...)`; `uuid::is_nil()` (05) = `uuid::is_null()`; `error_code::IO_ERROR/PARSE_FAILED` = `IO_FAILURE/PARSE_FAILURE`;
`CONJURE_DEFINE_LOG_CATEGORY(name)` takes a second display-name argument: `CONJURE_DEFINE_LOG_CATEGORY(LOG_ECS, "ecs")`.

### 2.2 math (`conjure/core/math/`)

Headers: `math.h` (umbrella + glm aliases), `transform.h`, `aabb.h`, `frustum.h`, `ray.h`, `rect.h`, `math_utils.h`.

```cpp
namespace conjure::math {
    using vec2 = glm::vec2;  using vec3 = glm::vec3;  using vec4 = glm::vec4;
    using ivec2 = glm::ivec2; using ivec3 = glm::ivec3; using uvec2 = glm::uvec2; using uvec3 = glm::uvec3;
    using quat = glm::quat;  using mat3 = glm::mat3;  using mat4 = glm::mat4;
    inline constexpr f32 PI = 3.14159265358979323846f;  inline constexpr f32 EPSILON = 1.0e-6f;
    [[nodiscard]] constexpr f32 radians(f32 degrees) noexcept; [[nodiscard]] constexpr f32 degrees(f32 radians) noexcept;
    [[nodiscard]] bool nearly_equal(f32 a, f32 b, f32 epsilon = EPSILON) noexcept;  // also vec2/3/4, quat, mat4
    [[nodiscard]] constexpr u32 align_up(u32 value, u32 alignment) noexcept;        // also usize, u64; power-of-two helpers
    [[nodiscard]] constexpr bool is_power_of_two(u64 value) noexcept;

    // Right-handed, Y up, -Z forward (glTF convention). Depth 0..1 (GLM_FORCE_DEPTH_ZERO_TO_ONE).
    // Quaternion storage is x, y, z, w (glm 1.0.1 default; never define GLM_FORCE_QUAT_DATA_WXYZ, and do not define
    // GLM_FORCE_QUAT_DATA_XYZW either because it changes the constructor to (x, y, z, w)). C# mirrors memcpy quats, so
    // math.h must static_assert(offsetof(quat, x) == 0 && offsetof(quat, w) == 12) and test_math.cpp repeats it (R58).
    struct transform {                       // trivially copyable; layout mirrored in C# (float x10)
        vec3 position = vec3(0.0f);
        quat rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);   // identity (w, x, y, z)
        vec3 scale = vec3(1.0f);
        [[nodiscard]] mat4 to_matrix() const noexcept;
        [[nodiscard]] static transform from_matrix(const mat4 &matrix) noexcept;   // decomposes TRS (no shear)
        [[nodiscard]] transform operator*(const transform &child) const noexcept;  // parent * child
        [[nodiscard]] transform inverse() const noexcept;
        [[nodiscard]] vec3 forward() const noexcept; right(); up();
        [[nodiscard]] vec3 transform_point(vec3) const noexcept; transform_direction(vec3) const noexcept;
        [[nodiscard]] static transform look_at(vec3 eye, vec3 target, vec3 up) noexcept;
        [[nodiscard]] static transform lerp(const transform &a, const transform &b, f32 t) noexcept; // slerp rotation
    };
    static_assert(sizeof(transform) == 40 && std::is_trivially_copyable_v<transform>);

    struct aabb { vec3 min = vec3(FLT_MAX); vec3 max = vec3(-FLT_MAX);
        [[nodiscard]] bool is_valid() const noexcept; vec3 center() const noexcept; vec3 extents() const noexcept; vec3 size() const noexcept;
        void expand(vec3 point) noexcept; void expand(const aabb &other) noexcept;
        [[nodiscard]] bool contains(vec3 point) const noexcept; bool intersects(const aabb &other) const noexcept;
        [[nodiscard]] aabb transformed(const mat4 &matrix) const noexcept;   // 8-corner method
        [[nodiscard]] f32 surface_area() const noexcept;
        [[nodiscard]] static aabb from_center_extents(vec3 center, vec3 extents) noexcept; };

    struct plane { vec3 normal = vec3(0, 1, 0); f32 distance = 0; [[nodiscard]] f32 signed_distance(vec3 point) const noexcept;
        [[nodiscard]] static plane from_point_normal(vec3 point, vec3 normal) noexcept; void normalize() noexcept; };

    struct frustum { std::array<plane, 6> planes = {};   // LEFT, RIGHT, BOTTOM, TOP, NEAR, FAR (enum class frustum_plane : u8)
        [[nodiscard]] static frustum from_view_projection(const mat4 &viewProjection) noexcept;  // Gribb-Hartmann, 0..1 depth
        [[nodiscard]] bool intersects(const aabb &box) const noexcept;   // conservative, p-vertex test
        [[nodiscard]] bool intersects_sphere(vec3 center, f32 radius) const noexcept;
        [[nodiscard]] bool contains(vec3 point) const noexcept; };

    struct ray { vec3 origin = vec3(0); vec3 direction = vec3(0, 0, -1);   // direction normalised by callers
        [[nodiscard]] vec3 at(f32 t) const noexcept;
        [[nodiscard]] std::optional<f32> intersect(const aabb &box) const noexcept;         // slab method, t >= 0
        [[nodiscard]] std::optional<f32> intersect_sphere(vec3 center, f32 radius) const noexcept;
        [[nodiscard]] std::optional<f32> intersect_plane(const plane &plane) const noexcept;
        [[nodiscard]] std::optional<f32> intersect_triangle(vec3 a, vec3 b, vec3 c, bool cullBackFaces = true) const noexcept; // Moller-Trumbore
        [[nodiscard]] static ray from_viewport(vec2 pixel, uvec2 viewportSize, const mat4 &inverseViewProjection) noexcept; };

    struct rect { vec2 min = vec2(0); vec2 max = vec2(0); ... contains/intersects/size/center };   // float
    struct urect { u32 x = 0; u32 y = 0; u32 width = 0; u32 height = 0; };                         // viewport/scissor

    // Projection helpers (depth 0..1, right-handed): perspective(fovYRadians, aspect, near, far), orthographic(l, r, b, t, near, far),
    // infinite_perspective(fovY, aspect, near), look_at(eye, target, up) -> mat4 (view matrix).
}
```
`std::formatter` for vec2/3/4, quat, mat4, transform, aabb. Tests: `test_math.cpp`, `test_transform.cpp`, `test_aabb.cpp`,
`test_frustum.cpp`, `test_ray.cpp`, `test_rect.cpp`, `test_math_utils.cpp` (include frustum culling against a known
projection, TRS round trips, transform composition against glm matrices, ray/aabb edge cases: parallel, inside, behind).

### 2.3 containers (`conjure/core/containers/`)

Headers: `slot_map.h`, `small_vector.h`, `stable_vector.h`, `flat_hash_map.h`, `flat_map.h`, `containers.h` (umbrella).

- `template <typename T, typename Key = slot_key> class slot_map`: `slot_key {u32 index; u32 generation;}` (8 bytes,
  `is_valid()`, null = all zero, `to_u64/from_u64`); `insert(T&&) -> slot_key`, `emplace(args...)`, `erase(key) -> bool`,
  `find(key) -> T*` (nullptr for stale/invalid), `contains`, `size`, `capacity`, `reserve`, `clear` (bumps generations),
  iteration over live elements in storage order (`for (auto &[key, value] : map)`), dense storage with swap-remove and an
  indirection table (lookup is O(1) and allocation-free). Generation 0 never issued; generation wrap retires the slot.
- `template <typename T, usize N> class small_vector`: inline storage for N elements, heap beyond; `std::vector`-like
  API subset (`push_back`, `emplace_back`, `pop_back`, `insert`, `erase`, `resize`, `reserve`, `clear`, `data`, `size`,
  `capacity`, `empty`, `front/back`, `operator[]`, `at` asserting, iterators, `std::span` conversion, `is_inline()`);
  trivially relocatable types use memcpy; copy/move correct for non-trivial T.
- `template <typename T, usize ChunkSize = 64> class stable_vector`: elements never move (chunked storage);
  `emplace_back -> T&`, `operator[]`, `size`, iteration; used by assets for `asset_slot`.
- `template <typename K, typename V, typename Hash = std::hash<K>, typename Eq = std::equal_to<K>> class flat_hash_map`:
  open addressing (robin hood or linear probing with backward-shift deletion), power-of-two capacity, load factor 0.875
  max, `find`, `contains`, `insert`, `try_emplace`, `insert_or_assign`, `erase(key)`, `erase(iterator)`, `operator[]`,
  `reserve`, `clear`, `size`, iteration over `std::pair<const K, V>`-like values (`key()`/`value()` accessors are fine);
  pointers/iterators invalidated by rehash only. Also `flat_hash_set<K>`.
- `template <typename K, typename V, typename Compare = std::less<K>> class flat_map`: sorted `std::vector` of pairs,
  binary search, same insertion API subset; good for small maps and deterministic iteration order.

Tests: one file per header plus property-style tests against `std::unordered_map`/`std::vector` oracles with a seeded
PRNG (`std::mt19937_64(0x5EED)`), stale-key semantics, non-trivial element types (counting constructors/destructors,
asserting zero leaks), and `CONJURE_CHECK_NO_ALLOCATIONS`-style checks once `memory` lands (optional in stage 1).

### 2.4 strings (`conjure/core/`)

Headers: `string_id.h`, `string_utils.h`, `bit_vector.h`, `radix_sort.h`.

- `class string_id`: `constexpr string_id(std::string_view name)` (FNV-1a 64), `constexpr string_id() = default`
  (null, hash 0), `hash() -> u64`, `is_null()`, `operator<=>`/`==` by hash, `std::hash`, `std::formatter` (prints the
  interned name when known, else `#<hex>`), `c_str()`/`view()` returning the interned name or "" when unknown.
  Interning: runtime-constructed ids register their name in a global, mutex-guarded table **in every configuration**
  (names are needed by the editor and by modules resolving component names); `constexpr` ids constructed in constant
  expressions are not interned until `intern()` is called or they are first formatted via a runtime construction.
  `string_id::intern(std::string_view) -> string_id` always interns. Collision detection: interning a different
  name with the same hash asserts and logs ERROR (`LOG_CORE`).
  Literal operator: `"ecs.component"_sid` in `inline namespace literals`.
- `string_utils.h` (`namespace conjure::core::str`): `to_lower/to_upper` (ASCII), `trim`, `starts_with/ends_with`
  (case-insensitive variants), `split(std::string_view, char) -> small_vector<std::string_view, 8>`, `join`,
  `replace_all`, `iequals`, `parse_int/parse_float -> std::optional`, `format_bytes(u64) -> "1.5 MiB"`, UTF-8 <->
  UTF-16 conversion (`to_wide/from_wide`, Windows paths), `is_valid_identifier`.
- `class bit_vector`: dynamic bitset: `resize`, `set/reset/test`, `any/none/all`, `count`, `find_first_set(from)`,
  `find_first_clear(from)`, bitwise ops with another `bit_vector` of equal size, iteration over set bits.
- `radix_sort.h`: `void radix_sort_u64(std::span<u64> keys, std::span<u32> values, std::span<u64> scratchKeys,
  std::span<u32> scratchValues)` (LSD, 8 passes of 8 bits or adaptive 11-bit passes; stable; caller-provided
  scratch so it is allocation-free); overload sorting keys only; `radix_sort_u32`.

Tests: `test_string_id.cpp` (constexpr, interning, collision assert via `assert_capture`, formatter, thread-safety of
interning from 8 threads), `test_string_utils.cpp`, `test_bit_vector.cpp`, `test_radix_sort.cpp` (random arrays vs
`std::stable_sort`, already sorted, all equal, empty, 1 element, 100k elements).

### 2.5 memory (`conjure/core/memory/`)

Headers: `allocation_counters.h`, `tracked_allocation.h`, `arena.h`, `pool_allocator.h`, `frame_allocator.h`, `memory.h` (umbrella).

- `struct allocation_counters { std::atomic<u64> allocationCount; std::atomic<u64> freeCount; std::atomic<u64> allocatedBytes; }`
  with `allocation_counters &counters() noexcept` (process-wide) and `memory_snapshot snapshot() noexcept`
  (`struct memory_snapshot { u64 allocationCount; u64 freeCount; u64 allocatedBytes; }`). Counting is cumulative.
- `void *tracked_allocate(usize bytes, usize alignment = alignof(std::max_align_t)) noexcept` /
  `void tracked_free(void *pointer, usize bytes) noexcept` (aligned via `_aligned_malloc`/`aligned_alloc`
  behind `CONJURE_PLATFORM_*`), `tracked_reallocate`. These bump the counters; everything in the engine that needs
  heap memory outside STL containers goes through them. The test support library's `tracked_global_new.cpp`
  (wave 1 stage 2) routes global `operator new/delete` through them in test executables only.
- `class arena`: `explicit arena(usize blockBytes = 64 * KIB)`; `void *allocate(usize bytes, usize alignment) noexcept`
  (bump within the current block; a request larger than a block gets its own block); `template <typename T, typename... Args> T *construct(Args &&...)`
  (trivially destructible T only, asserted); `reset()` (keeps the first block, frees the rest, addresses invalid);
  `release()` frees everything; `bytes_allocated()`, `bytes_reserved()`, `block_count()`. Not thread-safe.
- `class pool_allocator`: `pool_allocator(usize blockSize, usize alignment, u32 blocksPerSlab)`; `void *allocate() noexcept`
  (returns nullptr when a new slab cannot be obtained; grows by slabs otherwise), `void deallocate(void *block) noexcept`
  (asserts the pointer belongs to the pool in Debug), `template <typename T> T *allocate_as()`, `live_count()`,
  `capacity()`, `slab_count()`, `shrink_to_fit()` (frees fully free slabs). Not thread-safe; the ECS wraps it under its lock.
  Optional `max_slabs` cap in the constructor so `allocate()` exhaustion can be tested.
- `class frame_allocator`: `frame_allocator(usize arenaBytes, usize threadBlockBytes)`; `void *allocate(usize bytes, usize alignment) noexcept`
  thread-safe and lock-free in the common case: each thread owns a `thread_local` block carved from the arena with one
  atomic bump; when the arena is exhausted it falls back to `tracked_allocate` and increments `overflow_count()`
  (overflow blocks are freed at reset). `template <typename T> std::span<T> allocate_span(usize count)`, `construct<T>`;
  `void reset()` (not thread-safe: callers guarantee no concurrent `allocate`; invalidates every thread-local block
  via an epoch counter so stale `thread_local` pointers are never reused); `high_water_bytes()`, `capacity_bytes()`,
  `overflow_count()`, `epoch()`. Implementation note: the `thread_local` slot stores `{frame_allocator *owner, u64 epoch, byte *cursor, byte *end}`;
  a mismatch of owner or epoch means "grab a new block".
- All allocators report their block acquisitions through the counters.

Tests: one per header; `frame_allocator` tested with `std::thread`s **in the test only** (allowed: the jobs area may not
exist yet) proving no two threads receive overlapping ranges and that `reset()` + epoch bump invalidates blocks;
pool exhaustion returns nullptr; arena alignment up to 256; counters monotonic.

### 2.6 time (`conjure/core/`)

Headers: `clock.h`, `random.h`, `event.h`, `delegate.h`, `event_bus.h`.

- `class clock { public: virtual ~clock(); [[nodiscard]] virtual f64 now_seconds() const noexcept = 0; virtual void sleep_until(f64 seconds) = 0; }`;
  `class steady_clock final : public clock` (std::chrono::steady_clock; `sleep_until` sleeps until 2 ms before the
  deadline then spins); `class manual_clock final : public clock` (`advance(f64 seconds)`, `set(f64)`, `sleep_until`
  advances the clock); `[[nodiscard]] u64 ticks_now() noexcept` / `f64 ticks_to_seconds(u64)` for profiling;
  `struct stopwatch { start(); elapsed_seconds(); restart(); }`.
- `random.h`: `using default_engine = std::mt19937_64;` `default_engine &random::default_engine() noexcept`
  (thread-local, seeded from `random::seed(u64)` which sets the seed every thread uses on first use, default
  `0x5EED`); helpers `random::uniform(engine, min, max)` for integral and floating types, `random::unit_vector3(engine)`,
  `random::chance(engine, probability)`; `class random_engine` wrapper around `default_engine` with `seed()`.
- `template <typename... Args> class event`: multicast; `[[nodiscard]] event_subscription subscribe(std::function<void(Args...)>)`;
  `void emit(Args...) const`; `subscriber_count()`; subscriptions are RAII (`event_subscription` is move-only,
  `release()`, `is_active()`; destroying it unsubscribes; destroying the event first leaves subscriptions inert).
  Emission is reentrancy-safe: subscribing/unsubscribing inside a handler takes effect after the emit. Not thread-safe
  (documented; main-thread objects).
- `template <typename Signature> class delegate`: single-target, non-owning, small (two pointers): binds free
  functions, member functions (`delegate<void(int)>::bind<&foo::bar>(&instance)`), and stateless/capture-small lambdas
  (fits 16 bytes inline, else `static_assert`); `operator()`, `is_bound()`, `reset()`, comparable.
- `class event_bus`: typed pub/sub for engine-wide events: `template <typename Event> event_subscription subscribe(std::function<void(const Event &)>)`,
  `template <typename Event> void publish(const Event &)` (synchronous), `template <typename Event> void enqueue(Event)`
  + `void dispatch_queued()` (drained by the engine on the main thread); events are plain structs identified by
  `std::type_index`; thread-safe `enqueue`, everything else main-thread.

Tests: `test_clock.cpp` (manual clock, stopwatch with manual clock injection, steady clock monotonic),
`test_random.cpp` (seed reproducibility, ranges), `test_event.cpp` (RAII unsubscribe, reentrancy, order, dead event),
`test_delegate.cpp`, `test_event_bus.cpp`.

### 2.7 jobs (`conjure/core/jobs/`)

Headers: `job_system.h`, `job_handle.h`, `parallel_for.h`, `main_thread.h`, `dedicated_thread.h`, `jobs.h` (umbrella).

```cpp
namespace conjure::core::jobs {
    enum class job_priority : u8 { HIGH, NORMAL, LOW };
    class job_handle { /* shared counter reference; is_complete(); valid(); */ };
    struct job_system_desc { u32 workerCount = 0; /* 0 = inline */ usize maxQueuedJobs = 16384; const char *threadNamePrefix = "conjure-worker"; };

    class job_system {
        public:
            job_system(); ~job_system();
            [[nodiscard]] result<void> start(const job_system_desc &desc);  // creates workerCount threads (named); idempotent error INVALID_STATE if started
            void stop();                                                   // waits for queued jobs, joins threads; safe to call twice
            [[nodiscard]] bool is_running() const noexcept;
            [[nodiscard]] u32 worker_count() const noexcept;               // 0 when inline
            [[nodiscard]] job_handle schedule(std::function<void()> job, job_priority priority = job_priority::NORMAL);
            [[nodiscard]] job_handle schedule(std::span<const std::function<void()>> jobs, job_priority priority = job_priority::NORMAL); // one handle for a batch
            void wait(const job_handle &handle);                           // helps: executes queued jobs on the caller until complete
            void wait_idle();                                              // until no job is queued or running
            void parallel_for(usize count, usize grain, const std::function<void(usize begin, usize end)> &body);  // blocks; inline when workerCount == 0 or count <= grain
            void parallel_for(usize count, const std::function<void(usize index)> &body);                           // grain = max(1, count / (workers * 4))
            [[nodiscard]] static u32 thread_index() noexcept;              // 0 = main/calling non-worker thread, 1..workerCount for workers
            [[nodiscard]] static job_system *current() noexcept;           // the system whose worker is running, nullptr elsewhere
            [[nodiscard]] job_system_stats stats() const noexcept;         // scheduled, executed, stolen, maxQueueDepth
    };

    [[nodiscard]] bool is_main_thread() noexcept;        // the thread that first called set_main_thread() / created the first job_system; default: the thread that first calls it
    void set_main_thread() noexcept;                    // called by the engine at startup; tests call it in test_main
    void post_to_main_thread(std::function<void()> task); // thread-safe queue
    usize drain_main_thread_queue();                    // main thread only; runs every queued task once (tasks posted during drain run next drain); returns count
    class dedicated_thread { public: dedicated_thread(std::string name, std::function<void(std::stop_token)> body); ~dedicated_thread(); /* request_stop + join */ void request_stop() noexcept; bool joinable() const noexcept; };
}
```
Binding behaviour: work-stealing deques per worker (or a shared MPMC queue with per-priority lanes; stealing preferred
for `parallel_for` locality) with a condition-variable sleep when idle (no busy spin beyond a short bounded spin);
`parallel_for` splits `[0, count)` into ranges of `grain` and the caller participates; nested `parallel_for` from
inside a job is allowed; `wait` never deadlocks when called from a worker (it helps). The dispatch of `parallel_for`
must be allocation-free in steady state after warm-up (ranges carried in a stack-allocated array or a pooled job
block; the `std::function` body is referenced, not copied). Exceptions thrown by jobs are not supported (engine code
never throws); a job that throws terminates via `std::terminate` by design. `thread_index()` is a `thread_local`.

Tests (`test_job_system.cpp`, `test_parallel_for.cpp`, `test_main_thread.cpp`, `test_dedicated_thread.cpp`): run every
behavioural test twice via `SUBCASE`s with 0 and 8 workers (`[parallel]` tag); counting with atomics; `wait` from inside
a job; nested parallel_for; stop with pending jobs completes them; `post_to_main_thread` from workers; stats; a stress
test scheduling 100k tiny jobs; determinism of `parallel_for` results with per-range writes.

### 2.8 json (`conjure/core/json/`)

Headers: `value.h`, `document.h`, `writer.h`, `json.h` (umbrella with `parse`/`to_string`).

- `enum class json_type : u8 { NULL_VALUE, BOOL, NUMBER, STRING, ARRAY, OBJECT }`.
- `class value`: a **non-owning view** into a `document` (wraps `yyjson_val *` + document pointer): `type()`,
  `is_null/is_bool/is_number/is_string/is_array/is_object/is_integer`, `as_bool(bool fallback = false)`,
  `as_i64(i64 fallback = 0)`, `as_u64`, `as_f64`, `as_f32`, `as_string(std::string_view fallback = {})`,
  `size()` (array/object), `operator[](usize)`, `operator[](std::string_view)` (returns an invalid `value` when
  missing, never asserts), `contains(key)`, `get(key) -> std::optional<value>`, iteration `for (value item : array)`
  and `for (auto [key, item] : object.members())`, `is_valid()`. Typed getters with error: `template <typename T> result<T> to() const`
  for bool, integers (range-checked), floats, `std::string`, `uuid`, `string_id`, `math::vec2/3/4`, `quat`
  (arrays of numbers), `std::vector<T>`. `operator==` structural equality.
- `class document`: owning, mutable (yyjson mutable doc): `document()` (null root), `static document object()`,
  `static document array()`, `static result<document> parse(std::string_view text)` (errors: `PARSE_FAILURE` with
  line/column in the message), `static result<document> parse_file(const std::filesystem::path &)`, `root() -> value`,
  copy = deep copy, move cheap; mutation API: `set(std::string_view key, <scalar|document|value>)`, `set_null`,
  `push_back(...)`, `erase(key)`, `erase(index)`, `clear()`, pointer-style access `at_pointer("/a/b/0") -> value` and
  `set_pointer("/a/b/0", ...)` (creates intermediate objects), `merge_patch(const value &)` (RFC 7386),
  `to_string(bool pretty = false) const`, `to_canonical_string() const` (**sorted keys, minified, shortest round-trip
  floats, integers without exponent**; stable across platforms, used for hashing), `write_file(path, pretty) -> result<void>`.
  Convenience builders: `json::object({{"a", 1}, {"b", "x"}})`, `json::array({1, 2, 3})` returning `document`.
- `class writer`: streaming builder over a `std::string` buffer: `begin_object/end_object`, `begin_array/end_array`,
  `key(std::string_view)`, `value(bool|i64|u64|f64|std::string_view|nullptr_t)`, `write_raw_json(std::string_view)`
  (inserts pre-serialised JSON verbatim, validated in Debug), `finish() -> std::string`, `reset()`, asserts on
  structural misuse (key outside object, unbalanced end). Output of `writer` and `document::to_canonical_string` for the
  same data must be byte-identical when keys are written in sorted order.
- Free functions: `result<document> parse(std::string_view)`, `std::string to_string(const value &, bool canonical)`.

Tests: `test_value.cpp`, `test_document.cpp`, `test_writer.cpp`, `test_json.cpp` (round trips, canonical stability
across float values like 0.1, 1e21, -0.0, large u64; UTF-8 strings with escapes; deep nesting 1000; parse errors
report position; merge_patch per RFC examples; pointer access; equality; `to<T>` range checks).

### 2.9 vfs (`conjure/core/vfs/`)

Headers: `file_system.h`, `native_file_system.h`, `memory_file_system.h`, `mount_table.h`, `path_utils.h`, `vfs.h` (umbrella).

```cpp
namespace conjure::core::vfs {
    struct file_stat { u64 size = 0; u64 writeTimeNanoseconds = 0; bool isDirectory = false; bool isRegular = false; };
    enum class enumerate_flags : u8 { NONE = 0, RECURSIVE = 1 << 0, FILES = 1 << 1, DIRECTORIES = 1 << 2, ALL = FILES | DIRECTORIES };
    class file { public: virtual ~file(); [[nodiscard]] virtual result<usize> read_at(u64 offset, std::span<std::byte> out) = 0; [[nodiscard]] virtual u64 size() const noexcept = 0; };
    class file_system {   // one backend; paths are UTF-8, '/' separated, relative to the backend root
        public: virtual ~file_system();
        [[nodiscard]] virtual result<std::vector<std::byte>> read_file(std::string_view path) = 0;
        [[nodiscard]] virtual result<std::string> read_text_file(std::string_view path);                 // default via read_file
        [[nodiscard]] virtual result<void> write_file(std::string_view path, std::span<const std::byte> data) = 0;    // creates parents
        [[nodiscard]] virtual result<void> write_file_atomic(std::string_view path, std::span<const std::byte> data) = 0;  // tmp + rename
        [[nodiscard]] virtual result<file_stat> stat(std::string_view path) = 0;                        // NOT_FOUND when missing
        [[nodiscard]] virtual bool exists(std::string_view path);
        [[nodiscard]] virtual result<void> enumerate(std::string_view directory, enumerate_flags flags, const std::function<void(std::string_view path, const file_stat &)> &callback) = 0;
        [[nodiscard]] virtual result<void> rename(std::string_view from, std::string_view to) = 0;
        [[nodiscard]] virtual result<void> remove(std::string_view path) = 0;                          // files and empty dirs; remove_all(path) recursive
        [[nodiscard]] virtual result<void> create_directories(std::string_view path) = 0;
        [[nodiscard]] virtual result<std::unique_ptr<file>> open_read(std::string_view path) = 0;
        [[nodiscard]] virtual bool is_read_only() const noexcept; };
    class native_file_system final : public file_system { public: explicit native_file_system(std::filesystem::path root, bool readOnly = false); const std::filesystem::path &root() const noexcept; std::filesystem::path to_native(std::string_view path) const; /* rejects ".." escapes with PERMISSION_DENIED */ };
    class memory_file_system final : public file_system { /* in-memory tree; thread-safe via mutex; write times from a counter */ };
    class mount_table final : public file_system {   // "scheme://path" routing: project://, assets://, engine://, cooked://, modules://<name>/
        public: [[nodiscard]] result<void> mount(std::string_view scheme, std::shared_ptr<file_system> backend);   // ALREADY_EXISTS
                [[nodiscard]] result<void> unmount(std::string_view scheme);
                [[nodiscard]] file_system *find_mount(std::string_view scheme) const noexcept;
                [[nodiscard]] static result<std::pair<std::string_view, std::string_view>> split(std::string_view uri);  // {scheme, path}
                // every file_system method routes by scheme; a bare path without scheme is INVALID_ARGUMENT
    };
    // path_utils: normalize (collapse "//", "./", resolve "..", forward slashes), join, parent, filename, stem, extension (lower-case), has_extension, is_absolute, relative_to.
}
```
Tests: a **conformance suite** template run against `native_file_system` (in a `temp_directory`) and `memory_file_system`
(`test_file_system_conformance.cpp` with a `TEST_CASE_TEMPLATE` or shared function), plus `test_mount_table.cpp`,
`test_path_utils.cpp`, `test_native_file_system.cpp` (root escape rejected, atomic write leaves no temp file, write
time changes), `test_memory_file_system.cpp` (concurrent writers).

### 2.10 system (`conjure/core/`)

Headers: `process.h`, `dynamic_library.h`, `image.h`.

- `class process`: `struct process_desc { std::string executable; std::vector<std::string> arguments; std::filesystem::path workingDirectory; std::vector<std::pair<std::string, std::string>> environment; bool captureStdout = true; bool captureStderr = true; bool mergeStderr = false; f64 timeoutSeconds = 0; }`;
  `[[nodiscard]] static result<process_result> run(const process_desc &)` (blocking; `process_result { i32 exitCode; std::string standardOutput; std::string standardError; bool timedOut; f64 elapsedSeconds; }`),
  async: `[[nodiscard]] static result<process> start(const process_desc &)`, `bool is_running()`, `result<process_result> wait()`,
  `void kill()`, incremental `read_output(std::string &out)` (non-blocking). Implementation per platform in
  `process_windows.cpp` (CreateProcessW, pipes, Job objects for kill tree) and `process_posix.cpp` (posix_spawn/fork+exec,
  pipes, waitpid). Arguments are passed as an array (no shell); Windows quoting follows the MSVCRT rules.
- `class dynamic_library`: `[[nodiscard]] static result<dynamic_library> load(const std::filesystem::path &)` (`LoadLibraryW` /
  `dlopen(RTLD_NOW | RTLD_LOCAL)`; error message includes the OS error text), move-only RAII unload, `void *symbol(const char *name) const noexcept`,
  `template <typename Fn> Fn symbol_as(const char *name) const noexcept`, `is_loaded()`, `path()`, `static std::string decorate_name(std::string_view baseName)`
  (`conjure_foo` -> `conjure_foo.dll` / `libconjure_foo.so` / `libconjure_foo.dylib`), `static std::string_view extension()`, `static std::string_view prefix()`.
- `class image` (RGBA8 only in core; the renderer has its own formats): `image()`, `image(u32 width, u32 height)`
  (zero-filled), `width()`, `height()`, `pixels() -> std::span<std::byte>` (row-major, 4 bytes per pixel), `pixel(x, y) -> rgba8 {u8 r, g, b, a}`,
  `set_pixel`, `[[nodiscard]] static result<image> load_png(std::span<const std::byte>)` (stb_image, any channel count
  converted to RGBA8), `load_file(path)`, `[[nodiscard]] result<void> save_png(const std::filesystem::path &) const`,
  `result<std::vector<std::byte>> encode_png() const` (stb_image_write), `flip_vertical()`, `f64 compare(const image &other) const`
  (mean absolute difference 0..1, used by golden tests), `operator==`.

Tests: `test_process.cpp` (runs the test executable itself with a hidden `--conjure-child <mode>` flag or a tiny
`conjure_process_child` helper target added in `tests/core/system/CMakeLists.txt`: echo stdout, write stderr, exit codes,
timeout kill, environment variable visible, working directory), `test_dynamic_library.cpp` (missing file error,
`decorate_name` on all platforms, loading a tiny `conjure_test_shared_library` target built in the test dir and calling
an exported function), `test_image.cpp` (PNG round trip, channel conversion, compare, flip, invalid data error).

### 2.11 reflect (`conjure/core/reflect/`) - stage 2 (needs json, strings, math, containers)

Headers: `type_id.h`, `field_info.h`, `type_info.h`, `type_registry.h`, `reflect.h` (umbrella + macros), `json_conversion.h`.

```cpp
namespace conjure::core::reflect {
    struct type_id { u64 value = 0; /* fnv1a64 of the registered fully qualified name; stable across shared libraries */ constexpr bool is_null(); <=>; hash; formatter };
    template <typename T> [[nodiscard]] constexpr type_id type_id_of() noexcept;   // from reflect_traits<T>::NAME
    enum class field_kind : u8 { BOOL, I8, I16, I32, I64, U8, U16, U32, U64, F32, F64, STRING, STRING_ID, UUID, VEC2, VEC3, VEC4, QUAT, MAT4, TRANSFORM, ENUM, STRUCT, ARRAY, ENTITY, ASSET_REF, COUNT };
    struct field_attributes { std::optional<f64> min; std::optional<f64> max; f64 step = 0; std::string tooltip; bool hidden = false; bool readOnly = false; std::string category; };
    struct field_info { std::string name; type_id type; field_kind kind = field_kind::COUNT; u32 offset = 0; u32 size = 0; u32 arrayCount = 0; /* ARRAY: element kind in elementKind */ field_kind elementKind; std::string defaultValueJson; field_attributes attributes; const type_info *structType = nullptr; /* STRUCT/ARRAY-of-struct */ const enum_info *enumType = nullptr; };
    struct enum_info { std::string name; type_id type; u32 underlyingSize; std::vector<std::pair<std::string, i64>> values; bool isFlags; };
    struct type_info { std::string name; type_id type; u32 size = 0; u32 alignment = 0; std::vector<field_info> fields; bool isTriviallyCopyable = true; /* helpers */ const field_info *find_field(std::string_view) const noexcept; u64 layout_hash() const noexcept; /* fnv1a64 over (name, kind, offset, size) of every field, in order; mirrored by C# */ };

    class type_registry {   // one per process by default (registry()), but constructible for tests
        public: [[nodiscard]] result<const type_info *> register_type(type_info info);   // ALREADY_EXISTS if same id with different layout; same layout returns existing
                [[nodiscard]] result<const enum_info *> register_enum(enum_info info);
                [[nodiscard]] const type_info *find(type_id) const noexcept; find(std::string_view name) const noexcept;
                [[nodiscard]] const enum_info *find_enum(type_id) const noexcept;
                [[nodiscard]] std::span<const type_info *const> types() const noexcept;   // registration order
                [[nodiscard]] event<const type_info &> &on_registered() noexcept;
                [[nodiscard]] result<void> unregister_type(type_id);   // for module unload; INVALID_STATE if referenced by another type
                // mutex-guarded; find is lock-free after registration (append-only pointer stable storage)
    };
    [[nodiscard]] type_registry &registry() noexcept;

    template <typename T> [[nodiscard]] const type_info *type_info_of() noexcept;   // registers lazily in registry() from the CONJURE_REFLECT description; nullptr for unreflected types
    // Runtime construction (dynamic C# components): type_info build_type_info(std::string name, u32 size, u32 alignment, std::span<const field_info> fields); layout validated (offsets within size, no overlap, alignment).

    // JSON conversion driven by type_info (no templates needed at the call site):
    [[nodiscard]] result<void> to_json(const type_info &type, const void *object, json::writer &out);
    [[nodiscard]] result<void> from_json(const type_info &type, void *object, const json::value &in);   // missing fields keep their current value; unknown fields ignored (returned as warnings via optional std::vector<std::string> *unknownFields)
    [[nodiscard]] result<json::document> to_json_document(const type_info &type, const void *object);
    template <typename T> result<void> to_json(const T &, json::writer &); template <typename T> result<void> from_json(T &, const json::value &);
    // Field access by path: result<field_ref> resolve_path(const type_info &, void *object, std::string_view path /* "a.b[2].c" */); field_ref { void *data; const field_info *field; }; result<void> set_from_json(field_ref, const json::value &); result<json::document> get_as_json(field_ref);
}
// Description macros (in reflect.h), usable in any module:
//   CONJURE_REFLECT_STRUCT(ns::my_struct, "ns.my_struct",
//       CONJURE_FIELD(position),
//       CONJURE_FIELD(speed, .min = 0.0, .max = 100.0, .tooltip = "Units per second"),
//       CONJURE_FIELD_HIDDEN(internalCounter))
//   CONJURE_REFLECT_ENUM(ns::my_enum, "ns.my_enum", VALUE_A, VALUE_B, VALUE_C)
// These specialise reflect_traits<T> with NAME and a `static type_info describe()` built from the field list; the
// field kind is derived from the member type at compile time (kind_of<T>()), including nested reflected structs,
// std::array<T, N>, enums with CONJURE_REFLECT_ENUM, and the ENTITY / ASSET_REF kinds via customisation points
// `reflect_kind_override<T>` that the ecs and assets modules specialise (core knows nothing about them).
```
Tests: `test_type_id.cpp`, `test_type_info.cpp` (macro description, offsets vs `offsetof`, layout_hash stability
vector), `test_type_registry.cpp` (duplicate/same layout, different layout error, unregister, events, thread-safe
lookups), `test_json_conversion.cpp` (round trip of every field kind, missing/unknown fields, path resolution, arrays,
nested structs, enums by name and by value, flags), `test_reflect.cpp` (runtime-built type_info equals macro-built).

## 3. Test support (`conjure_test_support`) - stage 2, aligned with `08_testing.md` 2.2, 3.1, 4.1-4.3

Rework `tests/support/` into the headers `conjure/testing/doctest.h`, `approx.h`, `log_capture.h`,
`assert_capture.h` (`expect_assert`, `CONJURE_CHECK_ASSERTS`), `memory_guard.h`, `allocation_scope.h`,
`temp_directory.h`, `test_assets.h`, `test_environment.h`, plus `test_main.cpp` and `tracked_global_new.cpp`,
namespace `conjure::testing`. Keep the behaviours already implemented (quiet console, unexpected asserts fail the
case, re-entrancy-safe capture). `conjure_add_test_module` moves to `cmake/ConjureTesting.cmake` with the signature of
`08_testing.md` 3.8 (`doctest_discover_tests`, labels, timeout, `SKIP_RETURN_CODE 77`, `--conjure-isolated`), plus
`conjure_header_check(<target>)` compiling each public header in its own TU. Migrate every existing test file to
`#include "conjure/testing/doctest.h"` and the suite naming `TEST_SUITE("core/<header>")`. Add `tests/support/tests/`
self-tests (`conjure_support_tests`) and `tools/check_style.py` (line length, tabs, `#pragma once`, include order,
Windows-macro enumerator names, `using namespace` in headers, tests mirror headers) wired as a CTest test with label `style`.

## 4. Cross-area rules for implementers

1. You own exactly the directories of your area (`engine/include/conjure/core/<area-headers>`, `engine/src/core/<area>/`,
   `tests/core/<area>/`). Do not edit any other file. If foundation needs a change, write it in your report.
2. Build only your area: configure your own build directory (`cmake --preset windows-msvc-debug -B build/core_<area>`
   inside a developer shell, or `python tools/build.py --configure-only` once to verify the environment, then
   `cmake --build build/core_<area> --target conjure_core_<area>_tests` and run the executable).
3. Every public header compiles standalone, has `/// <summary>` docs on public methods, follows `CODE_STYLE.cpp` and
   `CONVENTIONS.md`, and is listed explicitly in your `CMakeLists.txt` (`conjure_add_core_area(<area> ...)`).
4. Tests use the current support header `conjure_test.h` (`conjure::test::log_capture`, `assert_capture`,
   `temp_directory`); stage 2 migrates them.
5. Write `engine/src/core/<area>/README.md` (purpose, types, threading, how to test).
6. Definition of done: `conjure_core_<area>_tests` passes in Debug with warnings as errors on the host, every
   behaviour in this document has a named test, no file outside your area touched.
