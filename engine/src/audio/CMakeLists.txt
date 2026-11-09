# conjure_audio - mixer, voices, groups, spatialisation, clip asset type and decoding, and the
# miniaudio / null backends (docs/design/12_audio.md). Honours CONJURE_WITH_AUDIO; depends publicly
# on conjure::core and conjure::assets (A12, R43); miniaudio is a private dependency reached only
# from decoder_impl.cpp and miniaudio_backend.cpp.

if(NOT CONJURE_WITH_AUDIO)
    message(STATUS "audio: CONJURE_WITH_AUDIO is OFF, module skipped")
    return()
endif()
if(NOT TARGET conjure::assets)
    message(STATUS "audio: conjure::assets is not available, module skipped")
    return()
endif()

set(CONJURE_AUDIO_PUBLIC_HEADERS
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/audio/audio.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/audio/audio_backend.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/audio/audio_device.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/audio/audio_engine.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/audio/audio_error.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/audio/audio_types.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/audio/clip.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/audio/clip_asset.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/audio/components.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/audio/decoder.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/audio/log_category.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/audio/mix_groups.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/audio/null_backend.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/audio/spatialisation.h
    ${PROJECT_SOURCE_DIR}/engine/include/conjure/audio/voice.h
)

conjure_add_module(audio
    PUBLIC_HEADERS
        ${CONJURE_AUDIO_PUBLIC_HEADERS}
    SOURCES
        audio_backend.cpp
        audio_device.cpp
        audio_engine.cpp
        audio_error.cpp
        audio_types.cpp
        clip.cpp
        clip_asset.cpp
        commands.h
        components.cpp
        decoder_impl.cpp
        group_table.cpp
        group_table.h
        log_category.cpp
        miniaudio_backend.cpp
        miniaudio_backend.h
        mix_groups.cpp
        mixer.cpp
        mixer.h
        null_backend.cpp
        spatialisation.cpp
        spsc_ring.h
        stream_ring.cpp
        stream_ring.h
        voice.cpp
    PUBLIC_DEPS
        conjure::core
        conjure::assets
    PRIVATE_DEPS
        conjure::thirdparty::miniaudio
)
