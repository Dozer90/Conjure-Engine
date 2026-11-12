#pragma once

#include "conjure/scripting/native_api.h"
#include "conjure/scripting/script_class_registry.h"

#include "conjure/core/api.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/value.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"
#include "conjure/core/uuid.h"
#include "conjure/ecs/entity.h"

#include <functional>
#include <span>
#include <string_view>

// The field blob (docs/design/03_scripting.md 2.2, D7): the single wire format of apply_fields and
// collect_fields.
//
//   header : u32 magic 'CSFB' | u16 version | u16 field_count
//   field  : u64 name_hash | u8 kind | u8 element_kind | u16 reserved | u32 payload_size | payload (padded to 4)
//   payload: BOOL=u8, INT32/ENUM=i32, INT64=i64, FLOAT=f32, DOUBLE=f64, STRING=utf8 bytes, VEC2/3/4/COLOR=f32s,
//            QUAT=x,y,z,w f32s, ENTITY=u64 (entity::to_u64), ASSET=16-byte uuid (high, low u64s),
//            LIST=u32 count + element payloads; a STRING element is u32 byte length + bytes padded to 4
//
// Everything is little-endian and 4-byte aligned. The writer makes two passes without allocating (an
// empty output span measures), the reader bounds-checks every read. json_to_blob / blob_to_json convert
// between the blob and the scene JSON of one script class.

namespace conjure::scripting {

    inline constexpr u32 FIELD_BLOB_MAGIC = 0x42465343u;      // "CSFB" as little-endian bytes
    inline constexpr u16 FIELD_BLOB_VERSION = 1;
    inline constexpr usize FIELD_BLOB_HEADER_SIZE = 8;
    inline constexpr usize FIELD_BLOB_FIELD_HEADER_SIZE = 16;

    /// <summary>
    /// One field as the reader yields it. The payload view points into the blob.
    /// </summary>
    struct field_blob_field {
        u64 hash = 0;
        script_field_kind kind = script_field_kind::INT32;
        script_field_kind elementKind = script_field_kind::INT32;
        std::span<const u8> payload = {};
    };

    class CONJURE_API field_blob_writer {
        private:
            std::span<u8> mOutput = {};
            usize mCursor = 0;
            u32 mFieldCount = 0;
            bool mBegun = false;
            bool mOverflowed = false;
            usize mOpenFieldSizeOffset = 0;
            usize mOpenFieldPayloadStart = 0;
            bool mFieldOpen = false;

        public:
            /// <summary>
            /// Writes into 'output'; an empty span measures (required_size() reports what a real pass needs).
            /// Every write past the end sets overflowed() and keeps counting bytes.
            /// </summary>
            explicit field_blob_writer(std::span<u8> output) noexcept;
            ~field_blob_writer();

            field_blob_writer(const field_blob_writer &) = delete;
            field_blob_writer &operator=(const field_blob_writer &) = delete;

        public:
            /// <summary>
            /// Writes the header (the field count is patched by finish()).
            /// </summary>
            void begin();

            /// <summary>
            /// Writes one field header plus payload, padded to 4 bytes.
            /// </summary>
            void write_field(
              u64 hash,
              script_field_kind kind,
              script_field_kind elementKind,
              std::span<const u8> payload
            );

            void write_bool(u64 hash, bool value);
            void write_int32(u64 hash, i32 value, script_field_kind kind = script_field_kind::INT32);
            void write_int64(u64 hash, i64 value);
            void write_float(u64 hash, f32 value);
            void write_double(u64 hash, f64 value);
            void write_string(u64 hash, std::string_view value);
            void write_floats(u64 hash, script_field_kind kind, std::span<const f32> values);
            void write_entity(u64 hash, ecs::entity value);
            void write_asset(u64 hash, const core::uuid &value);

            /// <summary>
            /// Patches the field count and returns the bytes the blob needs. When that is more than the output
            /// size (overflowed()), the output is NOT a valid blob: the header counts every field while the
            /// bytes stop at the overflow, possibly inside a field header, so field_blob_reader rejects it.
            /// Retry with an output of the returned size (the measure-then-write idiom json_to_blob uses).
            /// </summary>
            [[nodiscard]] usize finish();

            [[nodiscard]] usize required_size() const noexcept {
                return mCursor;
            }

            [[nodiscard]] bool overflowed() const noexcept {
                return mOverflowed;
            }

            [[nodiscard]] u32 field_count() const noexcept {
                return mFieldCount;
            }

            /*---------------------------------------------------------------*/
            // Raw helpers for list payloads (used by json_to_blob)

