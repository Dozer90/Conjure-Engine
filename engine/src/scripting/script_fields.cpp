#include "conjure/scripting/script_fields.h"

#include "conjure/scripting/log_category.h"
#include "conjure/scripting/scripting_error.h"

#include "conjure/core/assert.h"
#include "conjure/core/log.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <string>

using namespace conjure;
using namespace conjure::scripting;

namespace {

    /*---------------------------------------------------------------*/
    // Fixed payload sizes per kind (0 = variable)

    [[nodiscard]] constexpr usize fixed_payload_size(script_field_kind kind) noexcept {
        switch (kind) {
            case script_field_kind::BOOL:
                return 1;
            case script_field_kind::INT32:
            case script_field_kind::ENUM:
            case script_field_kind::FLOAT:
                return 4;
            case script_field_kind::INT64:
            case script_field_kind::DOUBLE:
            case script_field_kind::ENTITY:
            case script_field_kind::VEC2:
                return 8;
            case script_field_kind::VEC3:
                return 12;
            case script_field_kind::VEC4:
            case script_field_kind::QUAT:
            case script_field_kind::COLOR:
            case script_field_kind::ASSET:
                return 16;
            case script_field_kind::STRING:
            case script_field_kind::LIST:
            case script_field_kind::COUNT:
                return 0;
        }
        return 0;
    }

    [[nodiscard]] constexpr u32 float_count(script_field_kind kind) noexcept {
        switch (kind) {
            case script_field_kind::VEC2:
                return 2;
            case script_field_kind::VEC3:
                return 3;
            case script_field_kind::VEC4:
            case script_field_kind::QUAT:
            case script_field_kind::COLOR:
                return 4;
            default:
                return 0;
        }
    }

    [[nodiscard]] constexpr usize align4(usize value) noexcept {
        return (value + 3) & ~usize{3};
    }

    [[nodiscard]] bool is_scalar_compatible(script_field_kind kind, const core::json::value &value) noexcept {
        switch (kind) {
            case script_field_kind::BOOL:
                return value.is_bool();
            case script_field_kind::INT32:
            case script_field_kind::ENUM:
            case script_field_kind::INT64:
            case script_field_kind::FLOAT:
            case script_field_kind::DOUBLE:
            case script_field_kind::ENTITY:
                return value.is_number();
            case script_field_kind::STRING:
                return value.is_string();
            case script_field_kind::ASSET:
                if (!value.is_string()) {
                    return false;
                }
                return value.as_string().empty() || core::uuid::parse(value.as_string()).has_value();
            case script_field_kind::VEC2:
            case script_field_kind::VEC3:
            case script_field_kind::VEC4:
            case script_field_kind::QUAT:
            case script_field_kind::COLOR: {
                if (!value.is_array() || value.size() != float_count(kind)) {
                    return false;
                }
                for (const core::json::value element : value) {
                    if (!element.is_number()) {
                        return false;
                    }
                }
                return true;
            }
            case script_field_kind::LIST:
            case script_field_kind::COUNT:
                return false;
        }
        return false;
    }

    /*---------------------------------------------------------------*/
    // Scalar encoding into a writer that is inside an open field (list elements) or as a whole field

