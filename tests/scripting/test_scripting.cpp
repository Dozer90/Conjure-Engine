#include "conjure/testing/doctest.h"

#include "conjure/scripting/scripting.h"

using namespace conjure;
using namespace conjure::scripting;

TEST_SUITE("scripting/scripting") {

    TEST_CASE("scripting - the umbrella header exposes every public type") {
        const script_host_config hostConfig = {};
        CHECK(hostConfig.backend == script_backend::DOTNET);
        const script_runtime_config runtimeConfig = {};
        CHECK(runtimeConfig.initialMode == script_mode::EDIT);
        CHECK(status_of(scripting_error::NOT_INITIALISED) < 0);
        CHECK(MAX_SCRIPTS_PER_ENTITY == 8);
        CHECK(QUERY_MAX_COMPONENTS == 8);
        CHECK(SYSTEM_UPDATE == "scripting.update");
        CHECK(to_string(reload_state::IDLE) == "IDLE");
        CHECK(FIELD_BLOB_MAGIC == 0x42465343u);
        CHECK(std::string_view(LOG_SCRIPTING.name()) == "scripting");
    }
}
