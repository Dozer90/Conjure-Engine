#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"
#include "conjure/testing/log_capture.h"

#include "conjure/core/hash.h"
#include "conjure/core/string_id.h"

#include <atomic>
#include <cstring>
#include <format>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::literals;

namespace {
    // Hashes no real name produces, so the collision tests never disturb interned engine names.
    constexpr u64 COLLISION_HASH_A = 0x0000000000000001ull;
    constexpr u64 COLLISION_HASH_B = 0x0000000000000002ull;
    constexpr u64 COLLISION_HASH_C = 0x0000000000000003ull;

    // Namespace-scope ids are constant-initialised: hashed, never interned (see string_id.h).
    const string_id NAMESPACE_SCOPE_ID("string_id.test.namespace_scope");
    const string_id NAMESPACE_SCOPE_INTERNED = string_id::intern("string_id.test.namespace_scope_interned");

    // An assertion handler that behaves like real user code: it resolves and constructs ids while
    // the string_id collision path is reporting. With the table mutex held across the report this
    // would deadlock.
    std::atomic<int> sReentrantHandlerCalls = 0;
    std::atomic<bool> sReentrantHandlerResolved = false;

    assert_action reentrant_assert_handler(const assert_info &info) {
        sReentrantHandlerCalls.fetch_add(1);
        const string_id constructed = string_id::intern("string_id.test.reentrant.from_handler");
        const bool resolved = constructed.view() == "string_id.test.reentrant.from_handler" &&
          string_id::from_hash(COLLISION_HASH_C).view() == "string_id.test.reentrant.first" &&
          !info.message.empty();
        sReentrantHandlerResolved.store(resolved);
        return assert_action::CONTINUE;
    }

    // A log sink that formats ids, as an editor console resolving component names would.
    class resolving_log_sink final : public log_sink {
        public:
            std::atomic<int> mWrites = 0;
            std::atomic<bool> mResolved = false;

        public:
            void write(const log_message &message) override {
                mWrites.fetch_add(1);
                const std::string formatted = std::format("{}", string_id::from_hash(COLLISION_HASH_C));
                const string_id fromSink(std::string_view(message.category));
                mResolved.store(formatted == "string_id.test.reentrant.first" && !fromSink.is_null());
            }
    };
}

