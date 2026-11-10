#include "conjure/testing/doctest.h"

#include "conjure/testing/approx.h"
#include "renderer_test_support.h"

#include "renderer/gpu_types.h"

#include "conjure/renderer/debug_draw.h"
#include "conjure/renderer/limits.h"
#include "conjure/renderer/sprite_batch.h"

#include "conjure/core/math.h"
#include "conjure/core/types.h"

#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

// The GPU type mirror (docs/design/02_renderer.md 2.5, 10 "test_gpu_types"): the #defines of
// engine/shaders/shared/gpu_types.hlsli are re-read from the source tree and compared with the C++
// constants and struct sizes, so the two files cannot drift apart silently.

using namespace conjure;
using namespace conjure::renderer;
using namespace conjure::testing;

namespace {

    /// <summary>
    /// Evaluates the integer expressions the HLSL header uses: decimal and hex literals, defined
    /// names, + - * / and parentheses.
    /// </summary>
    class define_evaluator {
        private:
            const std::map<std::string, std::string> &mDefines;
            std::string_view mText = {};
            usize mPosition = 0;

        public:
            explicit define_evaluator(const std::map<std::string, std::string> &defines) :
                mDefines(defines) {}

            [[nodiscard]] std::optional<i64> evaluate(std::string_view text) {
                mText = text;
                mPosition = 0;
                const std::optional<i64> value = sum();
                skip_spaces();
                if (!value.has_value() || mPosition != mText.size()) {
                    return std::nullopt;
                }
                return value;
            }

        private:
            [[nodiscard]] static bool is_alnum(char character) noexcept {
                return std::isalnum(static_cast<unsigned char>(character)) != 0;
            }

            void skip_spaces() {
                while (mPosition < mText.size() && std::isspace(static_cast<unsigned char>(mText[mPosition])) != 0) {
                    ++mPosition;
                }
            }

            [[nodiscard]] std::optional<i64> sum() {
                std::optional<i64> left = product();
                while (left.has_value()) {
                    skip_spaces();
                    if (mPosition >= mText.size() || (mText[mPosition] != '+' && mText[mPosition] != '-')) {
                        break;
                    }
                    const char op = mText[mPosition++];
                    const std::optional<i64> right = product();
                    if (!right.has_value()) {
                        return std::nullopt;
                    }
                    left = op == '+' ? *left + *right : *left - *right;
                }
                return left;
            }

            [[nodiscard]] std::optional<i64> product() {
                std::optional<i64> left = atom();
                while (left.has_value()) {
                    skip_spaces();
                    if (mPosition >= mText.size() || (mText[mPosition] != '*' && mText[mPosition] != '/')) {
                        break;
                    }
                    const char op = mText[mPosition++];
                    const std::optional<i64> right = atom();
                    if (!right.has_value() || (op == '/' && *right == 0)) {
                        return std::nullopt;
                    }
                    left = op == '*' ? *left * *right : *left / *right;
                }
                return left;
            }

            [[nodiscard]] std::optional<i64> atom() {
                skip_spaces();
                if (mPosition >= mText.size()) {
                    return std::nullopt;
                }
                const char first = mText[mPosition];
                if (first == '(') {
                    ++mPosition;
                    const std::optional<i64> inner = sum();
                    skip_spaces();
                    if (!inner.has_value() || mPosition >= mText.size() || mText[mPosition] != ')') {
                        return std::nullopt;
                    }
                    ++mPosition;
                    return inner;
                }
                if (std::isdigit(static_cast<unsigned char>(first)) != 0) {
                    const usize start = mPosition;
                    while (mPosition < mText.size() && is_alnum(mText[mPosition])) {
                        ++mPosition;
                    }
                    const std::string literal(mText.substr(start, mPosition - start));
                    const bool hex = literal.size() > 2 && literal[0] == '0'
                      && (literal[1] == 'x' || literal[1] == 'X');
                    return static_cast<i64>(std::stoull(literal, nullptr, hex ? 16 : 10));
                }
                if (std::isalpha(static_cast<unsigned char>(first)) != 0 || first == '_') {
                    const usize start = mPosition;
                    while (mPosition < mText.size() && (is_alnum(mText[mPosition]) || mText[mPosition] == '_')) {
                        ++mPosition;
                    }
                    const std::string name(mText.substr(start, mPosition - start));
                    const auto found = mDefines.find(name);
                    if (found == mDefines.end()) {
                        return std::nullopt;
                    }
                    define_evaluator nested(mDefines);
                    return nested.evaluate(found->second);
                }
                return std::nullopt;
            }
    };

