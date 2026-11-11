#include "bindings/entry_support.h"

#include "conjure/assets/asset_manager.h"
#include "conjure/assets/asset_ref.h"
#include "conjure/assets/asset_slot.h"
#include "conjure/audio/audio_engine.h"
#include "conjure/audio/audio_error.h"
#include "conjure/audio/clip.h"
#include "conjure/audio/components.h"
#include "conjure/audio/mix_groups.h"
#include "conjure/audio/voice.h"
#include "conjure/core/string_id.h"

#include <atomic>
#include <cmath>
#include <optional>

// audio_* (docs/design/15_bindings.md 2.3, 3.8, B8, R50): entity entries write audio_source.playbackRequest
// (engine.audio_sync consumes it in SYNC and writes isPlaying back), one-shots go to audio_engine::play with
// retain = false, gains address the mix groups by name hash. Built only with CONJURE_WITH_AUDIO.

using namespace conjure;
using namespace conjure::bindings;
using namespace conjure::bindings::detail;

namespace {

    constexpr std::string_view AUDIO_SOURCE = "audio_source";

    [[nodiscard]] audio::audio_engine *audio_engine_of() noexcept {
        return active_context().audio;
    }

    [[nodiscard]] int32_t request(
      native_function_id id,
      uint64_t entity,
      audio::audio_playback_request value
    ) noexcept {
        int32_t status = 0;
        audio::audio_source *source =
          named_component_for_write<audio::audio_source>(
            id, entity, audio::AUDIO_SOURCE_TYPE_NAME, AUDIO_SOURCE, status
          );
        if (source == nullptr) {
            return status;
        }
        source->playbackRequest = value;
        return 0;
    }

    /// <summary>
    /// The mix group behind a name hash: the master group for 0, NOT_FOUND for an unknown name.
    /// </summary>
    [[nodiscard]] bool resolve_group(
      native_function_id id,
      uint64_t groupHash,
      audio::group_id &out,
      int32_t &status
    ) noexcept {
        if (groupHash == 0) {
            out = audio::MASTER_GROUP;
            return true;
        }
        out = audio_engine_of()->find_group(core::string_id::from_hash(groupHash));
        if (!out.is_valid()) {
            status = fail_entry(id, bindings_error::NOT_FOUND, "unknown mix group");
            return false;
        }
        return true;
    }

    /// <summary>
    /// The LOADED clip behind a uuid as a bound reference; records the usual asset failures.
    /// </summary>
    [[nodiscard]] bool resolve_clip(
      native_function_id id,
      const conjure_uuid *clip,
      assets::asset_ref<audio::audio_clip> &out,
      int32_t &status
    ) noexcept {
        if (clip == nullptr) {
            status = fail_entry(id, bindings_error::INVALID_ARGUMENT, "clip");
            return false;
        }
        if (active_context().assets == nullptr) {
            status = unavailable(id, "assets");
            return false;
        }
        const assets::asset_id uuid = marshal::to_uuid(*clip);
        if (uuid.is_null()) {
            status = fail_entry(id, bindings_error::INVALID_ARGUMENT, "null clip uuid");
            return false;
        }
        // A read: the slot is looked up, never created, so an unknown or stale uuid costs nothing permanent.
        const assets::asset_slot *slot = active_context().assets->find_slot(uuid);
        assets::asset_type_id type = assets::asset_type_id::INVALID;
        if (slot != nullptr) {
            type = slot->type.load(std::memory_order_acquire);
        }
        if (type == assets::asset_type_id::INVALID) {
            type = active_context().assets->type_of(uuid).value_or(assets::asset_type_id::INVALID);
        }
        if (type != assets::asset_type_id::INVALID && type != assets::asset_type_traits<audio::audio_clip>::id()) {
            status = fail_entry(id, bindings_error::ASSET_TYPE_MISMATCH, "not a conjure.audio.clip asset");
            return false;
        }
        if (slot == nullptr) {
            status = fail_entry(id, bindings_error::ASSET_NOT_LOADED, "clip is not resident");
            return false;
        }
        out = assets::asset_ref<audio::audio_clip>(uuid, slot);
        if (out.get() == nullptr) {
            status = fail_entry(id, bindings_error::ASSET_NOT_LOADED, "clip is not resident");
            return false;
        }
        return true;
    }

    [[nodiscard]] bool finite_gain(native_function_id id, float value, int32_t &status) noexcept {
        if (!std::isfinite(value)) {
            status = fail_entry(id, bindings_error::INVALID_ARGUMENT, "gain must be finite");
            return false;
        }
        return true;
    }
}

int32_t conjure::bindings::entries::audio_play(uint64_t entity) {
    constexpr native_function_id ID = native_function_id::audio_play;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    return request(ID, entity, audio::audio_playback_request::PLAY);
}

int32_t conjure::bindings::entries::audio_stop(uint64_t entity) {
    constexpr native_function_id ID = native_function_id::audio_stop;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    return request(ID, entity, audio::audio_playback_request::STOP);
}

int32_t conjure::bindings::entries::audio_pause(uint64_t entity) {
    constexpr native_function_id ID = native_function_id::audio_pause;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    return request(ID, entity, audio::audio_playback_request::PAUSE);
}