    void put_scalar(field_blob_writer &writer, script_field_kind kind, const core::json::value &value) {
        switch (kind) {
            case script_field_kind::BOOL: {
                const u8 byteValue = value.as_bool() ? 1 : 0;
                writer.write_bytes(std::span<const u8>(&byteValue, 1));
                return;
            }
            case script_field_kind::INT32:
            case script_field_kind::ENUM: {
                const i64 wide = std::clamp<i64>(
                  value.as_i64(),
                  std::numeric_limits<i32>::min(),
                  std::numeric_limits<i32>::max()
                );
                const i32 narrow = static_cast<i32>(wide);
                writer.write_bytes(std::span<const u8>(reinterpret_cast<const u8 *>(&narrow), sizeof(narrow)));
                return;
            }
            case script_field_kind::INT64: {
                const i64 wide = value.as_i64();
                writer.write_bytes(std::span<const u8>(reinterpret_cast<const u8 *>(&wide), sizeof(wide)));
                return;
            }
            case script_field_kind::FLOAT: {
                const f32 single = value.as_f32();
                writer.write_bytes(std::span<const u8>(reinterpret_cast<const u8 *>(&single), sizeof(single)));
                return;
            }
            case script_field_kind::DOUBLE: {
                const f64 wide = value.as_f64();
                writer.write_bytes(std::span<const u8>(reinterpret_cast<const u8 *>(&wide), sizeof(wide)));
                return;
            }
            case script_field_kind::ENTITY: {
                const u64 packed = value.as_u64();
                writer.write_bytes(std::span<const u8>(reinterpret_cast<const u8 *>(&packed), sizeof(packed)));
                return;
            }
            case script_field_kind::ASSET: {
                const core::uuid id = core::uuid::parse(value.as_string()).value_or(core::uuid::null());
                const u64 high = id.high();
                const u64 low = id.low();
                writer.write_bytes(std::span<const u8>(reinterpret_cast<const u8 *>(&high), sizeof(high)));
                writer.write_bytes(std::span<const u8>(reinterpret_cast<const u8 *>(&low), sizeof(low)));
                return;
            }
            case script_field_kind::VEC2:
            case script_field_kind::VEC3:
            case script_field_kind::VEC4:
            case script_field_kind::QUAT:
            case script_field_kind::COLOR: {
                for (const core::json::value element : value) {
                    const f32 single = element.as_f32();
                    writer.write_bytes(std::span<const u8>(reinterpret_cast<const u8 *>(&single), sizeof(single)));
                }
                return;
            }
            case script_field_kind::STRING:
            case script_field_kind::LIST:
            case script_field_kind::COUNT:
                return;
        }
    }

    [[nodiscard]] core::error truncated_list(const script_field_info &field, u32 index) {
        return core::make_error(
          scripting_error::FIELD_BLOB_INVALID,
          "list '{}' element {} truncated",
          field.name,
          index
        );
    }

    void put_zero_scalar(field_blob_writer &writer, script_field_kind kind) {
        const usize size = fixed_payload_size(kind);
        std::array<u8, 16> zeros = {};
        writer.write_bytes(std::span<const u8>(zeros.data(), size));
    }

    [[nodiscard]] core::result<core::json::document> decode_scalar(
      script_field_kind kind,
      std::span<const u8> payload
    ) {
        const usize needed = fixed_payload_size(kind);
        if (payload.size() < needed) {
            return core::make_error(
              scripting_error::FIELD_BLOB_INVALID,
              "{} payload needs {} bytes, {} present",
              to_string(kind),
              needed,
              payload.size()
            );
        }
        switch (kind) {
            case script_field_kind::BOOL:
                return core::json::init_value(payload[0] != 0).to_document();
            case script_field_kind::INT32:
            case script_field_kind::ENUM: {
                i32 narrow = 0;
                std::memcpy(&narrow, payload.data(), sizeof(narrow));
                return core::json::init_value(narrow).to_document();
            }
            case script_field_kind::INT64: {
                i64 wide = 0;
                std::memcpy(&wide, payload.data(), sizeof(wide));
                return core::json::init_value(wide).to_document();
            }
            case script_field_kind::FLOAT: {
                f32 single = 0.0f;
                std::memcpy(&single, payload.data(), sizeof(single));
                return core::json::init_value(single).to_document();
            }
            case script_field_kind::DOUBLE: {
                f64 wide = 0.0;
                std::memcpy(&wide, payload.data(), sizeof(wide));
                return core::json::init_value(wide).to_document();
            }
            case script_field_kind::ENTITY: {
                u64 packed = 0;
                std::memcpy(&packed, payload.data(), sizeof(packed));
                return core::json::init_value(packed).to_document();
            }
            case script_field_kind::ASSET: {
                u64 high = 0;
                u64 low = 0;
                std::memcpy(&high, payload.data(), sizeof(high));
                std::memcpy(&low, payload.data() + sizeof(high), sizeof(low));
                const core::uuid id(high, low);
                return core::json::init_value(id.is_null() ? std::string() : id.to_string()).to_document();
            }
            case script_field_kind::VEC2:
            case script_field_kind::VEC3:
            case script_field_kind::VEC4:
            case script_field_kind::QUAT:
            case script_field_kind::COLOR: {
                core::json::document values = core::json::document::array();
                for (u32 index = 0; index < float_count(kind); ++index) {
                    f32 single = 0.0f;
                    std::memcpy(&single, payload.data() + index * sizeof(f32), sizeof(single));
                    values.push_back(single);
                }
                return values;
            }
            case script_field_kind::STRING:
                return core::json::init_value(
                  std::string_view(reinterpret_cast<const char *>(payload.data()), payload.size())
                ).to_document();
            case script_field_kind::LIST:
            case script_field_kind::COUNT:
                break;
        }
        return core::make_error(scripting_error::FIELD_BLOB_INVALID, "unsupported scalar kind {}", to_string(kind));
    }
}


