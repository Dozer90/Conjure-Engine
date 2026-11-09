#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "fixture_descriptors.h"

#include "conjure/modules/module_registrar.h"
#include "conjure/modules/module_registration_set.h"

#include "conjure/core/containers/small_vector.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::modules;

namespace {
    struct byte_desc {
        static constexpr string_id EXTENSION_POINT{"test.byte"};

        u8 value = 0;
    };

    struct wide_desc {
        static constexpr string_id EXTENSION_POINT{"test.wide"};

        u64 first = 0;
        u64 second = 0;
    };

    struct null_point_desc {
        static constexpr string_id EXTENSION_POINT = {};

        u32 value = 0;
    };

    void activated(module_context &) {}
    void deactivating(module_context &) {}
}

TEST_SUITE("modules/module_registrar") {

    TEST_CASE("module_registrar - interleaved descriptor types keep their alignment and order") {
        module_registration_set set = {};
        module_registrar registrar(set);

        registrar.register_extension(byte_desc{1});
        registrar.register_extension(wide_desc{10, 11});
        registrar.register_extension(byte_desc{2});
        registrar.register_extension(byte_desc{3});
        registrar.register_extension(wide_desc{20, 21});

        REQUIRE(set.extensions.size() == 5);
        for (const extension_record &record : set.extensions) {
            CAPTURE(record.offset);
            CHECK(record.offset % record.alignment == 0);
            CHECK(record.offset + record.size <= set.extensionStorage.size());
        }
        CHECK(set.extensions[1].alignment == alignof(u64));
        CHECK(set.extensions[1].size == sizeof(wide_desc));
        CHECK(set.extensions[1].offset == 8);
        CHECK(set.extensions[2].offset == 24);
        CHECK(set.extensions[4].offset == 32);

        const small_vector<const byte_desc *, 8> bytes = extensions_of<byte_desc>(set);
        REQUIRE(bytes.size() == 3);
        CHECK(bytes[0]->value == 1);
        CHECK(bytes[1]->value == 2);
        CHECK(bytes[2]->value == 3);

        const small_vector<const wide_desc *, 8> wides = extensions_of<wide_desc>(set);
        REQUIRE(wides.size() == 2);
        CHECK(wides[0]->first == 10);
        CHECK(wides[1]->second == 21);
        CHECK(reinterpret_cast<std::uintptr_t>(wides[1]) % alignof(wide_desc) == 0);
    }

    TEST_CASE("module_registrar - descriptors with pointers are copied by value and read back intact") {
        module_registration_set set = {};
        module_registrar registrar(set);
        static constexpr u32 COUNTER = 7;
        registrar.register_extension(fixtures::probe_desc{&COUNTER, nullptr});

        const small_vector<const fixtures::probe_desc *, 8> probes = extensions_of<fixtures::probe_desc>(set);
        REQUIRE(probes.size() == 1);
        CHECK(probes[0]->activatedCount == &COUNTER);
        CHECK(*probes[0]->activatedCount == 7);
        CHECK(probes[0]->deactivatingCount == nullptr);
    }

    TEST_CASE("module_registrar - managed assemblies are recorded once each and empty paths assert") {
        module_registration_set set = {};
        module_registrar registrar(set);

        registrar.register_managed_assembly("managed/Game.dll");
        registrar.register_managed_assembly("managed/Shared.dll");
        registrar.register_managed_assembly("managed/Game.dll");
        CHECK(set.managedAssemblies == std::vector<std::string>{"managed/Game.dll", "managed/Shared.dll"});

        CONJURE_CHECK_ASSERTS(registrar.register_managed_assembly(""));
        CHECK(set.managedAssemblies.size() == 2);
    }

    TEST_CASE("module_registrar - fail accumulates every reason and an empty reason is named") {
        module_registration_set set = {};
        module_registrar registrar(set);
        CHECK(registrar.registrations().errors.empty());

        registrar.fail("needs the renderer");
        registrar.fail("needs physics");
        registrar.fail("");
        REQUIRE(set.errors.size() == 3);
        CHECK(set.errors[0] == "needs the renderer");
        CHECK(set.errors[1] == "needs physics");
        CHECK(set.errors[2] == "unspecified reason");
        CHECK(&registrar.registrations() == &set);
    }

    TEST_CASE("module_registrar - set_lifecycle installs the callbacks and the last call wins") {
        module_registration_set set = {};
        module_registrar registrar(set);

        registrar.set_lifecycle(module_lifecycle{&activated, nullptr});
        CHECK(set.lifecycle.onActivated == &activated);
        CHECK(set.lifecycle.onDeactivating == nullptr);

        registrar.set_lifecycle(module_lifecycle{nullptr, &deactivating});
        CHECK(set.lifecycle.onActivated == nullptr);
        CHECK(set.lifecycle.onDeactivating == &deactivating);
    }

    TEST_CASE("module_registrar - a descriptor with a null extension point asserts and is not stored") {
        module_registration_set set = {};
        module_registrar registrar(set);
        CONJURE_CHECK_ASSERTS(registrar.register_extension(null_point_desc{1}));
        CHECK(set.extensions.empty());
        CHECK(set.extensionStorage.empty());
    }

    TEST_CASE("module_registrar - an empty set stays empty when nothing is registered") {
        module_registration_set set = {};
        {
            const module_registrar registrar(set);
            CHECK(registrar.registrations().extensions.empty());
        }
        CHECK(set.extensions.empty());
        CHECK(set.managedAssemblies.empty());
        CHECK(set.errors.empty());
    }
}
