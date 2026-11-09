#include "conjure/assets/asset_pack.h"
#include "assets/binary_io.h"
#include "assets/fnv1a.h"

#include "conjure/core/json/writer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::assets;

namespace {
    constexpr std::string_view TEMPORARY_SUFFIX = ".tmp";

    core::error corrupt(std::string message) {
        return make_asset_error(asset_error_code::PACK_CORRUPT, std::move(message));
    }

    /// <summary>
    /// True when [offset, offset + length) lies inside [0, size). Written in subtraction form so
    /// a corrupt TOC entry cannot wrap the sum past the end of the file: the reader is the
    /// runtime's trust boundary for packs and must not rely on the content hash alone.
    /// </summary>
    constexpr bool range_fits(u64 offset, u64 length, u64 size) noexcept {
        return offset <= size && length <= size - offset;
    }

    core::error io_error(std::string_view what, std::string_view path, const core::error &cause) {
        return make_asset_error(
          asset_error_code::IO_ERROR,
          std::format("cannot {} '{}': {}", what, path, cause.message())
        );
    }

    std::vector<std::byte> to_bytes(std::string_view text) {
        std::vector<std::byte> bytes(text.size());
        for (usize index = 0; index < text.size(); ++index) {
            bytes[index] = static_cast<std::byte>(text[index]);
        }
        return bytes;
    }

    core::result<void> write_atomically(
      core::vfs::file_system &fileSystem,
      std::string_view path,
      std::span<const std::byte> bytes
    ) {
        const std::string temporary = std::string(path) + std::string(TEMPORARY_SUFFIX);
        core::result<void> written = fileSystem.write_file(temporary, bytes);
        if (!written.has_value()) {
            return io_error("write", temporary, written.get_error());
        }
        core::result<void> renamed = fileSystem.rename(temporary, path);
        if (!renamed.has_value()) {
            static_cast<void>(fileSystem.remove(temporary));
            return io_error("rename", path, renamed.get_error());
        }
        return core::status::success();
    }

    void write_toc_entry(detail::byte_writer &out, const pack_toc_entry &entry) {
        out.uuid_value(entry.id);
        out.u64_value(static_cast<u64>(entry.type));
        out.u32_value(entry.typeVersion);
        out.u32_value(entry.flags);
        out.u64_value(entry.blobOffset);
        out.u64_value(entry.blobSize);
        out.u64_value(entry.payloadHash);
        out.u32_value(entry.dependencyStart);
        out.u32_value(entry.dependencyCount);
        out.u32_value(entry.nameOffset);
        out.u32_value(entry.nameLength);
    }