/*===========================================================================*/
/*===========================================================================*/
// field_blob_writer

field_blob_writer::field_blob_writer(std::span<u8> output) noexcept :
    mOutput(output) {}

field_blob_writer::~field_blob_writer() {}

void field_blob_writer::begin() {
    CONJURE_ASSERT(!mBegun, "field_blob_writer::begin called twice");
    mBegun = true;
    const u32 magic = FIELD_BLOB_MAGIC;
    const u16 version = FIELD_BLOB_VERSION;
    const u16 count = 0;
    put(&magic, sizeof(magic));
    put(&version, sizeof(version));
    put(&count, sizeof(count));
}

void field_blob_writer::write_field(
  u64 hash,
  script_field_kind kind,
  script_field_kind elementKind,
  std::span<const u8> payload
) {
    begin_field(hash, kind, elementKind);
    write_bytes(payload);
    end_field();
}

void field_blob_writer::write_bool(u64 hash, bool value) {
    const u8 byteValue = value ? 1 : 0;
    write_field(hash, script_field_kind::BOOL, script_field_kind::BOOL, std::span<const u8>(&byteValue, 1));
}

void field_blob_writer::write_int32(u64 hash, i32 value, script_field_kind kind) {
    write_field(hash, kind, kind, std::span<const u8>(reinterpret_cast<const u8 *>(&value), sizeof(value)));
}

void field_blob_writer::write_int64(u64 hash, i64 value) {
    write_field(
      hash,
      script_field_kind::INT64,
      script_field_kind::INT64,
      std::span<const u8>(reinterpret_cast<const u8 *>(&value), sizeof(value))
    );
}

void field_blob_writer::write_float(u64 hash, f32 value) {
    write_field(
      hash,
      script_field_kind::FLOAT,
      script_field_kind::FLOAT,
      std::span<const u8>(reinterpret_cast<const u8 *>(&value), sizeof(value))
    );
}

void field_blob_writer::write_double(u64 hash, f64 value) {
    write_field(
      hash,
      script_field_kind::DOUBLE,
      script_field_kind::DOUBLE,
      std::span<const u8>(reinterpret_cast<const u8 *>(&value), sizeof(value))
    );
}

void field_blob_writer::write_string(u64 hash, std::string_view value) {
    write_field(
      hash,
      script_field_kind::STRING,
      script_field_kind::STRING,
      std::span<const u8>(reinterpret_cast<const u8 *>(value.data()), value.size())
    );
}

void field_blob_writer::write_floats(u64 hash, script_field_kind kind, std::span<const f32> values) {
    CONJURE_ASSERT(
      values.size() == float_count(kind),
      "write_floats: {} values for {}",
      values.size(),
      to_string(kind)
    );
    write_field(
      hash,
      kind,
      kind,
      std::span<const u8>(reinterpret_cast<const u8 *>(values.data()), values.size() * sizeof(f32))
    );
}

void field_blob_writer::write_entity(u64 hash, ecs::entity value) {
    const u64 packed = value.to_u64();
    write_field(
      hash,
      script_field_kind::ENTITY,
      script_field_kind::ENTITY,
      std::span<const u8>(reinterpret_cast<const u8 *>(&packed), sizeof(packed))
    );
}

void field_blob_writer::write_asset(u64 hash, const core::uuid &value) {
    std::array<u64, 2> halves = {value.high(), value.low()};
    write_field(
      hash,
      script_field_kind::ASSET,
      script_field_kind::ASSET,
      std::span<const u8>(reinterpret_cast<const u8 *>(halves.data()), sizeof(halves))
    );
}

usize field_blob_writer::finish() {
    CONJURE_ASSERT(mBegun, "field_blob_writer::finish without begin");
    CONJURE_ASSERT(!mFieldOpen, "field_blob_writer::finish with an open field");
    if (mFieldOpen) {
        end_field();
    }
    patch_u16(6, static_cast<u16>(std::min<u32>(mFieldCount, std::numeric_limits<u16>::max())));
    return mCursor;
}

