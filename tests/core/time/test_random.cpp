#include "conjure/testing/doctest.h"

#include "conjure/testing/assert_capture.h"

#include "conjure/core/random.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <set>
#include <thread>
#include <vector>

using namespace conjure;
using namespace conjure::core;

namespace {
    constexpr int SAMPLE_COUNT = 2000;

    std::vector<u64> draw_sequence(random::engine &generator, usize count) {
        std::vector<u64> values = {};
        values.reserve(count);
        for (usize index = 0; index < count; ++index) {
            values.push_back(generator());
        }
        return values;
    }

    struct simple_vec3 {
        f32 x = 0.0f;
        f32 y = 0.0f;
        f32 z = 0.0f;
    };

    // Restores the process seed after a test that changes it so other tests stay deterministic.
    class seed_guard {
        private:
            u64 mPrevious = random::current_seed();

        public:
            seed_guard() = default;
            ~seed_guard() {
                random::seed(mPrevious);
            }

            seed_guard(const seed_guard &) = delete;
            seed_guard &operator=(const seed_guard &) = delete;
    };
}

static_assert(std::uniform_random_bit_generator<random::random_engine>);
static_assert(std::is_same_v<random::engine, std::mt19937_64>);

TEST_SUITE("core/random") {

    TEST_CASE("random - the engine alias is the standard mt19937_64 with its mandated sequence") {
        // The C++ standard fixes the 10000th output of a default-constructed mt19937_64.
        random::engine generator = {};
        generator.discard(9999);
        CHECK(generator() == 9981545732273789042ull);
    }

    TEST_CASE("random - random_engine replays the same sequence for the same seed") {
        random::random_engine first(42);
        random::random_engine second(42);

        CHECK(first.current_seed() == 42);
        CHECK(first == second);
        for (int index = 0; index < 100; ++index) {
            CHECK(first() == second());
        }

        random::random_engine different(43);
        CHECK(different() != random::random_engine(42)());
    }

    TEST_CASE("random - random_engine defaults to DEFAULT_SEED and seed restarts the sequence") {
        random::random_engine generator = {};
        CHECK(generator.current_seed() == random::DEFAULT_SEED);
        CHECK(generator == random::random_engine(random::DEFAULT_SEED));

        const u64 firstDraw = generator();
        generator.discard(2);

        generator.seed(random::DEFAULT_SEED);
        CHECK(generator() == firstDraw);

        generator.seed(7);
        CHECK(generator.current_seed() == 7);
        CHECK(generator() == random::random_engine(7)());
    }

    TEST_CASE("random - random_engine discard and raw access advance the same state") {
        random::random_engine generator(1);
        random::engine reference(1);

        generator.discard(10);
        reference.discard(10);
        CHECK(generator() == reference());
        CHECK(generator.raw()() == reference());
    }

    TEST_CASE("random - random_engine satisfies the uniform random bit generator interface") {
        CHECK(random::random_engine::min() == random::engine::min());
        CHECK(random::random_engine::max() == random::engine::max());

        random::random_engine generator(3);
        std::vector<int> values = {1, 2, 3, 4, 5, 6, 7, 8};
        std::shuffle(values.begin(), values.end(), generator);
        CHECK(values.size() == 8);
    }

    TEST_CASE("random - default_engine is seeded with DEFAULT_SEED until seed is called") {
        const seed_guard guard = {};

        random::seed(random::DEFAULT_SEED);
        CHECK(random::current_seed() == random::DEFAULT_SEED);
        CHECK(random::default_engine()() == random::engine(random::DEFAULT_SEED)());
    }

    TEST_CASE("random - seed reseeds the calling thread's default engine and records the process seed") {
        const seed_guard guard = {};

        random::seed(1234);
        CHECK(random::current_seed() == 1234);
        const std::vector<u64> first = draw_sequence(random::default_engine(), 16);

        random::seed(1234);
        const std::vector<u64> second = draw_sequence(random::default_engine(), 16);
        CHECK(first == second);

        random::seed(4321);
        const std::vector<u64> third = draw_sequence(random::default_engine(), 16);
        CHECK(first != third);

        random::engine expected(4321);
        CHECK(third == draw_sequence(expected, 16));
    }

    TEST_CASE("random - default_engine returns the same engine for repeated calls on one thread") {
        random::engine &first = random::default_engine();
        random::engine &second = random::default_engine();
        CHECK(&first == &second);
    }

    TEST_CASE("random - a new thread's default engine starts from the process seed") {
        const seed_guard guard = {};
        random::seed(99);

        std::vector<u64> fromThread = {};
        std::vector<u64> fromSecondThread = {};
        std::thread worker([&fromThread] {
            fromThread = draw_sequence(random::default_engine(), 8);
        });
        worker.join();
        std::thread secondWorker([&fromSecondThread] {
            fromSecondThread = draw_sequence(random::default_engine(), 8);
        });
        secondWorker.join();

        random::engine expected(99);
        CHECK(fromThread == draw_sequence(expected, 8));
        CHECK(fromThread == fromSecondThread);

        // The main thread's engine is separate: the worker's draws did not consume its state.
        random::seed(99);
        const std::vector<u64> mainDraws = draw_sequence(random::default_engine(), 8);
        CHECK(mainDraws == fromThread);
    }

    TEST_CASE("random - uniform integers stay inside the inclusive range and reach both ends") {
        random::random_engine generator(5);
        bool sawMin = false;
        bool sawMax = false;

        for (int index = 0; index < SAMPLE_COUNT; ++index) {
            const i32 value = random::uniform(generator, -3, 3);
            CHECK(value >= -3);
            CHECK(value <= 3);
            sawMin = sawMin || value == -3;
            sawMax = sawMax || value == 3;
        }
        CHECK(sawMin);
        CHECK(sawMax);
    }

    TEST_CASE("random - uniform integers cover every value of a small range roughly evenly") {
        random::random_engine generator(11);
        std::array<int, 6> counts = {};

        for (int index = 0; index < 6000; ++index) {
            const u32 value = random::uniform(generator, 1u, 6u);
            REQUIRE(value >= 1);
            REQUIRE(value <= 6);
            ++counts[value - 1];
        }

        for (const int count : counts) {
            CHECK(count > 800);
            CHECK(count < 1200);
        }
    }

    TEST_CASE("random - uniform integers support 8-bit and 64-bit types") {
        random::random_engine generator(8);

        for (int index = 0; index < SAMPLE_COUNT; ++index) {
            const u8 small = random::uniform(generator, u8{250}, u8{255});
            CHECK(small >= 250);

            const i8 signedSmall = random::uniform(generator, i8{-128}, i8{-120});
            CHECK(signedSmall <= -120);

            const i64 wide = random::uniform(generator, i64{-5}, i64{5});
            CHECK(wide >= -5);
            CHECK(wide <= 5);
        }
    }

    TEST_CASE("random - uniform over the full 64-bit range is a plain draw") {
        random::random_engine generator(21);
        random::engine reference(21);

        const u64 value = random::uniform(generator, u64{0}, std::numeric_limits<u64>::max());
        CHECK(value == reference());

        const i64 signedValue =
          random::uniform(generator, std::numeric_limits<i64>::min(), std::numeric_limits<i64>::max());
        CHECK(static_cast<u64>(signedValue) == static_cast<u64>(reference() + 0x8000000000000000ull));
    }

    TEST_CASE("random - uniform with min equal to max returns that value without consuming state") {
        random::random_engine generator(2);
        random::engine reference(2);

        CHECK(random::uniform(generator, 7, 7) == 7);
        CHECK(random::uniform(generator, 2.5, 2.5) == 2.5);
        CHECK(generator() == reference());
    }

    TEST_CASE("random - uniform with min greater than max asserts and returns min") {
        testing::expect_assert asserts = {};
        random::random_engine generator(2);

        CHECK(random::uniform(generator, 10, 1) == 10);
        CHECK(asserts.count() == 1);

        CHECK(random::uniform(generator, 1.0f, 0.0f) == 1.0f);
        CHECK(asserts.count() == 2);
    }

    TEST_CASE("random - uniform floating point values lie in the half-open range") {
        random::random_engine generator(13);
        f64 lowest = 1.0;
        f64 highest = 0.0;

        for (int index = 0; index < SAMPLE_COUNT; ++index) {
            const f64 value = random::uniform(generator, -2.0, 2.0);
            CHECK(value >= -2.0);
            CHECK(value < 2.0);

            const f32 single = random::uniform(generator, 0.0f, 1.0f);
            CHECK(single >= 0.0f);
            CHECK(single < 1.0f);
            lowest = std::min(lowest, static_cast<f64>(single));
            highest = std::max(highest, static_cast<f64>(single));
        }

        CHECK(lowest < 0.05);
        CHECK(highest > 0.95);
    }

    TEST_CASE("random - uniform floating point never returns max even for tiny ranges") {
        random::random_engine generator(17);
        const f32 lowValue = 1.0f;
        const f32 highValue = std::nextafter(lowValue, 2.0f);

        for (int index = 0; index < SAMPLE_COUNT; ++index) {
            CHECK(random::uniform(generator, lowValue, highValue) == lowValue);
        }
    }

    TEST_CASE("random - uniform draws are reproducible across engines with the same seed") {
        random::random_engine first(77);
        random::random_engine second(77);

        for (int index = 0; index < 100; ++index) {
            CHECK(random::uniform(first, 0, 1000) == random::uniform(second, 0, 1000));
            CHECK(random::uniform(first, 0.0, 1.0) == random::uniform(second, 0.0, 1.0));
        }
    }

    TEST_CASE("random - uniform floating point handles ranges whose span overflows") {
        // Regression: min + unit * (max - min) with max - min == inf returned the constant
        // nextafter(max, min) (or NaN) instead of a spread of values.
        random::random_engine generator(19);
        constexpr f64 WIDE = 1.0e308;
        constexpr f32 WIDE_SINGLE = 3.0e38f;
        std::set<f64> distinct = {};
        std::set<f32> distinctSingle = {};

        for (int index = 0; index < 1000; ++index) {
            const f64 value = random::uniform(generator, -WIDE, WIDE);
            CHECK(std::isfinite(value));
            CHECK(value >= -WIDE);
            CHECK(value < WIDE);
            distinct.insert(value);

            const f32 single = random::uniform(generator, -WIDE_SINGLE, WIDE_SINGLE);
            CHECK(std::isfinite(single));
            CHECK(single >= -WIDE_SINGLE);
            CHECK(single < WIDE_SINGLE);
            distinctSingle.insert(single);
        }

        CHECK(distinct.size() > 900);
        CHECK(distinctSingle.size() > 900);
    }

    TEST_CASE("random - seeded helper outputs match the cross-platform golden values") {
        // These literals are the contract: the helpers exist so a seeded replay yields the same
        // values on MSVC, libstdc++ and libc++. Any change to the bit extraction, the rejection
        // threshold or the interpolation formula changes them and must be a deliberate decision.
        // The draws share one engine, so the order below matters as much as the values.
        random::engine generator(random::DEFAULT_SEED);

        constexpr std::array<int, 8> EXPECTED_INTEGERS = {540, 812, 763, 641, 788, 302, 29, 666};
        for (usize index = 0; index < EXPECTED_INTEGERS.size(); ++index) {
            CAPTURE(index);
            CHECK(random::uniform(generator, 0, 1000) == EXPECTED_INTEGERS[index]);
        }

        // Span 5 has a non-zero rejection threshold, so this pins the rejection loop as well.
        constexpr std::array<int, 8> EXPECTED_SMALL_SPAN = {2, 1, 5, 4, 0, 5, 3, 5};
        for (usize index = 0; index < EXPECTED_SMALL_SPAN.size(); ++index) {
            CAPTURE(index);
            CHECK(random::uniform(generator, 0, 5) == EXPECTED_SMALL_SPAN[index]);
        }

        constexpr std::array<f64, 8> EXPECTED_DOUBLES = {
          0.64346416347857138,
          -0.59624934468005475,
          -0.77765818078553139,
          0.091471301735369659,
          0.702906954569533,
          -0.78650085880570741,
          -0.76283882935130931,
          -0.99164441816184845
        };
        for (usize index = 0; index < EXPECTED_DOUBLES.size(); ++index) {
            CAPTURE(index);
            CHECK(random::uniform(generator, -1.0, 1.0) == EXPECTED_DOUBLES[index]);
        }

        constexpr std::array<f32, 8> EXPECTED_SINGLES = {
          0.338744462f,
          0.350513995f,
          0.0374429822f,
          0.267133951f,
          0.278685689f,
          0.177436113f,
          0.178217649f,
          0.82493645f
        };
        for (usize index = 0; index < EXPECTED_SINGLES.size(); ++index) {
            CAPTURE(index);
            CHECK(random::uniform(generator, 0.0f, 1.0f) == EXPECTED_SINGLES[index]);
        }

        constexpr std::array<bool, 8> EXPECTED_CHANCES = {false, true, true, true, false, false, true, false};
        for (usize index = 0; index < EXPECTED_CHANCES.size(); ++index) {
            CAPTURE(index);
            CHECK(random::chance(generator, 0.25) == EXPECTED_CHANCES[index]);
        }

        // cos/sin are not correctly rounded on every platform, so the vector gets a tolerance
        // far below anything an algorithm change would produce.
        const std::array<f32, 3> vector = random::unit_vector3(generator);
        CHECK(static_cast<f64>(vector[0]) == doctest::Approx(0.268532157).epsilon(1.0e-6));
        CHECK(static_cast<f64>(vector[1]) == doctest::Approx(0.200270176).epsilon(1.0e-6));
        CHECK(static_cast<f64>(vector[2]) == doctest::Approx(-0.942221999).epsilon(1.0e-6));

        // Pins how many raw draws the sequence above consumed (including rejections).
        CHECK(generator() == 384405839748580740ull);
    }

    TEST_CASE("random - chance honours the probability extremes") {
        random::random_engine generator(31);

        for (int index = 0; index < 100; ++index) {
            CHECK_FALSE(random::chance(generator, 0.0));
            CHECK_FALSE(random::chance(generator, -1.0));
            CHECK(random::chance(generator, 1.0));
            CHECK(random::chance(generator, 2.0));
        }
    }

    TEST_CASE("random - chance is true about as often as the probability says") {
        random::random_engine generator(37);
        int hits = 0;

        for (int index = 0; index < 10000; ++index) {
            if (random::chance(generator, 0.25)) {
                ++hits;
            }
        }

        CHECK(hits > 2200);
        CHECK(hits < 2800);
    }

    TEST_CASE("random - unit_vector3 produces unit length vectors in every octant") {
        random::random_engine generator(41);
        std::set<int> octants = {};

        for (int index = 0; index < SAMPLE_COUNT; ++index) {
            const std::array<f32, 3> vector = random::unit_vector3(generator);
            const f32 length = std::sqrt(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2]);
            CHECK(static_cast<f64>(length) == doctest::Approx(1.0).epsilon(1.0e-5));

            const int octant = (vector[0] < 0.0f ? 1 : 0) | (vector[1] < 0.0f ? 2 : 0) | (vector[2] < 0.0f ? 4 : 0);
            octants.insert(octant);
        }

        CHECK(octants.size() == 8);
    }

    TEST_CASE("random - unit_vector3 can build any three-float type") {
        random::random_engine generator(43);

        const simple_vec3 vector = random::unit_vector3<simple_vec3>(generator);
        const f32 length = std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
        CHECK(static_cast<f64>(length) == doctest::Approx(1.0).epsilon(1.0e-5));
    }

    TEST_CASE("random - random_engine member helpers match the free functions") {
        random::random_engine viaMembers(53);
        random::random_engine viaFunctions(53);

        CHECK(viaMembers.uniform(1, 100) == random::uniform(viaFunctions, 1, 100));
        CHECK(viaMembers.uniform(0.0f, 5.0f) == random::uniform(viaFunctions, 0.0f, 5.0f));
        CHECK(viaMembers.chance(0.5) == random::chance(viaFunctions, 0.5));
        CHECK(viaMembers.unit_vector3() == random::unit_vector3(viaFunctions));
        CHECK(viaMembers.unit_vector3<simple_vec3>().x == random::unit_vector3<simple_vec3>(viaFunctions).x);
    }

    TEST_CASE("random - helpers accept a 32-bit standard engine as well") {
        std::mt19937 narrow(9);

        for (int index = 0; index < 200; ++index) {
            const int value = random::uniform(narrow, 0, 9);
            CHECK(value >= 0);
            CHECK(value <= 9);

            const f64 real = random::uniform(narrow, 0.0, 1.0);
            CHECK(real >= 0.0);
            CHECK(real < 1.0);
        }
    }
}