    /// <summary>
    /// Reads every "#define NAME value" of the HLSL header.
    /// </summary>
    [[nodiscard]] std::map<std::string, std::string> read_defines(const std::filesystem::path &path) {
        std::map<std::string, std::string> defines = {};
        std::ifstream stream(path);
        REQUIRE_MESSAGE(stream.is_open(), path.string());
        std::string line = {};
        while (std::getline(stream, line)) {
            std::istringstream words(line);
            std::string hash = {};
            std::string name = {};
            words >> hash >> name;
            if (hash != "#define" || name.empty()) {
                continue;
            }
            std::string rest = {};
            std::getline(words, rest);
            const usize comment = rest.find("//");
            if (comment != std::string::npos) {
                rest.erase(comment);
            }
            const usize begin = rest.find_first_not_of(" \t");
            const usize end = rest.find_last_not_of(" \t\r");
            rest = begin == std::string::npos ? std::string() : rest.substr(begin, end - begin + 1);
            defines[name] = rest;
        }
        return defines;
    }

    [[nodiscard]] i64 value_of(const std::map<std::string, std::string> &defines, std::string_view name) {
        const auto found = defines.find(std::string(name));
        REQUIRE_MESSAGE(found != defines.end(), name);
        define_evaluator evaluator(defines);
        const std::optional<i64> value = evaluator.evaluate(found->second);
        REQUIRE_MESSAGE(value.has_value(), name);
        return *value;
    }
}

