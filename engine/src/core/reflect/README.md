# core/reflect

Runtime type information for plain data structs: stable type ids, field descriptions with editor
hints, a process-wide registry, and JSON (de)serialisation driven by those descriptions so the editor,
the scene format and the C# layer can read and write any reflected type without templates at the call
site. Static library `conjure::core_reflect`; depends on `foundation`, `json`, `strings`, `math`,
`containers` and `time` (for `event<>`). Design: `docs/design/10_core.md` section 2.11.

## Public headers (`conjure/core/reflect/`)

| Header | Contents |
|---|---|
| `type_id.h` | `type_id` (FNV-1a 64 of the registered name), `reflect_traits<T>` / `reflect_kind_override<T>` customisation points, `type_id_of<T>()`, `reflect_name_of<T>()` |
| `field_info.h` | `field_kind`, `kind_size` / `kind_alignment`, `field_attributes`, `field_info`, `enum_info` (name/value lookup, flag formatting), built-in `reflect_traits` for the fixed kinds, `kind_of<T>()`, `reflectable` concept |
| `type_info.h` | `type_info` (`find_field`, `layout_hash`, `has_same_layout`), `validate_layout`, `build_type_info` (dynamic C# components), `reflected_struct` / `reflected_enum`, `describe_struct<T>()` / `describe_enum<T>()` |
| `type_registry.h` | `type_registry`, `registry()`, `type_info_of<T>()`, `enum_info_of<T>()` |
| `json_conversion.h` | `json_converter` specialisations for `string_id` and the math types, `to_json` / `from_json` / `to_json_document`, `field_ref`, `resolve_path`, `write_json` / `set_from_json` / `get_as_json` |
| `reflect.h` | umbrella plus `CONJURE_REFLECT_STRUCT`, `CONJURE_FIELD`, `CONJURE_FIELD_HIDDEN`, `CONJURE_FIELD_READ_ONLY`, `CONJURE_REFLECT_ENUM`, `CONJURE_REFLECT_FLAGS` |

## Describing a type

```cpp
struct projectile { math::vec3 position = {}; f32 speed = 1.0f; u32 internalCounter = 0; };
CONJURE_REFLECT_STRUCT(projectile, "game.projectile",
    CONJURE_FIELD(position),
    CONJURE_FIELD(speed, .min = 0.0, .max = 100.0, .tooltip = "Units per second"),
    CONJURE_FIELD_HIDDEN(internalCounter));

enum class damage_type : u8 { BLUNT, FIRE };
CONJURE_REFLECT_ENUM(damage_type, "game.damage_type", BLUNT, FIRE);
```

- The macros specialise `reflect_traits<T>` at global namespace scope with `NAME`, `KIND` and
  `describe()`; like `CONJURE_ENUM_FLAGS` the call takes the caller's semicolon. `type_info_of<T>()`
  registers `describe()` in `registry()` on first use and returns the stable pointer; it returns
  `nullptr` for unreflected types and when the registry rejects the description (logged on
  `LOG_CORE`). Neither a failure nor a removal is cached: every call re-validates the cached pointer
  against `registry().find()` (lock-free), so the type registers again once a conflicting dynamic
  type is unregistered, and after `registry().unregister_type()` the next call yields the fresh
  registration instead of the retired description.
- Field kinds are derived at compile time by `kind_of<T>()`: the fixed kinds (`bool`, fixed-width
  integers, `f32`/`f64`, `std::string`, `string_id`, `uuid`, `vec2/3/4`, `quat`, `mat4`,
  `transform`), nested reflected structs (`STRUCT`, `field_info::structType` points at the registered
  description), reflected enums (`ENUM`, `enumType`), `std::array<T, N>` (`ARRAY` with `elementKind`
  and `arrayCount`; arrays of arrays are rejected at compile time) and foreign handles through
  `reflect_kind_override<T>` (`ENTITY` / `ASSET_REF`, specialised by the ecs and assets modules; core
  never sees those types). The platform integer types that differ from the fixed-width aliases
  (`long`, `unsigned long`, `long long`, `unsigned long long`, hence `usize`/`isize`) reflect as the
  fixed-width kind of their width and signedness, so a `usize` member describes identically on
  Windows, Linux and macOS. An unreflectable member fails to compile with a message saying what to add.
- Offsets come from `offsetof`, so reflected structs are plain data: no virtual functions (asserted at
  compile time) and ideally standard layout. Default values are captured by serialising a
  default-constructed instance into `field_info::defaultValueJson` (canonical JSON: minified, sorted
  keys, so a nested struct's default compares equal to `to_canonical_string()` of the same value;
  empty for types without a default constructor and for handles with no JSON representation).
- `<typeinfo>` declares a global `::type_info`, so after `using namespace conjure::core::reflect;` an
  unqualified `type_info` is ambiguous on MSVC; write `reflect::type_info` (the sources and tests do).
- `type_id` is the FNV-1a 64 hash of the registered name and nothing else, so it matches across shared
  libraries, sessions and the C# mirror; `type_id::from_name` is `constexpr`. Built-in kinds use the
  names `"bool"`, `"i8"` ... `"f64"`, `"string"`, `"string_id"`, `"uuid"`, `"vec2"` ... `"transform"`.
- `type_info::layout_hash()` is FNV-1a 64 from the offset basis over, for every field in order, the
  UTF-8 bytes of the field name, the kind as one byte, then offset and size as little-endian u32. It
  ignores the type name, attributes and defaults; `tests/core/reflect/test_type_info.cpp` pins the
  vector `0xfe13925eca48303a` for `test.inner` (`offset` VEC3 at 0, `weight` F32 at 12).
- `enum_info` keeps `isSigned` in addition to the design's fields so values are loaded and stored
  with the right width and sign; 64-bit unsigned enumerators are kept as their bit pattern in `i64`.
  A plain `CONJURE_REFLECT_ENUM` whose registered name ends in `_flags` is treated as a flag set;
  `CONJURE_REFLECT_FLAGS` makes it explicit for other names. At least one and at most 64
  enumerators per macro.

## Registry

- `register_type` validates the layout (`validate_layout`: names usable in paths, fields inside the
  struct, no overlap, natural alignment with no field stricter than the type's alignment, size
  matching the kind, struct/enum pointers present with a matching `field.type`, enum widths of 1, 2,
  4 or 8 bytes), requires every STRUCT/ENUM field pointer to be a live description of the same
  registry (a stack-local `type_info` would dangle), derives a null id from the name and rejects an
  id that does not match it. The same id registered again returns the existing description when
  `has_same_layout` (size, alignment, layout hash) holds and fails with `ALREADY_EXISTS` otherwise.
  `register_enum` applies the same policy with full equality of the description.
- `unregister_type` / `unregister_enum` (module unload) fail with `INVALID_STATE` while a live struct
  has a field (or array element) of that type, `NOT_FOUND` when unknown. Retired descriptions stay
  allocated until the registry is destroyed so a lock-free reader racing the removal never touches
  freed memory; re-registering the same name creates a fresh description.
- `on_registered()` fires once per new struct, `on_unregistered()` once per removal, on the calling
  thread after the registry lock is released; handlers may register further types. Struct events
  are emitted in the order the mutations took effect (the event lock is taken before the registry
  lock and held through the handlers), so a listener never sees a removal before the addition of
  the same description.
- `build_type_info(name, size, alignment, fields)` assembles a description for dynamic components and
  returns `result<type_info>` (the design sketches a plain `type_info`; the result carries the
  validation error since the engine does not throw). `isTriviallyCopyable` is derived from the fields.

### Threading

- `register_*` / `unregister_*` take a mutex and are safe from any thread.
- `find()` / `find_enum()` are lock-free: descriptions are heap objects that never move, and the
  id index (`src/core/reflect/pointer_index.h`) is a chain of open-addressing tables that only grows
  (a table never rehashes; when the newest one is half full the next insertion links a larger one)
  with release/acquire publication of each slot. Erasing leaves a tombstone so probe chains stay
  intact. Lookup by name hashes the name and verifies the stored name.
- `types()` / `enums()` return views of the live lists that the next registration or unregistration
  invalidates: use them when registrations are quiescent.
- Event handlers are serialised by a dedicated recursive mutex taken before the registry mutex (lock
  order: event mutex -> registry mutex, never the reverse; `register_enum`/`unregister_enum` and the
  counters take only the registry mutex); subscribe while no registration is in flight (startup,
  main thread).

## JSON

Representation (written in field order; `document::to_canonical_string()` sorts keys):

| Kind | JSON |
|---|---|
| `BOOL`, integers | `true`/`false`, numbers (integers range-checked on read, integral reals like `3.0` accepted) |
| `F32`, `F64` | numbers; non-finite values are written as `null` and `null` reads as NaN |
| `STRING` | string (any bytes, embedded zeros included) |
| `STRING_ID` | the interned name, or `"#" + 16 hex digits` when the name is unknown; reads also accept an unsigned number as the hash |
| `UUID` | `"xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"` |
| `VEC2/3/4`, `QUAT` | arrays of numbers, quaternions in storage order `[x, y, z, w]` |
| `MAT4` | 16 numbers, column-major |
| `TRANSFORM` | `{"position": [...], "rotation": [x, y, z, w], "scale": [...]}`; absent members keep their value |
| `ENUM` | the enumerator name (flags: `"A|B"`, zero as the zero enumerator's name or `""`), or the number when no name spells the value; reads accept either. 8 byte unsigned enumerators travel as their `u64` value; a number above `INT64_MAX` is `OUT_OF_RANGE` for a signed 8 byte enum |
| `STRUCT` | nested object |
| `ARRAY` | array; shorter input fills the leading elements, longer input is `OUT_OF_RANGE` |
| `ENTITY`, `ASSET_REF` | the raw handle: unsigned number for 1/2/4/8 byte handles, uuid string for 16 bytes; other sizes fail with `UNSUPPORTED`. Modules remap these after loading |

`from_json` keeps the current value of missing members, appends unknown members (dotted paths such
as `nested.extra`, `parts[0].stray`) to the optional list, and stops at the first failing field with
`INVALID_ARGUMENT` / `OUT_OF_RANGE` / `NOT_FOUND` and a message starting with `field '<path>'`;
fields read before the failure keep their new values. `resolve_path("a.b[2].c")` yields a `field_ref`
(data pointer, field, element flag, normalised path) for `set_from_json` / `get_as_json` /
`write_json`, whose errors name that path (`field 'counts[1]'`). Indices are plain decimal digits
(`[+1]` is malformed).

## Testing

`tests/core/reflect/` builds `conjure_core_reflect_tests` (one file per public header plus
`reflect_test_types.h`, a struct with a field of every kind and fake handle types):

```
python tools/build.py --build-dir build/core_reflect --target conjure_core_reflect_tests --test -R core_reflect
build/core_reflect/bin/conjure_core_reflect_tests
```

The suite covers compile-time ids and kinds, offsets against `offsetof`, the layout-hash formula
and pinned vector, every `validate_layout` failure, registry duplicate/conflict/unregister/event
behaviour, `type_info_of` recovering after a conflict or an unregistration, lock-free lookups under
concurrent registration and churn (`std::thread` in the tests only), round trips of every field kind
including 8 byte enums, lenient and failing reads with paths, enums by name and number, flag sets,
handle kinds, path resolution, and the equivalence of runtime-built and macro-built descriptions.
Every case passes on its own and in any order (`--order-by=rand`); the cases that touch the
process-wide `registry()` establish the state they need first.
