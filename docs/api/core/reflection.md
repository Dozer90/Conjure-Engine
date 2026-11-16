# Reflection

📘 **Public API wiki** · 🛠 Handbook: [Core module](../../handbook/modules/core.md)

Describe a plain struct once and the editor inspector, the scene format and the C# layer can all read and
write it without you writing field-by-field code. Include `conjure/core/reflect/reflect.h`
(`conjure::core::reflect`); it provides the `CONJURE_REFLECT_*` macros and pulls in
`conjure/core/reflect/type_id.h` (stable ids), `conjure/core/reflect/field_info.h` (field kinds and
editor attributes), `conjure/core/reflect/type_info.h` (struct descriptions),
`conjure/core/reflect/type_registry.h` (the process-wide registry) and
`conjure/core/reflect/json_conversion.h` (JSON in and out, field paths).

## At a glance

| You want to... | Use |
|---|---|
| Make a component or settings struct visible to the editor | `CONJURE_REFLECT_STRUCT(type, "ns.name", CONJURE_FIELD(member), ...)` |
| Give a field a range, step, tooltip or category | `CONJURE_FIELD(member, .min = 0.0, .max = 1.0, .tooltip = "...")` |
| Hide a field or make it read-only in the inspector | `CONJURE_FIELD_HIDDEN(member)`, `CONJURE_FIELD_READ_ONLY(member)` |
| Make an enum editable as a drop-down (or as flags) | `CONJURE_REFLECT_ENUM(...)`, `CONJURE_REFLECT_FLAGS(...)` |
| Save or load a reflected struct as JSON | `reflect::to_json_document(object)`, `reflect::from_json(object, value)` |
| Get the description of a type at runtime | `reflect::type_info_of<T>()`, `reflect::registry().find("ns.name")` |
| Read or write one field by path (inspector edits) | `reflect::resolve_path()`, `get_as_json()`, `set_from_json()` |
| Describe a type whose layout is only known at runtime | `reflect::build_type_info()` + `registry().register_type()` |

## Quick example

```cpp
#include "conjure/core/reflect/reflect.h"

namespace game {
    enum class damage_type : conjure::u8 {
        BLUNT,
        FIRE,
        FROST
    };

    struct projectile {
        conjure::math::vec3 position = conjure::math::vec3(0.0f);
        conjure::f32 speed = 1.0f;
        damage_type damage = damage_type::BLUNT;
        conjure::u32 internalCounter = 0;
    };
}

CONJURE_REFLECT_ENUM(game::damage_type, "game.damage_type", BLUNT, FIRE, FROST);
CONJURE_REFLECT_STRUCT(game::projectile, "game.projectile",
    CONJURE_FIELD(position, .tooltip = "World position", .category = "Motion"),
    CONJURE_FIELD(speed, .min = 0.0, .max = 100.0, .step = 0.5, .tooltip = "Units per second"),
    CONJURE_FIELD(damage),
    CONJURE_FIELD_HIDDEN(internalCounter));
```

That is the whole job for a component. From here on the inspector draws `position`, `speed` and `damage`
with the right widgets, scenes save and load the struct, and `"game.projectile"` is the name every tool
and the C# mirror agree on.

## Describing a struct

`CONJURE_REFLECT_STRUCT(type, "registered.name", fields...)` lists the members to expose, in the order the
inspector should show them. Each `CONJURE_FIELD(member, attributes...)` records the member's name, offset,
size and kind; the attributes are designated initialisers of `reflect::field_attributes` and must appear
in declaration order: `min`, `max`, `step`, `tooltip`, `hidden`, `readOnly`, `category`.

> [!IMPORTANT]
> The macros go at **global namespace scope** (not inside your namespace), take the **fully qualified**
> C++ type name, and end with the caller's semicolon, like `CONJURE_ENUM_FLAGS`. The registered name is a
> dotted, stable identifier such as `"game.projectile"`.

> [!CAUTION]
> The registered name **is** the type's identity: `reflect::type_id` is a hash of that string and nothing
> else, and it is what saved scenes, the editor and C# store. Renaming a type or an enumerator breaks
> loading of existing data. Pick names you can live with.