TEST_SUITE("renderer/gpu_types") {

    TEST_CASE("gpu_types - the HLSL header defines the same budgets, sizes and flags as the C++ side") {
        const std::filesystem::path path = renderer_support::shader_source_directory() / "shared" / "gpu_types.hlsli";
        REQUIRE(std::filesystem::exists(path));
        const std::map<std::string, std::string> defines = read_defines(path);
        CHECK(defines.contains("CONJURE_GPU_TYPES_HLSLI"));

        CHECK(value_of(defines, "CONJURE_MAX_VIEWS") == MAX_VIEWS);
        CHECK(value_of(defines, "CONJURE_MAX_LIGHTS_PER_VIEW") == MAX_LIGHTS_PER_VIEW);
        CHECK(value_of(defines, "CONJURE_MAX_LIGHTS_PER_TILE") == MAX_LIGHTS_PER_TILE);
        CHECK(value_of(defines, "CONJURE_LIGHT_TILE_SIZE") == LIGHT_TILE_SIZE);
        CHECK(value_of(defines, "CONJURE_MAX_SHADOW_CASCADES") == MAX_SHADOW_CASCADES);
        CHECK(value_of(defines, "CONJURE_MAX_SPRITES_PER_FRAME") == MAX_SPRITES_PER_FRAME);
        CHECK(value_of(defines, "CONJURE_MAX_DEBUG_VERTICES") == MAX_DEBUG_VERTICES);
        CHECK(value_of(defines, "CONJURE_MAX_MATERIAL_SLOTS") == MAX_MATERIAL_SLOTS);
        CHECK(value_of(defines, "CONJURE_INVALID_INDEX") == INVALID_INDEX);
        CHECK(value_of(defines, "CONJURE_EXPOSURE_HISTOGRAM_BINS") == EXPOSURE_HISTOGRAM_BINS);
        CHECK(value_of(defines, "CONJURE_BLOOM_MIP_COUNT") == BLOOM_MIP_COUNT);

        CHECK(value_of(defines, "CONJURE_GPU_VIEW_CONSTANTS_SIZE") == sizeof(gpu::view_constants));
        CHECK(value_of(defines, "CONJURE_GPU_INSTANCE_SIZE") == sizeof(gpu::instance));
        CHECK(value_of(defines, "CONJURE_GPU_LIGHT_SIZE") == sizeof(gpu::light));
        CHECK(value_of(defines, "CONJURE_GPU_SHADOW_TILE_SIZE") == sizeof(gpu::shadow_tile));
        CHECK(value_of(defines, "CONJURE_GPU_MATERIAL_CONSTANTS_SIZE") == sizeof(gpu::material_constants));
        CHECK(value_of(defines, "CONJURE_GPU_DRAW_CONSTANTS_SIZE") == sizeof(gpu::draw_constants));
        CHECK(value_of(defines, "CONJURE_GPU_SPRITE_VERTEX_SIZE") == sizeof(sprite_vertex));
        CHECK(value_of(defines, "CONJURE_GPU_DEBUG_VERTEX_SIZE") == sizeof(debug_vertex));
        CHECK(value_of(defines, "CONJURE_GPU_EXPOSURE_STATE_SIZE") == sizeof(gpu::exposure_state));
        CHECK(value_of(defines, "CONJURE_TILE_LIST_STRIDE") == gpu::TILE_LIST_STRIDE);
        CHECK(value_of(defines, "CONJURE_TILE_LIST_B_OFFSET") == gpu::TILE_LIST_B_OFFSET);

        CHECK(value_of(defines, "CONJURE_LIGHT_DIRECTIONAL") == static_cast<i64>(light_type::DIRECTIONAL));
        CHECK(value_of(defines, "CONJURE_LIGHT_POINT") == static_cast<i64>(light_type::POINT));
        CHECK(value_of(defines, "CONJURE_LIGHT_SPOT") == static_cast<i64>(light_type::SPOT));
        CHECK(value_of(defines, "CONJURE_SHADOW_FILTER_HARD") == static_cast<i64>(shadow_filter::HARD));
        CHECK(value_of(defines, "CONJURE_SHADOW_FILTER_PCF") == static_cast<i64>(shadow_filter::PCF));
        CHECK(value_of(defines, "CONJURE_SHADOW_FILTER_PCSS") == static_cast<i64>(shadow_filter::PCSS));
        CHECK(value_of(defines, "CONJURE_TONEMAP_ACES_FITTED") == static_cast<i64>(tonemap_operator::ACES_FITTED));
        CHECK(
          value_of(
            defines,
            "CONJURE_TONEMAP_REINHARD_EXTENDED"
          ) == static_cast<i64>(tonemap_operator::REINHARD_EXTENDED)
        );
        CHECK(value_of(defines, "CONJURE_TONEMAP_AGX") == static_cast<i64>(tonemap_operator::AGX));
        CHECK(value_of(defines, "CONJURE_TONEMAP_NONE") == static_cast<i64>(tonemap_operator::NONE));

        CHECK(value_of(defines, "CONJURE_VIEW_FLAG_NO_POST_PROCESS") == gpu::VIEW_FLAG_NO_POST_PROCESS);
        CHECK(value_of(defines, "CONJURE_VIEW_FLAG_SHADOWS") == gpu::VIEW_FLAG_SHADOWS);
        CHECK(value_of(defines, "CONJURE_VIEW_FLAG_SSAO") == gpu::VIEW_FLAG_SSAO);
        CHECK(value_of(defines, "CONJURE_VIEW_FLAG_ENVIRONMENT") == gpu::VIEW_FLAG_ENVIRONMENT);
        CHECK(value_of(defines, "CONJURE_VIEW_FLAG_AUTO_EXPOSURE") == gpu::VIEW_FLAG_AUTO_EXPOSURE);
        CHECK(value_of(defines, "CONJURE_VIEW_FLAG_BLOOM") == gpu::VIEW_FLAG_BLOOM);
        CHECK(value_of(defines, "CONJURE_MATERIAL_DOUBLE_SIDED") == gpu::MATERIAL_FLAG_DOUBLE_SIDED);
        CHECK(value_of(defines, "CONJURE_MATERIAL_UNLIT") == gpu::MATERIAL_FLAG_UNLIT);
        CHECK(value_of(defines, "CONJURE_INSTANCE_RECEIVES_SHADOWS") == gpu::INSTANCE_FLAG_RECEIVES_SHADOWS);
        CHECK(value_of(defines, "CONJURE_DRAW_FLAG_LIGHT_LIST_B") == gpu::DRAW_FLAG_LIGHT_LIST_B);
        CHECK(value_of(defines, "CONJURE_DRAW_FLAG_SCREEN_SPACE") == gpu::DRAW_FLAG_SCREEN_SPACE);
        CHECK(value_of(defines, "CONJURE_DRAW_FLAG_POINT_SAMPLER") == gpu::DRAW_FLAG_POINT_SAMPLER);
        CHECK(value_of(defines, "CONJURE_DRAW_FLAG_KARIS_AVERAGE") == gpu::DRAW_FLAG_KARIS_AVERAGE);
    }

    TEST_CASE("gpu_types - the local shadow near plane define equals the C++ constant") {
        const std::filesystem::path path = renderer_support::shader_source_directory() / "shared" / "gpu_types.hlsli";
        const std::map<std::string, std::string> defines = read_defines(path);
        const auto found = defines.find("CONJURE_LOCAL_SHADOW_NEAR_PLANE");
        REQUIRE(found != defines.end());
        CHECK(std::stof(found->second) == doctest::Approx(gpu::LOCAL_SHADOW_NEAR_PLANE));
    }

    TEST_CASE("gpu_types - the define evaluator handles the expression forms the header uses") {
        std::map<std::string, std::string> defines = {};
        defines["A"] = "64";
        defines["B"] = "(2 * (A + 1))";
        defines["C"] = "0xFFFFFFFF";
        defines["D"] = "B - A / 2";
        define_evaluator evaluator(defines);
        CHECK(evaluator.evaluate("A") == 64);
        CHECK(evaluator.evaluate("B") == 130);
        CHECK(evaluator.evaluate("C") == 0xFFFFFFFF);
        CHECK(evaluator.evaluate("D") == 98);
        CHECK(evaluator.evaluate("(1 + 2) * 3") == 9);
        CHECK_FALSE(evaluator.evaluate("UNKNOWN").has_value());
        CHECK_FALSE(evaluator.evaluate("(1 + 2").has_value());
        CHECK_FALSE(evaluator.evaluate("1 / 0").has_value());
        CHECK_FALSE(evaluator.evaluate("").has_value());
    }

    TEST_CASE("gpu_types - the view constants lay out the shader's offsets and the instance stores matrix rows") {
        static_assert(offsetof(gpu::view_constants, cameraPosition) == 320, "cameraPosition");
        static_assert(offsetof(gpu::view_constants, counts) == 368, "counts");
        static_assert(offsetof(gpu::view_constants, flags) == 384, "flags");
        static_assert(offsetof(gpu::view_constants, cascadeClipFromWorld) == 480, "cascades");
        static_assert(offsetof(gpu::view_constants, shCoefficients) == 736, "sh");
        static_assert(offsetof(gpu::view_constants, exposureParams) == 848, "exposure");
        static_assert(sizeof(gpu::view_constants) == gpu::VIEW_CONSTANTS_SIZE, "size");
        static_assert(sizeof(gpu::instance) == 96, "instance");

        math::mat4 world = glm::translate(math::mat4(1.0f), math::vec3(1.0f, 2.0f, 3.0f));
        world = glm::scale(world, math::vec3(2.0f, 2.0f, 2.0f));
        const gpu::instance record = gpu::make_instance(world, 42, gpu::INSTANCE_FLAG_RECEIVES_SHADOWS);
        // Rows of the world matrix: row i = (world[0][i], world[1][i], world[2][i], world[3][i]).
        CONJURE_CHECK_APPROX(record.world0, math::vec4(2.0f, 0.0f, 0.0f, 1.0f));
        CONJURE_CHECK_APPROX(record.world1, math::vec4(0.0f, 2.0f, 0.0f, 2.0f));
        CONJURE_CHECK_APPROX(record.world2, math::vec4(0.0f, 0.0f, 2.0f, 3.0f));
        // The normal matrix is the inverse transpose of the upper 3x3: a uniform scale of 2 becomes 0.5.
        CHECK(record.normal0.x == doctest::Approx(0.5f));
        CHECK(record.normal1.y == doctest::Approx(0.5f));
        CHECK(record.normal2.z == doctest::Approx(0.5f));
        CHECK(record.normal0.y == doctest::Approx(0.0f));
        // The entity id and the flags ride in the spare w components.
        CHECK(gpu::float_as_bits(record.normal0.w) == 42);
        CHECK(gpu::float_as_bits(record.normal1.w) == gpu::INSTANCE_FLAG_RECEIVES_SHADOWS);

        // A rotation keeps the normal matrix equal to the rotation.
        const math::mat4 rotation = glm::rotate(math::mat4(1.0f), math::HALF_PI, math::vec3(0.0f, 1.0f, 0.0f));
        const gpu::instance rotated = gpu::make_instance(rotation, 0, 0);
        CONJURE_CHECK_APPROX(math::vec3(rotated.normal0), math::vec3(rotation[0][0], rotation[1][0], rotation[2][0]));
        CONJURE_CHECK_APPROX(math::vec3(rotated.normal2), math::vec3(rotation[0][2], rotation[1][2], rotation[2][2]));
    }

    TEST_CASE("gpu_types - bits_as_float and float_as_bits reinterpret without conversion") {
        CHECK(gpu::float_as_bits(1.0f) == 0x3F800000u);
        CHECK(gpu::bits_as_float(0x3F800000u) == 1.0f);
        CHECK(gpu::float_as_bits(gpu::bits_as_float(0xDEADBEEFu)) == 0xDEADBEEFu);
        CHECK(gpu::float_as_bits(gpu::bits_as_float(0u)) == 0u);
        CHECK(gpu::float_as_bits(gpu::bits_as_float(0xFFFFFFFFu)) == 0xFFFFFFFFu);
        CHECK(gpu::float_as_bits(-0.0f) == 0x80000000u);
    }
}
