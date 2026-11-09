#include "conjure/assets/cooked_blob.h"

#include "assets/binary_io.h"
#include "assets/fnv1a.h"

#include <array>
#include <cstddef>
#include <format>
#include <span>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::assets;

namespace {
    core::error corrupt(std::string message) {
        return make_asset_error(asset_error_code::CACHE_CORRUPT, std::move(message));
    }

    void write_header(detail::byte_writer &out, const cooked_blob_header &header) {
        out.chars(std::span<const char>(cooked_blob_header::MAGIC.data(), cooked_blob_header::MAGIC.size()));
        out.u32_value(header.formatVersion);
        out.u64_value(static_cast<u64>(header.type));
        out.u32_value(header.typeVersion);
        out.u32_value(header.importerVersion);
        out.u64_value(header.sourceHash);
        out.u64_value(header.settingsHash);
        out.u64_value(header.payloadHash);
        out.u64_value(header.payloadSize);
        out.u32_value(header.dependencyCount);
        out.u32_value(header.flags);
        out.uuid_value(header.id);
    }
}

u64 conjure::assets::hash_bytes(std::span<const std::byte> bytes) noexcept {
    return detail::fnv1a64_bytes(bytes);
}

cooked_blob conjure::assets::make_cooked_blob(
  cooked_blob_header header,
  std::vector<asset_id> dependencies,
  std::vector<std::byte> payload
) {
    cooked_blob blob = {};
    header.formatVersion = COOKED_BLOB_FORMAT_VERSION;
    header.dependencyCount = static_cast<u32>(dependencies.size());
    header.payloadSize = payload.size();
    header.payloadHash = hash_bytes(payload);
    blob.header = header;
    blob.dependencies = std::move(dependencies);
    blob.payload = std::move(payload);
    return blob;
}

std::vector<std::byte> conjure::assets::write_cooked_blob(const cooked_blob &blob) {
    cooked_blob_header header = blob.header;
    header.formatVersion = COOKED_BLOB_FORMAT_VERSION;
    header.dependencyCount = static_cast<u32>(blob.dependencies.size());
    header.payloadSize = blob.payload.size();
    header.payloadHash = hash_bytes(blob.payload);

    std::vector<std::byte> bytes = {};
    bytes.reserve(static_cast<usize>(header.total_size()));
    detail::byte_writer out(bytes);
    write_header(out, header);
    for (const asset_id &dependency : blob.dependencies) {
        out.uuid_value(dependency);
    }
    out.align(cooked_blob_header::PAYLOAD_ALIGNMENT);
    out.bytes(blob.payload);
    return bytes;
}

core::result<cooked_blob_header> conjure::assets::parse_cooked_blob_header(std::span<const std::byte> bytes) {
    if (bytes.size() < cooked_blob_header::SIZE) {
        return corrupt(
          std::format("blob is {} bytes, shorter than the {} byte header", bytes.size(), cooked_blob_header::SIZE)
        );
    }

    detail::byte_reader in(bytes);
    std::array<char, 4> magic = {};
    if (!in.chars(magic) || magic != cooked_blob_header::MAGIC) {
        return corrupt("blob magic is not CJAB");
    }

    cooked_blob_header header = {};
    u64 type = 0;
    bool ok = in.u32_value(header.formatVersion);
    ok = ok && in.u64_value(type);
    ok = ok && in.u32_value(header.typeVersion);
    ok = ok && in.u32_value(header.importerVersion);
    ok = ok && in.u64_value(header.sourceHash);
    ok = ok && in.u64_value(header.settingsHash);
    ok = ok && in.u64_value(header.payloadHash);
    ok = ok && in.u64_value(header.payloadSize);
    ok = ok && in.u32_value(header.dependencyCount);
    ok = ok && in.u32_value(header.flags);
    ok = ok && in.uuid_value(header.id);
    if (!ok) {
        return corrupt("blob header is truncated");
    }
    header.type = static_cast<asset_type_id>(type);

    if (header.formatVersion != COOKED_BLOB_FORMAT_VERSION) {
        return corrupt(std::format(
          "blob format version {} is not the supported version {}",
          header.formatVersion,
          COOKED_BLOB_FORMAT_VERSION
        ));
    }
    return header;
}

core::result<cooked_blob> conjure::assets::parse_cooked_blob(std::span<const std::byte> bytes) {
    core::result<cooked_blob_header> parsedHeader = parse_cooked_blob_header(bytes);
    if (!parsedHeader.has_value()) {
        return std::move(parsedHeader).get_error();
    }

    cooked_blob blob = {};
    blob.header = parsedHeader.value();

    // Every range check is written in subtraction form: a corrupt header must never wrap a sum
    // past the buffer size and reach the allocation below (the design promises CACHE_CORRUPT).
    const usize available = bytes.size() - cooked_blob_header::SIZE;
    if (blob.header.dependencyCount > available / 16) {
        return corrupt(std::format(
          "blob declares {} dependencies but only {} bytes follow the header",
          blob.header.dependencyCount,
          available
        ));
    }
    const u64 payloadOffset = blob.header.payload_offset();
    if (payloadOffset > bytes.size() || blob.header.payloadSize != bytes.size() - payloadOffset) {
        return corrupt(std::format(
          "blob declares a {} byte payload at offset {} but the file holds {} bytes",
          blob.header.payloadSize,
          payloadOffset,
          bytes.size()
        ));
    }

    detail::byte_reader in(bytes, cooked_blob_header::SIZE);
    blob.dependencies.reserve(blob.header.dependencyCount);
    for (u32 index = 0; index < blob.header.dependencyCount; ++index) {
        asset_id dependency = {};
        if (!in.uuid_value(dependency)) {
            return corrupt("blob dependency list is truncated");
        }
        blob.dependencies.push_back(dependency);
    }

    const std::span<const std::byte> payload = bytes.subspan(
      static_cast<usize>(payloadOffset),
      static_cast<usize>(blob.header.payloadSize)
    );
    const u64 actualHash = hash_bytes(payload);
    if (actualHash != blob.header.payloadHash) {
        return corrupt(std::format(
          "blob payload hash {:#x} does not match the recorded {:#x}",
          actualHash,
          blob.header.payloadHash
        ));
    }
    blob.payload.assign(payload.begin(), payload.end());
    return blob;
}
