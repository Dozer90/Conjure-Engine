#pragma once

#include "conjure/core/platform_detect.h"

// The ECS names of the audio components (docs/design/12_audio.md 2.6, R7): audio has no ecs dependency, so
// the engine's audio_subsystem names and registers audio_source and audio_listener. Private to the engine
// module; tests include it through the engine/src include path.

#if defined(CONJURE_WITH_AUDIO) && CONJURE_WITH_AUDIO

#include "conjure/audio/components.h"
#include "conjure/ecs/component.h"

CONJURE_COMPONENT(conjure::audio::audio_source, "conjure.audio.audio_source");
CONJURE_COMPONENT(conjure::audio::audio_listener, "conjure.audio.audio_listener");

#endif