void field_blob_writer::write_bytes(std::span<const u8> bytes) {
    put(bytes.data(), bytes.size());
}

void field_blob_writer::write_u32(u32 value) {
    put(&value, sizeof(value));
}

void field_blob_writer::pad_to_4() {
    const usize padded = align4(mCursor);
    static constexpr std::array<u8, 4> ZEROS = {};
    put(ZEROS.data(), padded - mCursor);
}

void field_blob_writer::begin_field(u64 hash, script_field_kind kind, script_field_kind elementKind) {
    CONJURE_ASSERT(mBegun, "field_blob_writer::begin_field without begin");
    CONJURE_ASSERT(!mFieldOpen, "field_blob_writer::begin_field while a field is open");
    const u8 kindByte = static_cast<u8>(kind);
    const u8 elementByte = static_cast<u8>(elementKind);
    const u16 reserved = 0;
    const u32 payloadSize = 0;
    put(&hash, sizeof(hash));
    put(&kindByte, sizeof(kindByte));
    put(&elementByte, sizeof(elementByte));
    put(&reserved, sizeof(reserved));
    mOpenFieldSizeOffset = mCursor;
    put(&payloadSize, sizeof(payloadSize));
    mOpenFieldPayloadStart = mCursor;
    mFieldOpen = true;
}

void field_blob_writer::end_field() {
    CONJURE_ASSERT(mFieldOpen, "field_blob_writer::end_field without begin_field");
    if (!mFieldOpen) {
        return;
    }
    const usize payloadSize = mCursor - mOpenFieldPayloadStart;
    patch_u32(mOpenFieldSizeOffset, static_cast<u32>(payloadSize));
    pad_to_4();
    mFieldOpen = false;
    ++mFieldCount;
}


/*---------------------------------------------------------------*/
// Private

void field_blob_writer::put(const void *data, usize size) {
    if (size == 0) {
        return;
    }
    if (mCursor + size <= mOutput.size()) {
        std::memcpy(mOutput.data() + mCursor, data, size);
    } else {
        mOverflowed = true;
    }
    mCursor += size;
}

void field_blob_writer::patch_u32(usize offset, u32 value) {
    if (offset + sizeof(value) <= mOutput.size()) {
        std::memcpy(mOutput.data() + offset, &value, sizeof(value));
    }
}

void field_blob_writer::patch_u16(usize offset, u16 value) {
    if (offset + sizeof(value) <= mOutput.size()) {
        std::memcpy(mOutput.data() + offset, &value, sizeof(value));
    }
}


/*===========================================================================*/
/*===========================================================================*/
// field_blob_reader

field_blob_reader::field_blob_reader(std::span<const u8> blob) noexcept :
    mBlob(blob) {
    if (blob.empty()) {
        mValid = true;
        return;
    }
    if (blob.size() < FIELD_BLOB_HEADER_SIZE) {
        return;
    }
    u32 magic = 0;
    u16 version = 0;
    u16 count = 0;
    std::memcpy(&magic, blob.data(), sizeof(magic));
    std::memcpy(&version, blob.data() + 4, sizeof(version));
    std::memcpy(&count, blob.data() + 6, sizeof(count));
    if (magic != FIELD_BLOB_MAGIC || version != FIELD_BLOB_VERSION) {
        return;
    }
    mFieldCount = count;
    mCursor = FIELD_BLOB_HEADER_SIZE;
    mValid = true;
}

field_blob_reader::~field_blob_reader() {}