TEST_SUITE("core/string_id") {

    TEST_CASE("string_id - default id is null with hash 0 and no name") {
        const string_id id = {};

        CHECK(id.is_null());
        CHECK(id.hash() == 0);
        CHECK(id == string_id::null());
        CHECK(id.view().empty());
        CHECK(std::strcmp(id.c_str(), "") == 0);
        CHECK(id.to_string() == "#0000000000000000");
    }

    TEST_CASE("string_id - construction is constexpr and hashes with FNV-1a 64") {
        constexpr string_id id("ecs.position");
        static_assert(id.hash() == hash::fnv1a64("ecs.position"));
        static_assert(!id.is_null());
        static_assert(string_id().is_null());
        static_assert(string_id::null().hash() == 0);
        static_assert(string_id::from_hash(42).hash() == 42);
        static_assert(sizeof(string_id) == 8);
        CHECK(id.hash() == hash::fnv1a64("ecs.position"));
    }

    TEST_CASE("string_id - the _sid literal is constexpr and equals the constructor") {
        constexpr string_id literal = "ecs.component"_sid;
        constexpr string_id constructed("ecs.component");
        static_assert(literal == constructed);
        static_assert(literal.hash() == hash::fnv1a64("ecs.component"));
        static_assert("a"_sid != "b"_sid);
        CHECK(literal == constructed);
    }

    TEST_CASE("string_id - the empty name is a non-null id with the FNV offset basis as hash") {
        constexpr string_id empty("");
        static_assert(!empty.is_null());
        static_assert(empty.hash() == hash::FNV1A64_OFFSET_BASIS);
        CHECK(empty != string_id::null());
    }

    TEST_CASE("string_id - runtime construction interns the name") {
        const std::string name = "string_id.test.runtime";
        const string_id id = string_id(std::string_view(name));

        CHECK(id.hash() == hash::fnv1a64(name));
        CHECK(id.view() == name);
        CHECK(std::strcmp(id.c_str(), name.c_str()) == 0);
        CHECK(id.to_string() == name);
        CHECK(std::format("{}", id) == name);
    }

    TEST_CASE(
      "string_id - ids constructed in constant expressions are not interned until the name is seen at runtime"
    ) {
        constexpr string_id lazy("string_id.test.lazy");
        CHECK(lazy.view().empty());
        CHECK(std::format("{}", lazy) == std::format("#{:016x}", lazy.hash()));
        CHECK(lazy.to_string().starts_with("#"));
        CHECK(lazy.to_string().size() == 17);

        const string_id interned = string_id::intern("string_id.test.lazy");
        CHECK(interned == lazy);
        CHECK(lazy.view() == "string_id.test.lazy");
        CHECK(std::format("{}", lazy) == "string_id.test.lazy");
    }

    TEST_CASE("string_id - a runtime construction of the same name interns a constexpr id") {
        constexpr string_id lazy("string_id.test.lazy_runtime");
        CHECK(lazy.view().empty());

        const std::string name = "string_id.test.lazy_runtime";
        const string_id runtime = string_id(std::string_view(name));
        CHECK(runtime == lazy);
        CHECK(lazy.view() == name);
    }

    TEST_CASE("string_id - the _sid literal is compile-time only and never interns") {
        constexpr string_id lazy("string_id.test.literal_runtime");
        CHECK(lazy.view().empty());

        // consteval: hashed during compilation even in a runtime expression, so nothing is interned
        // and the table mutex is never taken.
        const usize before = string_id::interned_count();
        const string_id literal = "string_id.test.literal_runtime"_sid;
        CHECK(literal == lazy);
        CHECK(lazy.view().empty());
        CHECK(string_id::interned_count() == before);
    }

    TEST_CASE("string_id - a comparison against a runtime name interns regardless of how the result is stored") {
        constexpr string_id lazy("string_id.test.const_bool");
        CHECK(lazy.view().empty());

        // A const bool initialiser is trial-constant-evaluated by some compilers; constructing from
        // a std::string keeps the expression genuinely dynamic on all of them.
        const std::string name = "string_id.test.const_bool";
        const bool same = lazy == string_id(std::string_view(name));
        CHECK(same);
        CHECK(lazy.view() == name);
    }

    TEST_CASE("string_id - static and namespace-scope ids are constant-initialised and not interned") {
        // Constant initialisation is mandatory for static storage duration when the initialiser is
        // a constant expression ([basic.start.static]), so the constructor never interns here.
        static const string_id staticConst("string_id.test.static_const");
        static const string_id staticLiteral = "string_id.test.static_literal"_sid;
        CHECK(staticConst.hash() == hash::fnv1a64("string_id.test.static_const"));
        CHECK(staticConst.view().empty());
        CHECK(staticConst.to_string() == std::format("#{:016x}", staticConst.hash()));
        CHECK(staticLiteral.view().empty());
        CHECK(NAMESPACE_SCOPE_ID.view().empty());

        // intern() is the documented way to give a static id a visible name.
        static const string_id staticInterned = string_id::intern("string_id.test.static_interned");
        CHECK(staticInterned.view() == "string_id.test.static_interned");
        CHECK(NAMESPACE_SCOPE_INTERNED.view() == "string_id.test.namespace_scope_interned");
        CHECK(NAMESPACE_SCOPE_INTERNED.hash() == hash::fnv1a64("string_id.test.namespace_scope_interned"));
    }

    TEST_CASE("string_id - intern returns the same id as construction and counts each new name once") {
        const usize before = string_id::interned_count();

        const string_id first = string_id::intern("string_id.test.count");
        CHECK(string_id::interned_count() == before + 1);

        const string_id second = string_id::intern("string_id.test.count");
        CHECK(second == first);
        CHECK(string_id::interned_count() == before + 1);
        CHECK(first.view() == "string_id.test.count");

        const string_id constructed(std::string_view("string_id.test.count"));
        CHECK(constructed == first);
        CHECK(string_id::interned_count() == before + 1);
    }

    TEST_CASE("string_id - from_hash wraps a hash without interning") {
        const usize before = string_id::interned_count();
        const string_id wrapped = string_id::from_hash(0x1234'5678'9ABC'DEF0ull);

        CHECK(wrapped.hash() == 0x1234'5678'9ABC'DEF0ull);
        CHECK_FALSE(wrapped.is_null());
        CHECK(wrapped.view().empty());
        CHECK(std::format("{}", wrapped) == "#123456789abcdef0");
        CHECK(string_id::interned_count() == before);
        CHECK(string_id::from_hash(0).is_null());
    }

    TEST_CASE("string_id - ids compare and order by hash") {
        const string_id low = string_id::from_hash(1);
        const string_id high = string_id::from_hash(2);

        CHECK(low < high);
        CHECK(high > low);
        CHECK(low <= low);
        CHECK(low == string_id::from_hash(1));
        CHECK(low != high);
        CHECK((low <=> high) == std::strong_ordering::less);

        std::set<string_id> ordered = {high, low, string_id::null()};
        REQUIRE(ordered.size() == 3);
        CHECK(*ordered.begin() == string_id::null());
        CHECK(*ordered.rbegin() == high);
    }

    TEST_CASE("string_id - std::hash forwards the id hash so ids work as unordered keys") {
        const string_id id = "string_id.test.hash_key"_sid;
        CHECK(std::hash<string_id>{}(id) == id.hash());

        std::unordered_map<string_id, int> map = {};
        map[id] = 7;
        map["string_id.test.other_key"_sid] = 8;
        CHECK(map.size() == 2);
        CHECK(map.at(id) == 7);
        CHECK(map.at(string_id("string_id.test.other_key")) == 8);
    }

    TEST_CASE("string_id - formatter prints the name when known and the hex hash otherwise, honouring format specs") {
        const string_id known(std::string_view("string_id.test.format"));
        const string_id unknown = string_id::from_hash(0xABCDull);

        CHECK(std::format("{}", known) == "string_id.test.format");
        CHECK(std::format("[{:>23}]", known) == "[  string_id.test.format]");
        CHECK(std::format("{}", unknown) == "#000000000000abcd");
        CHECK(std::format("{:<18}|", unknown) == "#000000000000abcd |");
        CHECK(std::format("{}", string_id::null()) == "#0000000000000000");
    }

    TEST_CASE("string_id - interning the same name twice is not a collision") {
        testing::expect_assert asserts = {};
        testing::log_capture logs = {};

        detail::register_string_id_name(COLLISION_HASH_B, "string_id.test.same");
        detail::register_string_id_name(COLLISION_HASH_B, "string_id.test.same");

        CHECK(asserts.count() == 0);
        CHECK(logs.count(log_level::ERR) == 0);
        CHECK(detail::find_string_id_name(COLLISION_HASH_B) == "string_id.test.same");
    }

    TEST_CASE(
      "string_id - interning a different name under an existing hash asserts, logs ERROR and keeps the first name"
    ) {
        testing::expect_assert asserts = {};
        testing::log_capture logs = {};
        logs.allow_errors();

        detail::register_string_id_name(COLLISION_HASH_A, "string_id.test.alpha");
        CHECK(asserts.count() == 0);

        detail::register_string_id_name(COLLISION_HASH_A, "string_id.test.beta");
        CHECK(asserts.count() == 1);
        CHECK(asserts.last_message().find("string_id.test.alpha") != std::string::npos);
        CHECK(asserts.last_message().find("string_id.test.beta") != std::string::npos);

        REQUIRE(logs.count(log_level::ERR) == 1);
        CHECK(logs.contains("collision"));
        CHECK(logs.contains("string_id.test.alpha"));
        CHECK(logs.contains("string_id.test.beta"));
        CHECK(logs.contains("0x0000000000000001"));

        CHECK(detail::find_string_id_name(COLLISION_HASH_A) == "string_id.test.alpha");
        CHECK(string_id::from_hash(COLLISION_HASH_A).view() == "string_id.test.alpha");
    }

    TEST_CASE("string_id - the public constructor and intern hit the collision path") {
        testing::expect_assert asserts = {};
        testing::log_capture logs = {};
        logs.allow_errors();

        // Pre-register a fake spelling under the real hash of the test name; no FNV collision needed.
        const std::string name = "string_id.test.collide";
        detail::register_string_id_name(hash::fnv1a64(name), "string_id.test.collide_other");
        CHECK(asserts.count() == 0);

        const string_id constructed = string_id(std::string_view(name));
        CHECK(asserts.count() == 1);
        CHECK(constructed.hash() == hash::fnv1a64(name));
        CHECK(constructed.view() == "string_id.test.collide_other");

        const string_id interned = string_id::intern(name);
        CHECK(asserts.count() == 2);
        CHECK(interned == constructed);
        CHECK(interned.view() == "string_id.test.collide_other");
        CHECK(logs.count(log_level::ERR) == 2);
        CHECK(logs.contains("string_id.test.collide_other"));
    }

    TEST_CASE("string_id - the collision report runs without the table lock so handlers and sinks may use ids") {
        testing::log_capture quiet = {};
        quiet.allow_errors();
        sReentrantHandlerCalls.store(0);
        sReentrantHandlerResolved.store(false);
        const assert_handler previous = set_assert_handler(&reentrant_assert_handler);
        const std::shared_ptr<resolving_log_sink> sink = std::make_shared<resolving_log_sink>();
        log::add_sink(sink);

        detail::register_string_id_name(COLLISION_HASH_C, "string_id.test.reentrant.first");
        detail::register_string_id_name(COLLISION_HASH_C, "string_id.test.reentrant.second");

        log::remove_sink(sink);
        set_assert_handler(previous);

        CHECK(sReentrantHandlerCalls.load() == 1);
        CHECK(sReentrantHandlerResolved.load());
        CHECK(sink->mWrites.load() >= 1);
        CHECK(sink->mResolved.load());
        CHECK(string_id::from_hash(COLLISION_HASH_C).view() == "string_id.test.reentrant.first");
        CHECK(string_id::from_hash(hash::fnv1a64("string_id.test.reentrant.from_handler")).view() ==
          "string_id.test.reentrant.from_handler");
    }

    TEST_CASE("string_id - unknown hashes resolve to an empty, null-terminated name") {
        const std::string_view missing = detail::find_string_id_name(0xFFFF'FFFF'FFFF'FFF0ull);
        CHECK(missing.empty());
        REQUIRE(missing.data() != nullptr);
        CHECK(missing.data()[0] == '\0');
    }

    TEST_CASE("string_id - interning from 8 threads is safe and every name resolves afterwards") {
        testing::expect_assert asserts = {};
        constexpr int THREAD_COUNT = 8;
        constexpr int PER_THREAD = 500;
        constexpr int SHARED_COUNT = 64;

        std::atomic<int> ready = 0;
        std::atomic<bool> go = false;
        std::atomic<int> mismatches = 0;
        std::vector<std::thread> threads = {};
        for (int threadIndex = 0; threadIndex < THREAD_COUNT; ++threadIndex) {
            threads.emplace_back([threadIndex, &ready, &go, &mismatches] {
                ready.fetch_add(1);
                while (!go.load()) {
                    std::this_thread::yield();
                }
                for (int index = 0; index < PER_THREAD; ++index) {
                    // Every thread interns the shared names (contention on one key) and its own.
                    const std::string shared = std::format("string_id.test.shared.{}", index % SHARED_COUNT);
                    const string_id sharedId = string_id(std::string_view(shared));
                    if (sharedId.view() != shared) {
                        mismatches.fetch_add(1);
                    }

                    const std::string own = std::format("string_id.test.thread.{}.{}", threadIndex, index);
                    const string_id ownId = string_id::intern(own);
                    if (ownId.view() != own) {
                        mismatches.fetch_add(1);
                    }
                }
            });
        }
        while (ready.load() < THREAD_COUNT) {
            std::this_thread::yield();
        }
        go.store(true);
        for (std::thread &thread : threads) {
            thread.join();
        }

        CHECK(asserts.count() == 0);
        CHECK(mismatches.load() == 0);
        for (int threadIndex = 0; threadIndex < THREAD_COUNT; ++threadIndex) {
            for (int index = 0; index < PER_THREAD; ++index) {
                const std::string own = std::format("string_id.test.thread.{}.{}", threadIndex, index);
                CHECK(string_id::from_hash(hash::fnv1a64(own)).view() == own);
            }
        }
        for (int index = 0; index < SHARED_COUNT; ++index) {
            const std::string shared = std::format("string_id.test.shared.{}", index);
            CHECK(string_id::from_hash(hash::fnv1a64(shared)).view() == shared);
        }
    }

    TEST_CASE("string_id - threads racing to intern the same new names store each once without a collision") {
        // Every thread registers the same fresh names in the same order, so most first registrations
        // miss under the shared lock on several threads at once and meet again at the exclusive insert,
        // which must recognise the name another thread has just stored.
        testing::expect_assert asserts = {};
        testing::log_capture logs = {};
        constexpr int THREAD_COUNT = 8;
        constexpr int NAME_COUNT = 256;
        const usize before = string_id::interned_count();

        std::atomic<int> ready = 0;
        std::atomic<bool> go = false;
        std::atomic<int> mismatches = 0;
        std::vector<std::thread> threads = {};
        for (int threadIndex = 0; threadIndex < THREAD_COUNT; ++threadIndex) {
            threads.emplace_back([&ready, &go, &mismatches] {
                ready.fetch_add(1);
                while (!go.load()) {
                    std::this_thread::yield();
                }
                for (int index = 0; index < NAME_COUNT; ++index) {
                    const std::string name = std::format("string_id.test.race.{}", index);
                    if (string_id(std::string_view(name)).view() != name) {
                        mismatches.fetch_add(1);
                    }
                }
            });
        }
        while (ready.load() < THREAD_COUNT) {
            std::this_thread::yield();
        }
        go.store(true);
        for (std::thread &thread : threads) {
            thread.join();
        }

        CHECK(asserts.count() == 0);
        CHECK(logs.count(log_level::ERR) == 0);
        CHECK(mismatches.load() == 0);
        CHECK(string_id::interned_count() == before + NAME_COUNT);
    }

    TEST_CASE("string_id - names stay readable while other threads intern") {
        constexpr int READER_COUNT = 4;
        constexpr int WRITER_COUNT = 4;
        constexpr int WRITES = 2000;

        const string_id stable(std::string_view("string_id.test.stable"));
        std::atomic<bool> stop = false;
        std::atomic<int> readerFailures = 0;

        std::vector<std::thread> readers = {};
        for (int readerIndex = 0; readerIndex < READER_COUNT; ++readerIndex) {
            readers.emplace_back([&stable, &stop, &readerFailures] {
                while (!stop.load()) {
                    if (stable.view() != "string_id.test.stable") {
                        readerFailures.fetch_add(1);
                    }
                }
            });
        }

        std::vector<std::thread> writers = {};
        for (int writerIndex = 0; writerIndex < WRITER_COUNT; ++writerIndex) {
            writers.emplace_back([writerIndex] {
                for (int index = 0; index < WRITES; ++index) {
                    const std::string name = std::format("string_id.test.churn.{}.{}", writerIndex, index);
                    const string_id id = string_id::intern(name);
                    CONJURE_UNUSED(id);
                }
            });
        }
        for (std::thread &thread : writers) {
            thread.join();
        }
        stop.store(true);
        for (std::thread &thread : readers) {
            thread.join();
        }

        CHECK(readerFailures.load() == 0);
        const std::string_view last = "string_id.test.churn.3.1999";
        CHECK(string_id::from_hash(hash::fnv1a64(last)).view() == last);
    }
}