A reflected struct must be plain data: no virtual functions (a compile-time error), member offsets come
from `offsetof`, and every listed member must be of a reflectable type:

| Member type | Kind | Notes |
|---|---|---|
| `bool`, `i8`..`i64`, `u8`..`u64`, `f32`, `f64` | scalar | platform integers (`long`, `usize`, ...) reflect as the fixed-width kind of their width |
| `std::string` | `STRING` | any bytes |
| `string_id`, `uuid` | `STRING_ID`, `UUID` | |
| `math::vec2/vec3/vec4`, `math::quat`, `math::mat4`, `math::transform` | math | quaternions store `[x, y, z, w]` |
| an enum described with `CONJURE_REFLECT_ENUM` / `_FLAGS` | `ENUM` | |
| a struct described with `CONJURE_REFLECT_STRUCT` | `STRUCT` | nested object in JSON |
| `std::array<T, N>` of any of the above | `ARRAY` | arrays of arrays are not reflectable: wrap the inner one in a struct |
| entity and asset handles | `ENTITY`, `ASSET_REF` | supplied by the ecs and assets modules |

A member of any other type fails to compile with a message naming the fix. Default values are captured by
serialising a default-constructed instance into `field_info::defaultValueJson`, so give members
initialisers.

> [!TIP]
> Keep the engine-facing state (`internalCounter`, cached handles, scratch data) out of the inspector with
> `CONJURE_FIELD_HIDDEN`, or leave it out of the macro altogether. A field that is left out is still part
> of the struct; it is simply invisible to tools and not saved.

## Describing an enum

`CONJURE_REFLECT_ENUM(type, "name", ENUMERATOR, ...)` lists between 1 and 64 enumerators per macro. The
inspector shows a drop-down and JSON stores the enumerator name (`"FIRE"`); a value without a name is
stored as its number. `CONJURE_REFLECT_FLAGS` marks a flag set (so are enums whose registered name ends in
`_flags`): the inspector shows check boxes and JSON stores `"STATIC|DYNAMIC"`.

```cpp
enum class collision_flags : conjure::u32 {
    NONE = 0,
    STATIC = 1u << 0,
    DYNAMIC = 1u << 1
};
CONJURE_REFLECT_FLAGS(collision_flags, "game.collision_flags", NONE, STATIC, DYNAMIC);
```

The enumeration must have an explicit underlying type of 1, 2, 4 or 8 bytes.

## JSON in and out

With a description in place, whole structs convert in one call. `to_json_document()` writes every field in
declaration order; `from_json()` updates an existing object, so members missing from the JSON keep their
current value (defaults, when you pass a fresh instance).

```cpp
using namespace conjure;
using namespace conjure::core;

result<std::string> save_projectile(const game::projectile &shot) {
    CONJURE_TRY_ASSIGN(const json::document doc, reflect::to_json_document(shot));
    return doc.to_string(true);
}

result<game::projectile> load_projectile(std::string_view text) {
    CONJURE_TRY_ASSIGN(const json::document doc, json::parse(text));
    game::projectile shot = {};
    std::vector<std::string> unknownMembers = {};   // unknown members, e.g. "extra" or "nested.extra"
    CONJURE_TRY(reflect::from_json(shot, doc.root(), &unknownMembers));
    return shot;
}
```

Reading is lenient where that is safe: an integral real (`3.0`) is accepted for an integer, a shorter
array fills only the leading elements, an enum accepts its name or its number, and unknown members are
ignored (and listed when you pass a vector). Anything else fails with `INVALID_ARGUMENT` or `OUT_OF_RANGE`
and a message that starts with the field path, for example
`field 'parts[1].weight': expected number, found bool`.

JSON has no number for an infinity or for NaN, so floats (`f32`, `f64` and every component of a vector,
quaternion, matrix or transform) use three other spellings, and each reads back as the value written:

| Value | Written as | Read from |
|---|---|---|
| a finite number | a JSON number | a JSON number |
| +infinity | the string `"inf"` | `"inf"` |
| -infinity | the string `"-inf"` | `"-inf"` |
| NaN | `null` | `null` |

