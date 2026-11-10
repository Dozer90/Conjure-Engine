#include "conjure/testing/doctest.h"

#include "conjure/scripting/native_api.h"
#include "conjure/scripting/native_api_hash.h"

using namespace conjure;
using namespace conjure::scripting;

TEST_SUITE("scripting/native_api_hash") {

    TEST_CASE("native_api_hash - the generated constants describe the table") {
        CHECK(CONJURE_NATIVE_API_ENTRY_COUNT == 126);
        CHECK(CONJURE_NATIVE_API_CORE_ENTRY_COUNT == 20);
        CHECK(NATIVE_API_ENTRY_COUNT == CONJURE_NATIVE_API_ENTRY_COUNT);
        CHECK(NATIVE_API_CORE_ENTRY_COUNT == CONJURE_NATIVE_API_CORE_ENTRY_COUNT);
        CHECK(static_cast<u64>(CONJURE_NATIVE_API_HASH) != 0);
    }

    TEST_CASE("native_api_hash - the FOR_EACH_ENTRY macro expands once per entry in table order") {
        u32 count = 0;
        bool firstIsLogWrite = false;
#define SCRIPTING_TEST_COUNT_ENTRY(group, name, signature)                                                  \
        if (count == 0) {                                                                                   \
            firstIsLogWrite = std::string_view(#group "_" #name) == "log_write";                            \
        }                                                                                                   \
        ++count;
        CONJURE_NATIVE_API_FOR_EACH_ENTRY(SCRIPTING_TEST_COUNT_ENTRY)
#undef SCRIPTING_TEST_COUNT_ENTRY
        CHECK(count == CONJURE_NATIVE_API_ENTRY_COUNT);
        CHECK(firstIsLogWrite);
    }
}