    bool read_toc_entry(detail::byte_reader &in, pack_toc_entry &entry) {
        u64 type = 0;
        bool ok = in.uuid_value(entry.id);
        ok = ok && in.u64_value(type);
        ok = ok && in.u32_value(entry.typeVersion);
        ok = ok && in.u32_value(entry.flags);
        ok = ok && in.u64_value(entry.blobOffset);
        ok = ok && in.u64_value(entry.blobSize);
        ok = ok && in.u64_value(entry.payloadHash);
        ok = ok && in.u32_value(entry.dependencyStart);
        ok = ok && in.u32_value(entry.dependencyCount);
        ok = ok && in.u32_value(entry.nameOffset);
        ok = ok && in.u32_value(entry.nameLength);
        entry.type = static_cast<asset_type_id>(type);
        return ok;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// pack_writer

pack_writer::pack_writer() {}

pack_writer::~pack_writer() {}

core::result<void> pack_writer::add(cooked_blob blob, std::string_view assetPath) {
    const u64 actualHash = hash_bytes(blob.payload);
    if (actualHash != blob.header.payloadHash || blob.header.payloadSize != blob.payload.size()) {
        return make_asset_error(
          asset_error_code::CACHE_CORRUPT,
          std::format("blob {} payload does not match its header", blob.header.id)
        );
    }
    for (const entry &existing : mEntries) {
        if (existing.blob.header.id == blob.header.id) {
            return core::error(
              core::error_code::ALREADY_EXISTS,
              std::format("blob {} was already added", blob.header.id)
            );
        }
    }
    mEntries.push_back(entry{std::move(blob), std::string(assetPath)});
    return core::status::success();
}

std::vector<std::byte> pack_writer::build(pack_stats &outStats) const {
    std::vector<const entry *> sorted = {};
    sorted.reserve(mEntries.size());
    for (const entry &item : mEntries) {
        sorted.push_back(&item);
    }
    std::sort(sorted.begin(), sorted.end(), [](const entry *a, const entry *b) {
        return a->blob.header.id < b->blob.header.id;
    });

    // Table layout: dependency ids and names in TOC order; payloads deduplicated by content.
    std::vector<asset_id> dependencies = {};
    std::string strings = {};
    std::vector<pack_toc_entry> toc = {};
    toc.reserve(sorted.size());
    for (const entry *item : sorted) {
        pack_toc_entry tocEntry = {};
        tocEntry.id = item->blob.header.id;
        tocEntry.type = item->blob.header.type;
        tocEntry.typeVersion = item->blob.header.typeVersion;
        tocEntry.flags = item->blob.header.flags;
        tocEntry.blobSize = item->blob.payload.size();
        tocEntry.payloadHash = item->blob.header.payloadHash;
        tocEntry.dependencyStart = static_cast<u32>(dependencies.size());
        tocEntry.dependencyCount = static_cast<u32>(item->blob.dependencies.size());
        dependencies.insert(dependencies.end(), item->blob.dependencies.begin(), item->blob.dependencies.end());
        tocEntry.nameOffset = static_cast<u32>(strings.size());
        tocEntry.nameLength = static_cast<u32>(item->assetPath.size());
        strings.append(item->assetPath);
        toc.push_back(tocEntry);
    }

    const u64 tocOffset = pack_header::SIZE;
    const u64 dependencyTableOffset = tocOffset + u64{pack_toc_entry::SIZE} * toc.size();
    const u64 stringTableOffset = dependencyTableOffset + u64{16} * dependencies.size();
    u64 blobsOffset = stringTableOffset + strings.size();
    blobsOffset = (blobsOffset + (pack_header::PAYLOAD_ALIGNMENT - 1)) & ~u64{pack_header::PAYLOAD_ALIGNMENT - 1};

    // Payload placement: identical payloads (same hash, size and bytes) share one copy.
    struct placed_payload {
        u64 hash = 0;
        u64 offset = 0;
        const std::vector<std::byte> *bytes = nullptr;
    };
    std::vector<placed_payload> placed = {};
    std::vector<std::byte> payloadArea = {};
    u32 duplicates = 0;
    for (usize index = 0; index < sorted.size(); ++index) {
        const std::vector<std::byte> &payload = sorted[index]->blob.payload;
        const u64 hash = toc[index].payloadHash;
        bool reused = false;
        for (const placed_payload &candidate : placed) {
            if (candidate.hash == hash && candidate.bytes->size() == payload.size() && *candidate.bytes == payload) {
                toc[index].blobOffset = candidate.offset;
                reused = true;
                break;
            }
        }
        if (reused) {
            ++duplicates;
            continue;
        }
        while (payloadArea.size() % pack_header::PAYLOAD_ALIGNMENT != 0) {
            payloadArea.push_back(std::byte{0});
        }
        const u64 offset = blobsOffset + payloadArea.size();
        payloadArea.insert(payloadArea.end(), payload.begin(), payload.end());
        toc[index].blobOffset = offset;
        placed.push_back(placed_payload{hash, offset, &payload});
    }

    // Everything after the header is hashed; the header is written last.
    std::vector<std::byte> body = {};
    body.reserve(static_cast<usize>(blobsOffset - pack_header::SIZE) + payloadArea.size());
    detail::byte_writer bodyOut(body);
    for (const pack_toc_entry &tocEntry : toc) {
        write_toc_entry(bodyOut, tocEntry);
    }
    for (const asset_id &dependency : dependencies) {
        bodyOut.uuid_value(dependency);
    }
    bodyOut.chars(std::span<const char>(strings.data(), strings.size()));
    while (body.size() + pack_header::SIZE < blobsOffset) {
        body.push_back(std::byte{0});
    }
    bodyOut.bytes(payloadArea);

    pack_header header = {};
    header.formatVersion = PACK_FORMAT_VERSION;
    header.flags = 0;
    header.entryCount = static_cast<u32>(toc.size());
    header.tocOffset = tocOffset;
    header.dependencyTableOffset = dependencyTableOffset;
    header.stringTableOffset = stringTableOffset;
    header.blobsOffset = blobsOffset;
    header.contentHash = hash_bytes(body);

    std::vector<std::byte> pack = {};
    pack.reserve(pack_header::SIZE + body.size());
    detail::byte_writer out(pack);
    out.chars(std::span<const char>(pack_header::MAGIC.data(), pack_header::MAGIC.size()));
    out.u32_value(header.formatVersion);
    out.u32_value(header.flags);
    out.u32_value(header.entryCount);
    out.u64_value(header.tocOffset);
    out.u64_value(header.dependencyTableOffset);
    out.u64_value(header.stringTableOffset);
    out.u64_value(header.blobsOffset);
    out.u64_value(header.contentHash);
    out.align(pack_header::SIZE);
    out.bytes(body);

    outStats.entryCount = header.entryCount;
    outStats.packBytes = pack.size();
    outStats.duplicatesRemoved = duplicates;
    outStats.contentHash = header.contentHash;
    return pack;
}

core::result<pack_stats> pack_writer::write(core::vfs::file_system &fileSystem, std::string_view packPath) const {
    pack_stats stats = {};
    const std::vector<std::byte> bytes = build(stats);
    core::result<void> written = write_atomically(fileSystem, packPath, bytes);
    if (!written.has_value()) {
        return std::move(written).get_error();
    }
    return stats;
}

std::string pack_writer::build_manifest() const {
    std::vector<const entry *> sorted = {};
    sorted.reserve(mEntries.size());
    for (const entry &item : mEntries) {
        sorted.push_back(&item);
    }
    std::sort(sorted.begin(), sorted.end(), [](const entry *a, const entry *b) {
        return a->blob.header.id < b->blob.header.id;
    });

    // Keys are written in sorted order so the text equals the canonical form of the same data.
    core::json::writer out = {};
    out.begin_array();
    for (const entry *item : sorted) {
        out.begin_object();
        out.key("dependencies");
        out.begin_array();
        for (const asset_id &dependency : item->blob.dependencies) {
            out.value(dependency.to_string());
        }
        out.end_array();
        out.key("hash");
        out.value(std::format("{:016x}", item->blob.header.payloadHash));
        out.key("name");
        out.value(item->assetPath);
        out.key("size");
        out.value(static_cast<u64>(item->blob.payload.size()));
        out.key("type");
        out.value(std::format("{:016x}", static_cast<u64>(item->blob.header.type)));
        out.key("uuid");
        out.value(item->blob.header.id.to_string());
        out.end_object();
    }
    out.end_array();
    std::string text = out.finish();
    text.push_back('\n');
    return text;
}

core::result<void> pack_writer::write_manifest(
  core::vfs::file_system &fileSystem,
  std::string_view manifestPath
) const {
    return write_atomically(fileSystem, manifestPath, to_bytes(build_manifest()));
}

/*===========================================================================*/
/*===========================================================================*/
// pack_reader

pack_reader::pack_reader() {}

pack_reader::~pack_reader() {}

core::result<void> pack_reader::open(core::vfs::file_system &fileSystem, std::string_view packPath) {
    core::result<std::unique_ptr<core::vfs::file>> opened = fileSystem.open_read(packPath);
    if (!opened.has_value()) {
        if (opened.get_error().generic_code() == core::error_code::NOT_FOUND) {
            return make_asset_error(asset_error_code::NOT_FOUND, std::format("no pack at '{}'", packPath));
        }
        return io_error("open pack", packPath, opened.get_error());
    }
    mFile = std::move(opened).value();
    mBuffer.clear();
    mFileSize = mFile->size();
    mOpen = false;

    core::result<void> parsed = parse_tables();
    if (!parsed.has_value()) {
        mFile.reset();
        return parsed;
    }
    mOpen = true;
    return core::status::success();
}

core::result<void> pack_reader::open_bytes(std::vector<std::byte> bytes) {
    mFile.reset();
    mBuffer = std::move(bytes);
    mFileSize = mBuffer.size();
    mOpen = false;

    core::result<void> parsed = parse_tables();
    if (!parsed.has_value()) {
        mBuffer.clear();
        return parsed;
    }
    mOpen = true;
    return core::status::success();
}

const pack_toc_entry *pack_reader::find(const asset_id &id) const noexcept {
    const auto found = std::lower_bound(mToc.begin(), mToc.end(), id, [](
      const pack_toc_entry &entry,
      const asset_id &key
    ) {
        return entry.id < key;
    });
    if (found == mToc.end() || found->id != id) {
        return nullptr;
    }
    return &*found;
}

core::result<cooked_blob> pack_reader::read(const pack_toc_entry &entry) const {
    if (!mOpen) {
        return make_asset_error(asset_error_code::PACK_CORRUPT, "pack is not open");
    }
    if (!range_fits(entry.blobOffset, entry.blobSize, mFileSize)) {
        return corrupt(std::format("entry {} points past the end of the pack", entry.id));
    }

    cooked_blob blob = {};
    blob.payload.resize(static_cast<usize>(entry.blobSize));
    core::result<void> read = read_range(entry.blobOffset, blob.payload);
    if (!read.has_value()) {
        return std::move(read).get_error();
    }
    const u64 actualHash = hash_bytes(blob.payload);
    if (actualHash != entry.payloadHash) {
        return corrupt(
          std::format("payload of {} hashes to {:#x}, expected {:#x}", entry.id, actualHash, entry.payloadHash)
        );
    }

    const std::span<const asset_id> dependencies = dependencies_of(entry);
    blob.dependencies.assign(dependencies.begin(), dependencies.end());
    blob.header.formatVersion = COOKED_BLOB_FORMAT_VERSION;
    blob.header.type = entry.type;
    blob.header.typeVersion = entry.typeVersion;
    blob.header.payloadHash = entry.payloadHash;
    blob.header.payloadSize = entry.blobSize;
    blob.header.dependencyCount = entry.dependencyCount;
    blob.header.flags = entry.flags;
    blob.header.id = entry.id;
    return blob;
}

std::span<const asset_id> pack_reader::dependencies_of(const pack_toc_entry &entry) const noexcept {
    if (!range_fits(entry.dependencyStart, entry.dependencyCount, mDependencies.size())) {
        return {};
    }
    return std::span<const asset_id>(mDependencies).subspan(entry.dependencyStart, entry.dependencyCount);
}

std::string_view pack_reader::name_of(const pack_toc_entry &entry) const noexcept {
    if (!range_fits(entry.nameOffset, entry.nameLength, mStrings.size())) {
        return {};
    }
    return std::string_view(mStrings).substr(entry.nameOffset, entry.nameLength);
}

const pack_toc_entry *pack_reader::find_by_name(std::string_view assetPath) const noexcept {
    for (const pack_toc_entry &entry : mToc) {
        if (entry.nameLength != 0 && name_of(entry) == assetPath) {
            return &entry;
        }
    }
    return nullptr;
}

/*===========================================================================*/
/*===========================================================================*/
// Private methods

core::result<void> pack_reader::read_range(u64 offset, std::span<std::byte> out) const {
    if (mFile == nullptr) {
        if (!range_fits(offset, out.size(), mBuffer.size())) {
            return corrupt("read past the end of the pack buffer");
        }
        std::memcpy(out.data(), mBuffer.data() + offset, out.size());
        return core::status::success();
    }
    core::result<usize> read = mFile->read_at(offset, out);
    if (!read.has_value()) {
        return io_error("read pack", "", read.get_error());
    }
    if (read.value() != out.size()) {
        return corrupt("pack is truncated");
    }
    return core::status::success();
}

core::result<void> pack_reader::parse_tables() {
    mToc.clear();
    mDependencies.clear();
    mStrings.clear();
    mHeader = pack_header();

    if (mFileSize < pack_header::SIZE) {
        return corrupt(std::format("pack is {} bytes, shorter than the {} byte header", mFileSize, pack_header::SIZE));
    }

    std::vector<std::byte> headerBytes(pack_header::SIZE);
    core::result<void> headerRead = read_range(0, headerBytes);
    if (!headerRead.has_value()) {
        return headerRead;
    }
    detail::byte_reader in(headerBytes);
    std::array<char, 4> magic = {};
    if (!in.chars(magic) || magic != pack_header::MAGIC) {
        return corrupt("pack magic is not CPAK");
    }
    bool ok = in.u32_value(mHeader.formatVersion);
    ok = ok && in.u32_value(mHeader.flags);
    ok = ok && in.u32_value(mHeader.entryCount);
    ok = ok && in.u64_value(mHeader.tocOffset);
    ok = ok && in.u64_value(mHeader.dependencyTableOffset);
    ok = ok && in.u64_value(mHeader.stringTableOffset);
    ok = ok && in.u64_value(mHeader.blobsOffset);
    ok = ok && in.u64_value(mHeader.contentHash);
    if (!ok) {
        return corrupt("pack header is truncated");
    }
    if (mHeader.formatVersion != PACK_FORMAT_VERSION) {
        return corrupt(
          std::format("pack format version {} is not the supported {}", mHeader.formatVersion, PACK_FORMAT_VERSION)
        );
    }

    const u64 tocEnd = mHeader.tocOffset + u64{pack_toc_entry::SIZE} * mHeader.entryCount;
    if (mHeader.tocOffset != pack_header::SIZE || tocEnd > mHeader.dependencyTableOffset ||
        mHeader.dependencyTableOffset > mHeader.stringTableOffset || mHeader.stringTableOffset > mHeader.blobsOffset ||
        mHeader.blobsOffset > mFileSize || (mHeader.dependencyTableOffset - tocEnd) != 0 ||
        (mHeader.stringTableOffset - mHeader.dependencyTableOffset) % 16 != 0) {
        return corrupt("pack table offsets are inconsistent");
    }

    // The content hash covers everything after the header, so the whole file is read once here;
    // payload reads afterwards are positional.
    std::vector<std::byte> body(static_cast<usize>(mFileSize - pack_header::SIZE));
    core::result<void> bodyRead = read_range(pack_header::SIZE, body);
    if (!bodyRead.has_value()) {
        return bodyRead;
    }
    const u64 actualHash = hash_bytes(body);
    if (actualHash != mHeader.contentHash) {
        return corrupt(std::format("pack content hashes to {:#x}, expected {:#x}", actualHash, mHeader.contentHash));
    }

    detail::byte_reader tables(body);
    mToc.reserve(mHeader.entryCount);
    for (u32 index = 0; index < mHeader.entryCount; ++index) {
        pack_toc_entry entry = {};
        if (!read_toc_entry(tables, entry)) {
            return corrupt("pack TOC is truncated");
        }
        if (index > 0 && !(mToc.back().id < entry.id)) {
            return corrupt("pack TOC is not sorted by uuid");
        }
        mToc.push_back(entry);
    }

    const usize dependencyCount = static_cast<usize>((mHeader.stringTableOffset - mHeader.dependencyTableOffset) / 16);
    mDependencies.reserve(dependencyCount);
    for (usize index = 0; index < dependencyCount; ++index) {
        asset_id dependency = {};
        if (!tables.uuid_value(dependency)) {
            return corrupt("pack dependency table is truncated");
        }
        mDependencies.push_back(dependency);
    }

    const usize stringBytes = static_cast<usize>(mHeader.blobsOffset - mHeader.stringTableOffset);
    std::span<const std::byte> stringSpan = {};
    if (!tables.bytes(stringBytes, stringSpan)) {
        return corrupt("pack string table is truncated");
    }
    mStrings.assign(reinterpret_cast<const char *>(stringSpan.data()), stringSpan.size());
    // The string table is followed by alignment padding up to blobsOffset; names never reach it.
    while (!mStrings.empty() && mStrings.back() == '\0') {
        mStrings.pop_back();
    }

    for (const pack_toc_entry &entry : mToc) {
        if (entry.blobOffset < mHeader.blobsOffset || !range_fits(entry.blobOffset, entry.blobSize, mFileSize) ||
            entry.blobOffset % pack_header::PAYLOAD_ALIGNMENT != 0) {
            return corrupt(std::format("entry {} has an invalid payload range", entry.id));
        }
        if (!range_fits(entry.dependencyStart, entry.dependencyCount, mDependencies.size())) {
            return corrupt(std::format("entry {} has an invalid dependency range", entry.id));
        }
        if (!range_fits(entry.nameOffset, entry.nameLength, stringBytes)) {
            return corrupt(std::format("entry {} has an invalid name range", entry.id));
        }
    }
    return core::status::success();
}