int32_t conjure::bindings::entries::audio_resume(uint64_t entity) {
    constexpr native_function_id ID = native_function_id::audio_resume;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    return request(ID, entity, audio::audio_playback_request::RESUME);
}

int32_t conjure::bindings::entries::audio_is_playing(uint64_t entity) {
    constexpr native_function_id ID = native_function_id::audio_is_playing;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    int32_t status = 0;
    const audio::audio_source *source =
      named_component_for_read<audio::audio_source>(ID, entity, audio::AUDIO_SOURCE_TYPE_NAME, AUDIO_SOURCE, status);
    if (source == nullptr) {
        return status;
    }
    return source->isPlaying ? 1 : 0;
}

int32_t conjure::bindings::entries::audio_play_one_shot(
  const conjure_uuid *clip,
  const conjure_vec3 *positionOrNull,
  float volume,
  float pitch,
  uint64_t groupHash
) {
    constexpr native_function_id ID = native_function_id::audio_play_one_shot;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (audio_engine_of() == nullptr) {
        return unavailable(ID, "audio");
    }
    if (!std::isfinite(volume) || !std::isfinite(pitch)) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "volume and pitch must be finite");
    }
    int32_t status = 0;
    assets::asset_ref<audio::audio_clip> reference = {};
    if (!resolve_clip(ID, clip, reference, status)) {
        return status;
    }
    audio::group_id group = audio::MASTER_GROUP;
    if (!resolve_group(ID, groupHash, group, status)) {
        return status;
    }
    audio::play_params params = {};
    params.volume = volume;
    params.pitch = pitch;
    params.spatial = positionOrNull != nullptr;
    params.retain = false;
    params.group = group;
    const core::result<audio::voice_id> voice = audio_engine_of()->play(reference, params);
    if (!voice.has_value()) {
        const core::error &failure = voice.get_error();
        if (failure.is(audio::audio_error::CLIP_NOT_LOADED) || failure.is(audio::audio_error::INVALID_CLIP)) {
            return fail_entry(ID, bindings_error::ASSET_NOT_LOADED, failure.message());
        }
        if (failure.is(audio::audio_error::INVALID_GROUP)) {
            return fail_entry(ID, bindings_error::NOT_FOUND, failure.message());
        }
        return fail_entry(ID, failure);
    }
    if (positionOrNull != nullptr) {
        audio_engine_of()->set_position(voice.value(), marshal::to_vec3(*positionOrNull));
    }
    return 0;
}

int32_t conjure::bindings::entries::audio_set_group_gain(uint64_t groupHash, float gain) {
    constexpr native_function_id ID = native_function_id::audio_set_group_gain;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (audio_engine_of() == nullptr) {
        return unavailable(ID, "audio");
    }
    int32_t status = 0;
    if (!finite_gain(ID, gain, status)) {
        return status;
    }
    audio::group_id group = audio::MASTER_GROUP;
    if (!resolve_group(ID, groupHash, group, status)) {
        return status;
    }
    audio_engine_of()->set_group_gain(group, gain);
    return 0;
}

int32_t conjure::bindings::entries::audio_get_group_gain(uint64_t groupHash, float *out) {
    constexpr native_function_id ID = native_function_id::audio_get_group_gain;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (out == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "out");
    }
    if (audio_engine_of() == nullptr) {
        return unavailable(ID, "audio");
    }
    int32_t status = 0;
    audio::group_id group = audio::MASTER_GROUP;
    if (!resolve_group(ID, groupHash, group, status)) {
        return status;
    }
    const std::optional<audio::group_info> info = audio_engine_of()->group(group);
    if (!info.has_value()) {
        return fail_entry(ID, bindings_error::NOT_FOUND, "unknown mix group");
    }
    *out = info->gain;
    return 0;
}

int32_t conjure::bindings::entries::audio_set_master_gain(float gain) {
    constexpr native_function_id ID = native_function_id::audio_set_master_gain;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (audio_engine_of() == nullptr) {
        return unavailable(ID, "audio");
    }
    int32_t status = 0;
    if (!finite_gain(ID, gain, status)) {
        return status;
    }
    audio_engine_of()->set_master_gain(gain);
    return 0;
}

int32_t conjure::bindings::entries::audio_get_master_gain(float *out) {
    constexpr native_function_id ID = native_function_id::audio_get_master_gain;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (out == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "out");
    }
    if (audio_engine_of() == nullptr) {
        return unavailable(ID, "audio");
    }
    const std::optional<audio::group_info> master = audio_engine_of()->group(audio::MASTER_GROUP);
    *out = master.has_value() ? master->gain : 1.0f;
    return 0;
}

int32_t conjure::bindings::entries::audio_clip_duration(const conjure_uuid *clip, float *outSeconds) {
    constexpr native_function_id ID = native_function_id::audio_clip_duration;
    CONJURE_BINDINGS_GUARD_STATUS(ID);
    if (outSeconds == nullptr) {
        return fail_entry(ID, bindings_error::INVALID_ARGUMENT, "outSeconds");
    }
    int32_t status = 0;
    assets::asset_ref<audio::audio_clip> reference = {};
    if (!resolve_clip(ID, clip, reference, status)) {
        return status;
    }
    *outSeconds = static_cast<float>(reference.get()->duration_seconds());
    return 0;
}
