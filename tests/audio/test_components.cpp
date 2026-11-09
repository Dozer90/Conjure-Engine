#include "conjure/testing/doctest.h"

#include "conjure/audio/components.h"

#include "conjure/assets/asset_types.h"
#include "conjure/core/json/document.h"
#include "conjure/core/json/writer.h"
#include "conjure/core/reflect/json_conversion.h"
#include "conjure/core/reflect/reflect.h"
#include "conjure/core/string_id.h"
#include "conjure/core/uuid.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>

using namespace conjure;
using namespace conjure::audio;
using namespace conjure::core::reflect;

namespace {
    const field_info &field_named(const core::reflect::type_info &info, std::string_view name) {
        const field_info *field = info.find_field(name);
        REQUIRE(field != nullptr);
        return *field;
    }
}

TEST_SUITE("audio/components") {

    TEST_CASE("audio_source - trivially copyable, 88 bytes, defaults per the design") {
        CHECK(std::is_trivially_copyable_v<audio_source>);
        CHECK(sizeof(audio_source) == 88);
        const audio_source source = {};
        CHECK_FALSE(source.clip.is_bound());
        CHECK(source.clip.id().is_null());
        CHECK(source.volume == doctest::Approx(1.0));
        CHECK(source.pitch == doctest::Approx(1.0));
        CHECK_FALSE(source.loop);
        CHECK(source.spatial);
        CHECK(source.playOnStart);
        CHECK_FALSE(source.oneShot);
        CHECK(source.group.is_null());
        CHECK(source.attenuation == attenuation_model::INVERSE);
        CHECK(source.minDistance == doctest::Approx(1.0));
        CHECK(source.maxDistance == doctest::Approx(100.0));
        CHECK(source.rolloff == doctest::Approx(1.0));
        CHECK(source.playbackRequest == audio_playback_request::NONE);
        CHECK_FALSE(source.isPlaying);
        CHECK(source.handle == source_handle{});
    }

    TEST_CASE("audio_source - reflection lists the fields in mirror order with offsetof offsets") {
        const core::reflect::type_info info = describe_struct<audio_source>();
        CHECK(info.name == AUDIO_SOURCE_TYPE_NAME);
        CHECK(info.name == "conjure.audio.audio_source");
        CHECK(info.size == sizeof(audio_source));
        CHECK(info.alignment == alignof(audio_source));
        CHECK(info.isTriviallyCopyable);
        REQUIRE(info.fields.size() == 15);

        const std::array<std::string_view, 15> order = {
          "clip",      "volume",     "pitch",       "loop",        "spatial",         "playOnStart",
          "oneShot",   "group",      "attenuation", "minDistance", "maxDistance",     "rolloff",
          "playbackRequest", "isPlaying", "handle",
        };
        for (usize index = 0; index < order.size(); ++index) {
            CAPTURE(index);
            CHECK(info.fields[index].name == order[index]);
        }

        CHECK(field_named(info, "clip").kind == field_kind::ASSET_REF);
        CHECK(field_named(info, "clip").offset == offsetof(audio_source, clip));
        CHECK(field_named(info, "clip").size == sizeof(assets::asset_ref<audio_clip>));
        CHECK(field_named(info, "clip").type == type_id::from_name(assets::ASSET_REF_TYPE_NAME));
        CHECK(field_named(info, "volume").kind == field_kind::F32);
        CHECK(field_named(info, "volume").offset == offsetof(audio_source, volume));
        CHECK(field_named(info, "volume").attributes.min == 0.0);
        CHECK(field_named(info, "volume").attributes.max == 2.0);
        CHECK(field_named(info, "pitch").attributes.min == 0.25);
        CHECK(field_named(info, "pitch").attributes.max == 4.0);
        CHECK(field_named(info, "loop").kind == field_kind::BOOL);
        CHECK(field_named(info, "loop").offset == offsetof(audio_source, loop));
        CHECK(field_named(info, "group").kind == field_kind::STRING_ID);
        CHECK(field_named(info, "group").offset == offsetof(audio_source, group));
        CHECK(field_named(info, "attenuation").kind == field_kind::ENUM);
        CHECK(field_named(info, "attenuation").offset == offsetof(audio_source, attenuation));
        CHECK(field_named(info, "minDistance").attributes.min == 0.01);
        CHECK(field_named(info, "maxDistance").attributes.min == 0.01);
        CHECK(field_named(info, "rolloff").attributes.min == 0.0);
        CHECK(field_named(info, "rolloff").attributes.max == 10.0);
        CHECK(field_named(info, "rolloff").offset == offsetof(audio_source, rolloff));
        CHECK(field_named(info, "handle").kind == field_kind::STRUCT);
        CHECK(field_named(info, "handle").offset == offsetof(audio_source, handle));

        // Authored fields are visible; the three runtime fields are hidden from the inspector.
        for (usize index = 0; index < 12; ++index) {
            CAPTURE(index);
            CHECK_FALSE(info.fields[index].attributes.hidden);
        }
        CHECK(field_named(info, "playbackRequest").attributes.hidden);
        CHECK(field_named(info, "isPlaying").attributes.hidden);
        CHECK(field_named(info, "handle").attributes.hidden);
        CHECK(type_id_of<audio_source>() == type_id::from_name("conjure.audio.audio_source"));
    }

    TEST_CASE("audio_source - JSON round trip carries the clip uuid and reads back an unbound reference") {
        audio_source source = {};
        source.clip = assets::asset_ref<audio_clip>(assets::asset_id(0x0123456789ABCDEFull, 0x0011223344556677ull));
        source.volume = 0.5f;
        source.pitch = 2.0f;
        source.loop = true;
        source.spatial = false;
        source.playOnStart = false;
        source.oneShot = true;
        source.group = core::string_id("Music");
        source.attenuation = attenuation_model::LINEAR;
        source.minDistance = 2.0f;
        source.maxDistance = 20.0f;
        source.rolloff = 0.5f;

        core::json::writer out = {};
        CONJURE_REQUIRE_OK(to_json(source, out));
        const std::string text = out.finish();
        const core::result<core::json::document> document = core::json::document::parse(text);
        CONJURE_REQUIRE_OK(document);
        const core::json::value root = document.value().root();
        CHECK(root["clip"].as_string() == "01234567-89ab-cdef-0011-223344556677");
        CHECK(root["clip"].as_string() == source.clip.id().to_string());
        CHECK(root["volume"].as_f64() == doctest::Approx(0.5));
        CHECK(root["loop"].as_bool());
        CHECK(root["group"].as_string() == "Music");
        CHECK(root["attenuation"].as_string() == "LINEAR");

        audio_source restored = {};
        CONJURE_REQUIRE_OK(from_json(restored, root));
        CHECK(restored == source);
        CHECK(restored.clip.id() == source.clip.id());
        CHECK_FALSE(restored.clip.is_bound());
        CHECK(restored.clip.get() == nullptr);

        // A clip read from JSON is unbound even when the destination held a bound reference.
        assets::asset_slot slot = {};
        audio_source bound = {};
        bound.clip = assets::asset_ref<audio_clip>(source.clip.id(), &slot);
        CHECK(bound.clip.is_bound());
        CONJURE_REQUIRE_OK(from_json(bound, root));
        CHECK_FALSE(bound.clip.is_bound());
        CHECK(bound.clip.id() == source.clip.id());
    }

    TEST_CASE("play_params_from - copies the authored fields and marks the voice retained") {
        audio_source source = {};
        source.volume = 0.25f;
        source.pitch = 1.5f;
        source.loop = true;
        source.spatial = false;
        source.attenuation = attenuation_model::EXPONENTIAL;
        source.minDistance = 3.0f;
        source.maxDistance = 30.0f;
        source.rolloff = 2.0f;
        const group_id group = {3, 7};

        const play_params params = play_params_from(source, group);
        CHECK(params.volume == doctest::Approx(0.25));
        CHECK(params.pitch == doctest::Approx(1.5));
        CHECK(params.pan == doctest::Approx(0.0));
        CHECK(params.loop);
        CHECK_FALSE(params.spatial);
        CHECK(params.retain);
        CHECK_FALSE(params.startPaused);
        CHECK(params.priority == play_params{}.priority);
        CHECK(params.group == group);
        CHECK(params.startSeconds == doctest::Approx(0.0));
        CHECK(params.attenuation == attenuation_model::EXPONENTIAL);
        CHECK(params.minDistance == doctest::Approx(3.0));
        CHECK(params.maxDistance == doctest::Approx(30.0));
        CHECK(params.rolloff == doctest::Approx(2.0));

        // Values are copied as authored; clamping (A15) happens in the engine.
        source.volume = 5.0f;
        CHECK(play_params_from(source, MASTER_GROUP).volume == doctest::Approx(5.0));
    }

    TEST_CASE("audio_listener - trivially copyable, 8 bytes, defaults") {
        CHECK(std::is_trivially_copyable_v<audio_listener>);
        CHECK(sizeof(audio_listener) == 8);
        const audio_listener listener = {};
        CHECK(listener.gain == doctest::Approx(1.0));
        CHECK(listener.active);
        CHECK(offsetof(audio_listener, gain) == 0);
        CHECK(offsetof(audio_listener, active) == 4);
    }

    TEST_CASE("audio_listener - reflection lists the fields in order with offsetof offsets and ranges") {
        const core::reflect::type_info info = describe_struct<audio_listener>();
        CHECK(info.name == AUDIO_LISTENER_TYPE_NAME);
        CHECK(info.name == "conjure.audio.audio_listener");
        CHECK(info.size == sizeof(audio_listener));
        CHECK(info.isTriviallyCopyable);
        REQUIRE(info.fields.size() == 2);
        CHECK(info.fields[0].name == "gain");
        CHECK(info.fields[0].kind == field_kind::F32);
        CHECK(info.fields[0].offset == offsetof(audio_listener, gain));
        CHECK(info.fields[0].attributes.min == 0.0);
        CHECK(info.fields[0].attributes.max == 2.0);
        CHECK_FALSE(info.fields[0].attributes.hidden);
        CHECK(info.fields[1].name == "active");
        CHECK(info.fields[1].kind == field_kind::BOOL);
        CHECK(info.fields[1].offset == offsetof(audio_listener, active));
        CHECK(type_id_of<audio_listener>() == type_id::from_name("conjure.audio.audio_listener"));
        CHECK(type_info_of<audio_listener>() != nullptr);
    }

    TEST_CASE("audio_listener - JSON round trip through the reflection layer") {
        audio_listener listener = {};
        listener.gain = 0.75f;
        listener.active = false;
        core::json::writer out = {};
        CONJURE_REQUIRE_OK(to_json(listener, out));
        const std::string text = out.finish();
        CHECK(text == R"({"gain":0.75,"active":false})");

        const core::result<core::json::document> document = core::json::document::parse(text);
        CONJURE_REQUIRE_OK(document);
        audio_listener restored = {};
        CONJURE_REQUIRE_OK(from_json(restored, document.value().root()));
        CHECK(restored == listener);
    }

    TEST_CASE("source_handle - 16 bytes, trivially copyable, hidden everywhere") {
        CHECK(std::is_trivially_copyable_v<source_handle>);
        CHECK(sizeof(source_handle) == 16);
        const source_handle handle = {};
        CHECK_FALSE(handle.voice.is_valid());
        CHECK(handle.clipVersion == 0);
        CHECK(handle.flags == 0);
        CHECK_FALSE(handle.started_once());
        source_handle started = handle;
        started.flags |= source_handle::FLAG_STARTED_ONCE;
        CHECK(started.started_once());
        CHECK(started != handle);

        const core::reflect::type_info info = describe_struct<source_handle>();
        CHECK(info.name == SOURCE_HANDLE_TYPE_NAME);
        REQUIRE(info.fields.size() == 3);
        for (const field_info &field : info.fields) {
            CAPTURE(field.name);
            CHECK(field.attributes.hidden);
        }
        CHECK(info.fields[0].name == "voice");
        CHECK(info.fields[0].kind == field_kind::STRUCT);
        CHECK(info.fields[0].offset == offsetof(source_handle, voice));
        CHECK(info.fields[1].name == "clipVersion");
        CHECK(info.fields[1].kind == field_kind::U32);
        CHECK(info.fields[2].name == "flags");
        CHECK(info.fields[2].kind == field_kind::U8);
    }

    TEST_CASE("audio_playback_request - NONE is zero and the enum is reflected") {
        CHECK(static_cast<u8>(audio_playback_request::NONE) == 0);
        const enum_info info = describe_enum<audio_playback_request>();
        CHECK(info.name == PLAYBACK_REQUEST_TYPE_NAME);
        CHECK(info.underlyingSize == 1);
        CHECK_FALSE(info.isFlags);
        REQUIRE(info.values.size() == 5);
        CHECK(info.values[0].first == "NONE");
        CHECK(info.values[1].first == "PLAY");
        CHECK(info.values[2].first == "STOP");
        CHECK(info.values[3].first == "PAUSE");
        CHECK(info.values[4].first == "RESUME");
        CHECK(info.find_value("RESUME") == static_cast<i64>(audio_playback_request::RESUME));
    }

    TEST_CASE("attenuation_model - reflected with its four enumerators") {
        const enum_info info = describe_enum<attenuation_model>();
        CHECK(info.name == ATTENUATION_MODEL_TYPE_NAME);
        CHECK(info.name == "conjure.audio.attenuation_model");
        CHECK(info.underlyingSize == 1);
        REQUIRE(info.values.size() == 4);
        CHECK(info.values[0].first == "NONE");
        CHECK(info.values[1].first == "INVERSE");
        CHECK(info.values[2].first == "LINEAR");
        CHECK(info.values[3].first == "EXPONENTIAL");
        CHECK(info.find_value("LINEAR") == static_cast<i64>(attenuation_model::LINEAR));
        CHECK(kind_of<attenuation_model>() == field_kind::ENUM);
    }
}
