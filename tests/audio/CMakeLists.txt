# conjure_audio_tests: one file per public header of conjure/audio plus the topic files of
# docs/design/12_audio.md 10 (test_mixer over the private mixer, test_source_listener,
# test_miniaudio_backend). Everything runs on the null backend; the [audio_device] cases require
# precondition::AUDIO_DEVICE (probe in audio_test_support.cpp) and skip with exit 77 without one.

if(NOT TARGET conjure::audio)
    return()
endif()

conjure_add_test_module(audio
    SOURCES
        audio_test_support.cpp
        audio_test_support.h
        wav_builder.cpp
        wav_builder.h
        test_audio.cpp
        test_audio_backend.cpp
        test_audio_device.cpp
        test_audio_engine.cpp
        test_audio_error.cpp
        test_audio_types.cpp
        test_clip.cpp
        test_clip_asset.cpp
        test_components.cpp
        test_decoder.cpp
        test_log_category.cpp
        test_miniaudio_backend.cpp
        test_mix_groups.cpp
        test_mixer.cpp
        test_null_backend.cpp
        test_source_listener.cpp
        test_spatialisation.cpp
        test_voice.cpp
)

conjure_header_check(conjure_audio)