core::result<bool> field_blob_reader::next(field_blob_field &out) {
    if (!mValid) {
        return core::make_error(scripting_error::FIELD_BLOB_INVALID, "bad field blob header");
    }
    if (mBlob.empty() || mFieldsRead >= mFieldCount) {
        return false;
    }
    if (mCursor + FIELD_BLOB_FIELD_HEADER_SIZE > mBlob.size()) {
        return core::make_error(
          scripting_error::FIELD_BLOB_INVALID,
          "field {} header runs past the blob ({} bytes)",
          mFieldsRead,
          mBlob.size()
        );
    }
    u8 kindByte = 0;
    u8 elementByte = 0;
    u32 payloadSize = 0;
    std::memcpy(&out.hash, mBlob.data() + mCursor, sizeof(out.hash));
    std::memcpy(&kindByte, mBlob.data() + mCursor + 8, sizeof(kindByte));
    std::memcpy(&elementByte, mBlob.data() + mCursor + 9, sizeof(elementByte));
    std::memcpy(&payloadSize, mBlob.data() + mCursor + 12, sizeof(payloadSize));
    constexpr u8 KIND_COUNT = static_cast<u8>(script_field_kind::COUNT);
    if (kindByte >= KIND_COUNT || elementByte >= KIND_COUNT) {
        return core::make_error(scripting_error::FIELD_BLOB_INVALID, "field {} has an unknown kind", mFieldsRead);
    }
    const usize payloadStart = mCursor + FIELD_BLOB_FIELD_HEADER_SIZE;
    if (payloadStart + payloadSize > mBlob.size()) {
        return core::make_error(
          scripting_error::FIELD_BLOB_INVALID,
          "field {} payload of {} bytes runs past the blob",
          mFieldsRead,
          payloadSize
        );
    }
    out.kind = static_cast<script_field_kind>(kindByte);
    out.elementKind = static_cast<script_field_kind>(elementByte);
    out.payload = mBlob.subspan(payloadStart, payloadSize);
    mCursor = std::min(align4(payloadStart + payloadSize), mBlob.size());
    ++mFieldsRead;
    return true;
}