A "no limit" sentinel such as `maxDistance = infinity` therefore survives a save and a load. Only these two
strings are understood, exactly as spelled; `"Infinity"`, `"INF"` or `"nan"` fail with `INVALID_ARGUMENT`.

> [!NOTE]
> Files saved before this encoding existed stored an infinity as `null`. They still load, and the value
> reads as NaN, as it did then. A file saved now with an infinity in it does not load in an older build of
> the engine, which expects a number or `null` there. A tool of your own that reads scene or prefab JSON
> must accept the two strings wherever it expects a float.

> [!WARNING]
> `from_json()` stops at the first failing field and **keeps the values it already wrote**. When a load
> fails, discard the object (or reload the previous state) instead of using it half-updated.

The typed wrappers fail with `INVALID_STATE` for a type without a description; the untyped overloads take a
`const reflect::type_info &` and a `void *` and are what the editor and the scene loader use when the C++
type is not known at compile time. `to_json(object, writer)` streams into a `json::writer` instead of
building a document. `value.to<T>()` also works for `string_id` and the math types once this header is
included (see 📘 API: [JSON](json.md)).

## Reading descriptions at runtime

`reflect::type_info_of<T>()` returns the registered description of a macro-described struct, registering
it in the process-wide `reflect::registry()` on first use. It returns `nullptr` for an unreflected type and
when a different type already owns the same registered name (logged as an error). The description is a
plain struct you can walk:

```cpp
struct inspector_row {
    std::string label = {};
    std::string valueJson = {};
};

result<std::vector<inspector_row>> inspector_rows(const reflect::type_info &type, void *component) {
    std::vector<inspector_row> rows = {};
    for (const reflect::field_info &field : type.fields) {
        if (field.attributes.hidden) {
            continue;
        }
        CONJURE_TRY_ASSIGN(const reflect::field_ref ref, reflect::resolve_path(type, component, field.name));
        CONJURE_TRY_ASSIGN(const json::document current, reflect::get_as_json(ref));
        rows.push_back({field.name, current.to_string()});
    }
    return rows;
}
```

`field_info` carries `name`, `kind` (and `elementKind` / `arrayCount` for arrays), `offset`, `size`,
`attributes`, `defaultValueJson`, and `structType` / `enumType` pointers for nested types. `enum_info`
offers `find_value(name)`, `find_name(value)`, `format_value()` and `parse_value()` for drop-downs.

> [!WARNING]
> `<typeinfo>` declares a global `::type_info`. After `using namespace conjure::core::reflect;` an
> unqualified `type_info` is ambiguous on MSVC; always write `reflect::type_info`.

## Editing one field by path

Inspector edits, undo entries and scripting all address a field by a dotted path such as
`"nested.offset"`, `"counts[2]"` or `"parts[1].weight"`. `resolve_path()` turns it into a
`reflect::field_ref` (data pointer plus description); `get_as_json()` and `set_from_json()` then read or
write exactly that field or array element, with errors naming the path.

```cpp
result<void> apply_inspector_edit(
  const reflect::type_info &type,
  void *component,
  std::string_view path,
  std::string_view editedJson
) {
    CONJURE_TRY_ASSIGN(const reflect::field_ref ref, reflect::resolve_path(type, component, path));
    CONJURE_TRY_ASSIGN(const json::document edited, json::document::parse(editedJson));
    return reflect::set_from_json(ref, edited.root());
}
```

`resolve_path()` fails with `NOT_FOUND` for an unknown field, `OUT_OF_RANGE` for an index past the array
and `INVALID_ARGUMENT` for a malformed path, an index on a non-array or a `.` into a non-struct. Indices
are plain decimal digits.

## The registry and dynamic types

`reflect::registry()` holds every registered struct and enum by stable pointer and offers lock-free lookup
by id or name: `find(type_id)`, `find("game.projectile")`, `find_enum(...)`, plus `types()` and `enums()`
to list everything (for example to populate an "Add component" menu). `on_registered()` and
`on_unregistered()` are events for tools that cache per-type data.

Types whose layout is only known at runtime, such as components defined in C#, are described with
`build_type_info()` and registered explicitly. This is the data-oriented escape hatch: the same
`to_json` / `from_json` / `resolve_path` calls work on a `void *` and the runtime description.

