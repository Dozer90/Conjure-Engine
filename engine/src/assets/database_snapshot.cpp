#include "assets/database_snapshot.h"
#include "assets/binary_io.h"

#include <array>
#include <format>
#include <string>
#include <utility>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::detail;

namespace {
    constexpr std::array<char, 4> MAGIC = {'C', 'J', 'D', 'B'};
    constexpr u32 FORMAT_VERSION = 1;
    constexpr u32 MAX_COUNT = 1u << 24;

    core::error corrupt(std::string message) {
        return make_asset_error(asset_error_code::CACHE_CORRUPT, std::move(message));
    }
}

std::vector<std::byte> conjure::assets::detail::write_database_snapshot(const database_snapshot &snapshot) {
    std::vector<std::byte> bytes = {};
    byte_writer out(bytes);
    out.chars(std::span<const char>(MAGIC.data(), MAGIC.size()));
    out.u32_value(FORMAT_VERSION);

    out.u32_value(static_cast<u32>(snapshot.mountNames.size()));
    for (const std::string &name : snapshot.mountNames) {
        out.string_value(name);
    }

    out.u32_value(static_cast<u32>(snapshot.records.size()));
    for (const snapshot_record &item : snapshot.records) {
        const asset_record &record = item.record;
        out.uuid_value(record.id);
        out.uuid_value(record.parentId);
        out.u64_value(static_cast<u64>(record.type));
        out.u32_value(record.mountIndex);
        out.string_value(record.relativePath);
        out.string_value(record.subPath);
        out.string_value(record.importerId);
        out.string_value(record.displayName);
        out.u32_value(record.importerVersion);
        out.u64_value(record.sourceSize);
        out.i64_value(record.sourceWriteTime);
        out.u64_value(record.sourceHash);
        out.u64_value(record.settingsHash);
        out.u64_value(record.payloadHash);
        out.u8_value(static_cast<u8>(record.status));
        out.u32_value(static_cast<u32>(item.dependencies.size()));
        for (const asset_id &dependency : item.dependencies) {
            out.uuid_value(dependency);
        }
        out.u32_value(static_cast<u32>(item.sideFiles.size()));
        for (const std::string &sideFile : item.sideFiles) {
            out.string_value(sideFile);
        }
    }
    return bytes;
}

core::result<database_snapshot> conjure::assets::detail::parse_database_snapshot(std::span<const std::byte> bytes) {
    byte_reader in(bytes);
    std::array<char, 4> magic = {};
    if (!in.chars(magic) || magic != MAGIC) {
        return corrupt("snapshot magic is not CJDB");
    }
    u32 version = 0;
    if (!in.u32_value(version) || version != FORMAT_VERSION) {
        return corrupt(std::format("snapshot format version {} is not {}", version, FORMAT_VERSION));
    }

    database_snapshot snapshot = {};
    u32 mountCount = 0;
    if (!in.u32_value(mountCount) || mountCount > MAX_COUNT) {
        return corrupt("snapshot mount table is malformed");
    }
    for (u32 index = 0; index < mountCount; ++index) {
        std::string name = {};
        if (!in.string_value(name)) {
            return corrupt("snapshot mount name is truncated");
        }
        snapshot.mountNames.push_back(std::move(name));
    }

    u32 recordCount = 0;
    if (!in.u32_value(recordCount) || recordCount > MAX_COUNT) {
        return corrupt("snapshot record table is malformed");
    }
    snapshot.records.reserve(recordCount);
    for (u32 index = 0; index < recordCount; ++index) {
        snapshot_record item = {};
        asset_record &record = item.record;
        u64 type = 0;
        u8 status = 0;
        bool ok = in.uuid_value(record.id);
        ok = ok && in.uuid_value(record.parentId);
        ok = ok && in.u64_value(type);
        ok = ok && in.u32_value(record.mountIndex);
        ok = ok && in.string_value(record.relativePath);
        ok = ok && in.string_value(record.subPath);
        ok = ok && in.string_value(record.importerId);
        ok = ok && in.string_value(record.displayName);
        ok = ok && in.u32_value(record.importerVersion);
        ok = ok && in.u64_value(record.sourceSize);
        ok = ok && in.i64_value(record.sourceWriteTime);
        ok = ok && in.u64_value(record.sourceHash);
        ok = ok && in.u64_value(record.settingsHash);
        ok = ok && in.u64_value(record.payloadHash);
        ok = ok && in.u8_value(status);
        if (!ok) {
            return corrupt(std::format("snapshot record {} is truncated", index));
        }
        record.type = static_cast<asset_type_id>(type);
        if (status > static_cast<u8>(asset_record_status::CYCLIC_DEPENDENCY)) {
            return corrupt(std::format("snapshot record {} has an unknown status", index));
        }
        record.status = static_cast<asset_record_status>(status);
        if (record.mountIndex >= mountCount) {
            return corrupt(std::format("snapshot record {} names an unknown mount", index));
        }

        u32 dependencyCount = 0;
        if (!in.u32_value(dependencyCount) || dependencyCount > MAX_COUNT) {
            return corrupt(std::format("snapshot record {} dependency list is malformed", index));
        }
        for (u32 dependency = 0; dependency < dependencyCount; ++dependency) {
            asset_id id = {};
            if (!in.uuid_value(id)) {
                return corrupt(std::format("snapshot record {} dependency list is truncated", index));
            }
            item.dependencies.push_back(id);
        }

        u32 sideFileCount = 0;
        if (!in.u32_value(sideFileCount) || sideFileCount > MAX_COUNT) {
            return corrupt(std::format("snapshot record {} side file list is malformed", index));
        }
        for (u32 sideFile = 0; sideFile < sideFileCount; ++sideFile) {
            std::string path = {};
            if (!in.string_value(path)) {
                return corrupt(std::format("snapshot record {} side file list is truncated", index));
            }
            item.sideFiles.push_back(std::move(path));
        }
        snapshot.records.push_back(std::move(item));
    }
    if (in.remaining() != 0) {
        return corrupt("snapshot has trailing bytes");
    }
    return snapshot;
}
