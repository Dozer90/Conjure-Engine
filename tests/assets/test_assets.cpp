#include "conjure/testing/doctest.h"

#include "conjure/assets/assets.h"

#include <type_traits>

using namespace conjure;
using namespace conjure::assets;

TEST_SUITE("assets/assets") {

    TEST_CASE("assets - the umbrella header exposes every public type") {
        static_assert(std::is_same_v<asset_id, core::uuid>);
        static_assert(std::is_class_v<asset_manager>);
        static_assert(std::is_class_v<asset_database>);
        static_assert(std::is_class_v<asset_cache>);
        static_assert(std::is_class_v<asset_cooker>);
        static_assert(std::is_class_v<pack_reader>);
        static_assert(std::is_class_v<pack_writer>);
        static_assert(std::is_class_v<polling_file_watcher>);
        static_assert(std::is_class_v<hot_reload_controller>);
        static_assert(std::is_class_v<binary_importer>);
        static_assert(std::is_abstract_v<asset_loader>);
        static_assert(std::is_abstract_v<asset_importer>);
        static_assert(std::is_abstract_v<asset_source>);
        static_assert(std::is_abstract_v<file_watcher>);
        static_assert(std::is_trivially_copyable_v<importer_extension_desc>);
        CHECK(make_asset_type_id(asset_type_traits<binary_asset>::NAME) == asset_type_traits<binary_asset>::id());
        CHECK(std::string_view(LOG_ASSETS.name()) == "assets");
    }
}