```cpp
result<const reflect::type_info *> register_script_component() {
    reflect::field_info health = {};
    health.name = "health";
    health.kind = reflect::field_kind::F32;
    health.type = reflect::type_id_of<f32>();
    health.offset = 0;
    health.size = 4;

    const std::span<const reflect::field_info> fields(&health, 1);
    CONJURE_TRY_ASSIGN(const reflect::type_info description, reflect::build_type_info("script.health", 4, 4, fields));
    return reflect::registry().register_type(description);   // the existing pointer when already registered
}
```

`register_type()` validates the layout (`INVALID_ARGUMENT` naming the offending field), returns the existing
description when the same name is registered again with an identical layout and `ALREADY_EXISTS` when the
layout differs. `unregister_type()` (module unload) is `INVALID_STATE` while another live struct still has a
field of that type.

> [!WARNING]
> `types()` and `enums()` return views that the next registration or unregistration invalidates; copy them
> if registrations may happen meanwhile. Subscribe to `on_registered()` / `on_unregistered()` at startup on
> the main thread, while no registration is in flight. `register_*` / `unregister_*` are safe from any
> thread; `find*()` never blocks.

## API summary

| Signature (short) | Purpose |
|---|---|
| `CONJURE_REFLECT_STRUCT(type, "name", CONJURE_FIELD(member, .attr = ...), ...)` | describe a struct |
| `CONJURE_FIELD_HIDDEN(member)` / `CONJURE_FIELD_READ_ONLY(member)` | field shortcuts |
| `CONJURE_REFLECT_ENUM(type, "name", A, B, ...)` / `CONJURE_REFLECT_FLAGS(...)` | describe an enum or flag set |
| `const type_info *type_info_of<T>()` / `const enum_info *enum_info_of<T>()` | registered description, `nullptr` when none |
| `type_info describe_struct<T>()` / `enum_info describe_enum<T>()` | a fresh, unregistered copy |
| `constexpr type_id type_id_of<T>()` / `static type_id type_id::from_name(string_view)` | stable ids |
| `constexpr field_kind kind_of<T>()` / concept `reflectable<T>` | compile-time classification |
| `const field_info *type_info::find_field(string_view) const` / `u64 layout_hash() const` | walk a description |
| `result<void> validate_layout(const type_info &)` | check a hand-built description |
| `result<type_info> build_type_info(std::string name, u32 size, u32 alignment, span<const field_info>)` | runtime description |
| `result<json::document> to_json_document(const T &)` / `(const type_info &, const void *)` | struct to JSON |
| `result<void> to_json(const T &, json::writer &)` / `(const type_info &, const void *, json::writer &)` | struct to a writer |
| `result<void> from_json(T &, const json::value &, std::vector<std::string> *unknown = nullptr)` | JSON to struct |
| `result<field_ref> resolve_path(const type_info &, void *object, string_view path)` | locate a field or element |
| `result<json::document> get_as_json(field_ref)` / `result<void> set_from_json(field_ref, const json::value &)` | one field |
| `result<void> write_json(field_ref, json::writer &)` | one field, streaming |
| `type_registry &registry()` | the process-wide registry |
| `result<const type_info *> type_registry::register_type(type_info)` / `register_enum(enum_info)` | register |
| `const type_info *type_registry::find(type_id) const` / `find(string_view) const` / `find_enum(...)` | lock-free lookup |
| `span<const type_info *const> type_registry::types() const` / `enums()` / `type_count()` | list |
| `event<const type_info &> &type_registry::on_registered()` / `on_unregistered()` | change events |
| `result<void> type_registry::unregister_type(type_id)` / `unregister_enum(type_id)` | module unload |

## See also

- 📘 API: [JSON](json.md) for `json::document`, `json::value` and `json::writer`
- 📘 API: [Math](math.md) for the vector, quaternion and transform types that reflect as fields
- 📘 API: [Strings and identifiers](strings-and-identifiers.md) for `string_id`
- 📘 API: [Time, events and random](time-and-events.md) for the `event<>` type behind `on_registered()`
- 🛠 Handbook: [Core module](../../handbook/modules/core.md) for layout validation, the lock-free registry and the C# layout hash