            void write_bytes(std::span<const u8> bytes);
            void write_u32(u32 value);
            void pad_to_4();

            /// <summary>
            /// Starts a field whose payload is written piecewise through the raw helpers; end_field()
            /// patches the payload size. Nested begin_field is not allowed.
            /// </summary>
            void begin_field(u64 hash, script_field_kind kind, script_field_kind elementKind);
            void end_field();

        private:
            void put(const void *data, usize size);
            void patch_u32(usize offset, u32 value);
            void patch_u16(usize offset, u16 value);
    };

    class CONJURE_API field_blob_reader {
        private:
            std::span<const u8> mBlob = {};
            usize mCursor = 0;
            u32 mFieldCount = 0;
            u32 mFieldsRead = 0;
            bool mValid = false;

        public:
            /// <summary>
            /// Validates the header; a blob of 0 bytes is treated as an empty, valid blob.
            /// </summary>
            explicit field_blob_reader(std::span<const u8> blob) noexcept;
            ~field_blob_reader();

            field_blob_reader(const field_blob_reader &) = delete;
            field_blob_reader &operator=(const field_blob_reader &) = delete;

        public:
            /// <summary>
            /// False when the header is bad (magic, version, truncated).
            /// </summary>
            [[nodiscard]] bool is_valid() const noexcept {
                return mValid;
            }

            /// <summary>
            /// Fields the header announces.
            /// </summary>
            [[nodiscard]] u32 field_count() const noexcept {
                return mFieldCount;
            }

            /// <summary>
            /// Reads the next field. Returns false at the end, or FIELD_BLOB_INVALID when the field header or
            /// payload runs past the blob.
            /// </summary>
            [[nodiscard]] core::result<bool> next(field_blob_field &out);

            /// <summary>
            /// Reads the whole blob and fails on the first malformed field.
            /// </summary>
            [[nodiscard]] static core::result<void> validate(std::span<const u8> blob);
    };

    /*---------------------------------------------------------------*/
    // Payload codecs for one field

    /// <summary>
    /// Appends the payload of one value of 'field' (default when 'value' is invalid or mismatched, with a
    /// WARN naming the class and field unless 'warnOnMismatch' is false) as one blob field.
    /// </summary>
    CONJURE_API void write_field_value(
      field_blob_writer &writer,
      const script_class_info &classInfo,
      const script_field_info &field,
      const core::json::value &value,
      bool warnOnMismatch = true
    );

    /// <summary>
    /// Decodes one payload into JSON. FIELD_BLOB_INVALID when the payload is too short or malformed.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<core::json::document> read_field_value(
      const script_field_info &field,
      std::span<const u8> payload
    );

    /// <summary>
    /// True when a JSON value can be encoded as this kind (number for INT32, array of 3 numbers for VEC3,
    /// ...). An invalid value is never compatible.
    /// </summary>
    [[nodiscard]] CONJURE_API bool is_value_compatible(
      script_field_kind kind,
      script_field_kind elementKind,
      const core::json::value &value
    ) noexcept;

    /// <summary>
    /// Encodes the fields of a class from a JSON object into a blob. Missing or mismatched JSON members
    /// take the field default; JSON keys not in the schema are ignored (they stay in script_entry.fields).
    /// Returns the bytes needed; with an output that is too small the content is cut at the overflow and
    /// the caller retries with the returned size. Only a pass with a non-empty output warns about
    /// mismatched members, so the measure-then-write idiom reports each once.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<usize> json_to_blob(
      const script_class_info &classInfo,
      const core::json::value &fields,
      std::span<u8> output
    );

    /// <summary>
    /// Decodes a blob into a JSON object keyed by field name. Fields whose hash the class does not know are
    /// skipped (counted in a DEBUG log). FIELD_BLOB_INVALID for a malformed blob.
    /// </summary>
    [[nodiscard]] CONJURE_API core::result<core::json::document> blob_to_json(
      const script_class_info &classInfo,
      std::span<const u8> blob
    );

    /// <summary>
    /// Rewrites every ENTITY field and every element of a LIST of ENTITY in 'fields' through 'remap' and
    /// returns the result; members the schema does not know pass through unchanged.
    /// </summary>
    [[nodiscard]] CONJURE_API core::json::document remap_entity_fields_json(
      const script_class_info &classInfo,
      const core::json::value &fields,
      const std::function<ecs::entity(ecs::entity)> &remap
    );

    /// <summary>
    /// The JSON form of an entity reference in script fields: the packed id as a number (0 = null).
    /// </summary>
    [[nodiscard]] CONJURE_API ecs::entity entity_from_json(const core::json::value &value) noexcept;
}
