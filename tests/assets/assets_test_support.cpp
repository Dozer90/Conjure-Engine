#include "assets_test_support.h"

#include "conjure/core/string_utils.h"
#include "conjure/core/vfs/path_utils.h"

#include <algorithm>
#include <array>
#include <format>
#include <thread>
#include <utility>

using namespace conjure;
using namespace conjure::assets;
using namespace conjure::assets::testing;

namespace {
    constexpr std::array<std::string_view, 2> TEST_EXTENSIONS = {".ta", ".txt"};
    constexpr std::array<std::string_view, 1> FAILING_EXTENSIONS = {".bad"};

    struct parsed_source {
        std::string content = {};
        std::vector<asset_id> dependencies = {};
        std::vector<std::string> dependencyPaths = {};
        std::vector<std::string> subAssets = {};
        std::vector<std::string> sideFiles = {};
        std::vector<std::string> warnings = {};
        bool fail = false;
    };

    parsed_source parse_source(std::string_view text) {
        parsed_source parsed = {};
        for (const std::string_view rawLine : core::str::split_lines(text)) {
            const std::string_view line = core::str::trim(rawLine);
            if (core::str::starts_with(line, "dep-path ")) {
                parsed.dependencyPaths.emplace_back(core::str::trim(line.substr(9)));
            } else if (core::str::starts_with(line, "dep ")) {
                const std::optional<core::uuid> id = core::uuid::parse(core::str::trim(line.substr(4)));
                if (id.has_value()) {
                    parsed.dependencies.push_back(*id);
                }
            } else if (core::str::starts_with(line, "sub ")) {
                parsed.subAssets.emplace_back(core::str::trim(line.substr(4)));
            } else if (core::str::starts_with(line, "side ")) {
                parsed.sideFiles.emplace_back(core::str::trim(line.substr(5)));
            } else if (core::str::starts_with(line, "warn ")) {
                parsed.warnings.emplace_back(core::str::trim(line.substr(5)));
            } else if (line == "fail") {
                parsed.fail = true;
            } else {
                if (!parsed.content.empty()) {
                    parsed.content.push_back('\n');
                }
                parsed.content.append(line);
            }
        }
        return parsed;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Controls and helpers

loader_controls &conjure::assets::testing::controls() noexcept {
    static loader_controls sControls = {};
    return sControls;
}

std::vector<std::byte> conjure::assets::testing::bytes_of(std::string_view text) {
    std::vector<std::byte> bytes(text.size());
    for (usize index = 0; index < text.size(); ++index) {
        bytes[index] = static_cast<std::byte>(text[index]);
    }
    return bytes;
}

std::string conjure::assets::testing::text_of(std::span<const std::byte> bytes) {
    std::string text(bytes.size(), '\0');
    for (usize index = 0; index < bytes.size(); ++index) {
        text[index] = static_cast<char>(bytes[index]);
    }
    return text;
}

cooked_blob conjure::assets::testing::make_test_blob(
  const asset_id &id,
  std::string_view content,
  std::vector<asset_id> dependencies,
  asset_type_id type
) {
    cooked_blob_header header = {};
    header.type = type;
    header.typeVersion = 1;
    header.importerVersion = 1;
    header.sourceHash = 0x1234;
    header.settingsHash = 0x5678;
    header.id = id;
    std::sort(dependencies.begin(), dependencies.end());
    return make_cooked_blob(header, std::move(dependencies), bytes_of(content));
}

asset_id conjure::assets::testing::test_id(u64 index) noexcept {
    return asset_id(0x1000000000000000ull + index, 0x4000000000000000ull | (index * 0x9E3779B97F4A7C15ull));
}

u32 conjure::assets::testing::settle(asset_manager &manager, u32 maxUpdates) {
    u32 updates = 0;
    while (updates < maxUpdates) {
        manager.update();
        ++updates;
        if (manager.stats().pending == 0) {
            break;
        }
    }
    return updates;
}

/*===========================================================================*/
/*===========================================================================*/
// test_importer

test_importer::test_importer(u32 version) :
    mVersion(version) {}

test_importer::~test_importer() {}

std::string_view test_importer::id() const noexcept {
    return ID;
}

u32 test_importer::version() const noexcept {
    return mVersion;
}

std::span<const std::string_view> test_importer::extensions() const noexcept {
    return TEST_EXTENSIONS;
}

core::json::document test_importer::default_settings() const {
    return core::json::object({{"upper", false}});
}

core::result<import_output> test_importer::import(import_context &context) const {
    const parsed_source parsed = parse_source(text_of(context.source_bytes()));
    if (parsed.fail) {
        return core::error(core::error_code::INVALID_ARGUMENT, "source asked to fail");
    }
    for (const std::string &warning : parsed.warnings) {
        context.add_warning(warning);
    }

    std::string content = parsed.content;
    if (context.settings()["upper"].as_bool(false)) {
        core::str::to_upper_in_place(content);
    }
    for (const std::string &sideFile : parsed.sideFiles) {
        core::result<std::vector<std::byte>> side = context.read_side_file(sideFile);
        if (!side.has_value()) {
            return side.get_error();
        }
        content.append("|");
        content.append(text_of(side.value()));
    }

    std::vector<asset_id> dependencies = parsed.dependencies;
    for (const std::string &path : parsed.dependencyPaths) {
        const std::optional<asset_id> found = context.find_asset_by_path(path);
        if (!found.has_value()) {
            return core::error(core::error_code::NOT_FOUND, std::format("dependency path '{}' is unknown", path));
        }
        dependencies.push_back(*found);
    }
    std::sort(dependencies.begin(), dependencies.end());
    dependencies.erase(std::unique(dependencies.begin(), dependencies.end()), dependencies.end());

    import_output output = {};
    imported_asset primary = {};
    primary.type = asset_type_traits<test_asset>::id();
    primary.displayName = std::string(core::vfs::path_utils::stem(context.source_path()));
    primary.dependencies = dependencies;
    primary.payload = bytes_of(content);
    output.assets.push_back(std::move(primary));

    for (const std::string &name : parsed.subAssets) {
        imported_asset sub = {};
        sub.subPath = name;
        sub.displayName = name;
        sub.type = asset_type_traits<test_asset>::id();
        sub.payload = bytes_of(name + ":" + content);
        static_cast<void>(context.sub_asset_id(name));
        output.assets.push_back(std::move(sub));
    }
    return output;
}

/*===========================================================================*/
/*===========================================================================*/
// failing_importer

failing_importer::failing_importer() {}

failing_importer::~failing_importer() {}

std::string_view failing_importer::id() const noexcept {
    return ID;
}

u32 failing_importer::version() const noexcept {
    return 1;
}

std::span<const std::string_view> failing_importer::extensions() const noexcept {
    return FAILING_EXTENSIONS;
}

core::result<import_output> failing_importer::import(import_context &context) const {
    return core::error(core::error_code::UNKNOWN, std::format("failing importer refused '{}'", context.source_path()));
}

/*===========================================================================*/
/*===========================================================================*/
// Loaders

test_loader::test_loader() {}

test_loader::~test_loader() {}

core::result<test_asset> test_loader::load_cpu_typed(
  const load_context &context,
  std::span<const std::byte> payloadBytes
) const {
    controls().loadCpuCalls.fetch_add(1, std::memory_order_relaxed);
    while (controls().blockLoadCpu.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    if (controls().failLoadCpu.load(std::memory_order_acquire)) {
        return core::error(core::error_code::UNKNOWN, "load_cpu asked to fail");
    }
    test_asset asset = {};
    asset.content = text_of(payloadBytes);
    asset.dependencies.assign(context.dependencies.begin(), context.dependencies.end());
    return asset;
}

core::result<test_asset> test_loader::finalise_typed(const finalise_context &context, test_asset &&intermediate) const {
    controls().finaliseCalls.fetch_add(1, std::memory_order_relaxed);
    if (controls().failFinalise.load(std::memory_order_acquire)) {
        return make_asset_error(asset_error_code::FINALISE_FAILED, "finalise asked to fail");
    }
    test_asset finished = std::move(intermediate);
    finished.finaliseCount = 1;
    for (const asset_handle &dependency : context.dependencies) {
        if (!dependency.is_terminal()) {
            return make_asset_error(asset_error_code::FINALISE_FAILED, "a dependency was not terminal");
        }
    }
    return finished;
}

std::optional<test_asset> test_loader::create_fallback_typed() const {
    test_asset fallback = {};
    fallback.content = "fallback";
    return fallback;
}

other_loader::other_loader() {}

other_loader::~other_loader() {}

core::result<other_asset> other_loader::load_cpu_typed(
  const load_context &,
  std::span<const std::byte> payloadBytes
) const {
    other_asset asset = {};
    asset.content = text_of(payloadBytes);
    return asset;
}

core::result<other_asset> other_loader::finalise_typed(const finalise_context &, other_asset &&intermediate) const {
    return std::move(intermediate);
}

counted_loader::counted_loader() {}

counted_loader::~counted_loader() {}

core::result<counted_asset> counted_loader::load_cpu_typed(
  const load_context &,
  std::span<const std::byte> payloadBytes
) const {
    return counted_asset(text_of(payloadBytes));
}

core::result<counted_asset> counted_loader::finalise_typed(
  const finalise_context &,
  counted_asset &&intermediate
) const {
    return std::move(intermediate);
}

void conjure::assets::testing::register_test_types(asset_type_registry &types, asset_type_flags testAssetFlags) {
    CONJURE_REQUIRE_OK(
      types.register_type(make_asset_type_desc<test_asset>(std::make_unique<test_loader>(), 1, testAssetFlags))
    );
    CONJURE_REQUIRE_OK(types.register_type(make_asset_type_desc<other_asset>(std::make_unique<other_loader>(), 1)));
    CONJURE_REQUIRE_OK(types.register_type(make_asset_type_desc<counted_asset>(std::make_unique<counted_loader>(), 1)));
}

/*===========================================================================*/
/*===========================================================================*/
// pipeline_fixture

pipeline_fixture::pipeline_fixture(u32 workerCount, asset_type_flags testAssetFlags) {
    controls().failLoadCpu = false;
    controls().failFinalise = false;
    controls().blockLoadCpu = false;
    controls().loadCpuCalls = 0;
    controls().finaliseCalls = 0;

    core::jobs::job_system_desc jobsDesc = {};
    jobsDesc.workerCount = workerCount;
    CONJURE_REQUIRE_OK(jobs.start(jobsDesc));

    CONJURE_REQUIRE_OK(importers.register_importer(std::make_unique<test_importer>()));
    CONJURE_REQUIRE_OK(importers.register_importer(std::make_unique<failing_importer>()));

    // The manager owns the type registry; the database reads it.
    asset_manager_desc desc = {};
    desc.jobs = &jobs;
    manager = std::make_unique<asset_manager>(std::move(desc));
    register_test_types(manager->types(), testAssetFlags);

    database = std::make_unique<asset_database>(*fileSystem, importers, manager->types(), &jobs);
    CONJURE_REQUIRE_OK(database->add_mount("assets", "assets"));
    CONJURE_REQUIRE_OK(database->add_mount("engine", "engine"));
    CONJURE_REQUIRE_OK(fileSystem->create_directories("assets"));
    CONJURE_REQUIRE_OK(database->open(".conjure/cache", "test"));
    manager->swap_source(std::make_unique<cache_asset_source>(*database->cache(), *database));
}

pipeline_fixture::~pipeline_fixture() {
    if (manager != nullptr) {
        manager->shutdown();
    }
    manager.reset();
    database.reset();
    jobs.stop();
    controls().blockLoadCpu = false;
}

std::string pipeline_fixture::write_source(std::string_view relativePath, std::string_view text) {
    const std::string filePath = std::string("assets/") + std::string(relativePath);
    CONJURE_REQUIRE_OK(fileSystem->write_file(filePath, bytes_of(text)));
    return std::string("assets://") + std::string(relativePath);
}

void pipeline_fixture::scan_and_import() {
    CONJURE_REQUIRE_OK(database->scan());
    CONJURE_REQUIRE_OK(database->import_stale(jobs));
}

asset_id pipeline_fixture::id_of(std::string_view relativePath) const {
    const std::optional<asset_id> found = database->find_by_path(std::string("assets://") + std::string(relativePath));
    REQUIRE_MESSAGE(found.has_value(), "unknown source ", std::string(relativePath));
    return *found;
}