core::result<void> field_blob_reader::validate(std::span<const u8> blob) {
    field_blob_reader reader(blob);
    field_blob_field field = {};
    while (true) {
        CONJURE_TRY_ASSIGN(const bool more, reader.next(field));
        if (!more) {
            return core::result<void>::success();
        }
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Codecs

bool conjure::scripting::is_value_compatible(
  script_field_kind kind,
  script_field_kind elementKind,
  const core::json::value &value
) noexcept {
    if (!value.is_valid()) {
        return false;
    }
    if (kind != script_field_kind::LIST) {
        return is_scalar_compatible(kind, value);
    }
    if (!value.is_array() || elementKind == script_field_kind::LIST || elementKind == script_field_kind::COUNT) {
        return false;
    }
    for (const core::json::value element : value) {
        if (!is_scalar_compatible(elementKind, element)) {
            return false;
        }
    }
    return true;
}

void conjure::scripting::write_field_value(
  field_blob_writer &writer,
  const script_class_info &classInfo,
  const script_field_info &field,
  const core::json::value &value,
  bool warnOnMismatch
) {
    const core::json::value *chosen = &value;
    const core::json::value fallback = field.defaultValue.root();
    if (!is_value_compatible(field.kind, field.elementKind, value)) {
        if (value.is_valid() && warnOnMismatch) {
            CONJURE_LOG_WARN(
              LOG_SCRIPTING,
              "field '{}' of '{}' holds a {} where {} was expected; using the default",
              field.name,
              classInfo.fullName,
              core::json::to_string(value.type()),
              to_string(field.kind)
            );
        }
        chosen = &fallback;
    }
    const bool useZero = !is_value_compatible(field.kind, field.elementKind, *chosen);

    writer.begin_field(field.hash, field.kind, field.elementKind);
    if (field.kind == script_field_kind::LIST) {
        if (!useZero) {
            writer.write_u32(static_cast<u32>(chosen->size()));
            for (const core::json::value element : *chosen) {
                if (field.elementKind == script_field_kind::STRING) {
                    const std::string_view text = element.as_string();
                    writer.write_u32(static_cast<u32>(text.size()));
                    writer.write_bytes(std::span<const u8>(reinterpret_cast<const u8 *>(text.data()), text.size()));
                    writer.pad_to_4();
                } else {
                    put_scalar(writer, field.elementKind, element);
                }
            }
        } else {
            writer.write_u32(0);
        }
    } else if (field.kind == script_field_kind::STRING) {
        if (!useZero) {
            const std::string_view text = chosen->as_string();
            writer.write_bytes(std::span<const u8>(reinterpret_cast<const u8 *>(text.data()), text.size()));
        }
    } else if (useZero) {
        put_zero_scalar(writer, field.kind);
    } else {
        put_scalar(writer, field.kind, *chosen);
    }
    writer.end_field();
}

core::result<core::json::document> conjure::scripting::read_field_value(
  const script_field_info &field,
  std::span<const u8> payload
) {
    if (field.kind != script_field_kind::LIST) {
        return decode_scalar(field.kind, payload);
    }
    if (payload.size() < sizeof(u32)) {
        return core::make_error(scripting_error::FIELD_BLOB_INVALID, "list '{}' has no count", field.name);
    }
    u32 count = 0;
    std::memcpy(&count, payload.data(), sizeof(count));
    core::json::document list = core::json::document::array();
    usize cursor = sizeof(u32);
    for (u32 index = 0; index < count; ++index) {
        if (field.elementKind == script_field_kind::STRING) {
            if (cursor + sizeof(u32) > payload.size()) {
                return truncated_list(field, index);
            }
            u32 length = 0;
            std::memcpy(&length, payload.data() + cursor, sizeof(length));
            cursor += sizeof(u32);
            if (cursor + length > payload.size()) {
                return truncated_list(field, index);
            }
            list.push_back(std::string_view(reinterpret_cast<const char *>(payload.data() + cursor), length));
            cursor = align4(cursor + length);
        } else {
            const usize elementSize = fixed_payload_size(field.elementKind);
            if (elementSize == 0 || cursor + elementSize > payload.size()) {
                return truncated_list(field, index);
            }
            CONJURE_TRY_ASSIGN(
              const core::json::document element,
              decode_scalar(field.elementKind, payload.subspan(cursor, elementSize))
            );
            list.push_back(element);
            cursor += elementSize;
        }
    }
    return list;
}

core::result<usize> conjure::scripting::json_to_blob(
  const script_class_info &classInfo,
  const core::json::value &fields,
  std::span<u8> output
) {
    field_blob_writer writer(output);
    writer.begin();
    for (const script_field_info &field : classInfo.fields) {
        const core::json::value value = fields.is_object() ? fields[field.name] : core::json::value();
        write_field_value(writer, classInfo, field, value, !output.empty());
    }
    return writer.finish();
}

core::result<core::json::document> conjure::scripting::blob_to_json(
  const script_class_info &classInfo,
  std::span<const u8> blob
) {
    core::json::document result = core::json::document::object();
    field_blob_reader reader(blob);
    field_blob_field field = {};
    u32 unknown = 0;
    while (true) {
        CONJURE_TRY_ASSIGN(const bool more, reader.next(field));
        if (!more) {
            break;
        }
        const script_field_info *known = classInfo.find_field(field.hash);
        if (known == nullptr) {
            ++unknown;
            continue;
        }
        if (field.kind != known->kind) {
            CONJURE_LOG_WARN(
              LOG_SCRIPTING,
              "field '{}' of '{}' arrived as {} but the schema says {}; skipped",
              known->name,
              classInfo.fullName,
              to_string(field.kind),
              to_string(known->kind)
            );
            continue;
        }
        CONJURE_TRY_ASSIGN(const core::json::document decoded, read_field_value(*known, field.payload));
        result.set(known->name, decoded);
    }
    if (unknown > 0) {
        CONJURE_LOG_DEBUG(LOG_SCRIPTING, "blob_to_json: {} unknown field(s) of '{}'", unknown, classInfo.fullName);
    }
    return result;
}

core::json::document conjure::scripting::remap_entity_fields_json(
  const script_class_info &classInfo,
  const core::json::value &fields,
  const std::function<ecs::entity(ecs::entity)> &remap
) {
    core::json::document result =
      fields.is_object() ? core::json::document::from_value(fields) : core::json::document::object();
    for (const script_field_info &field : classInfo.fields) {
        const core::json::value current = fields.is_object() ? fields[field.name] : core::json::value();
        if (!current.is_valid()) {
            continue;
        }
        if (field.kind == script_field_kind::ENTITY && current.is_number()) {
            result.set(field.name, remap(entity_from_json(current)).to_u64());
            continue;
        }
        const bool entityList = field.kind == script_field_kind::LIST && field.elementKind == script_field_kind::ENTITY;
        if (entityList && current.is_array()) {
            core::json::document list = core::json::document::array();
            for (const core::json::value element : current) {
                list.push_back(remap(entity_from_json(element)).to_u64());
            }
            result.set(field.name, list);
        }
    }
    return result;
}

ecs::entity conjure::scripting::entity_from_json(const core::json::value &value) noexcept {
    if (!value.is_number()) {
        return ecs::NULL_ENTITY;
    }
    return ecs::entity::from_u64(value.as_u64());
}
