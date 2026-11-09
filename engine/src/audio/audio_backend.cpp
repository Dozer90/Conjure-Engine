#include "conjure/audio/audio_backend.h"
#include "audio/miniaudio_backend.h"
#include "conjure/audio/audio_error.h"
#include "conjure/audio/null_backend.h"

#include "conjure/core/error.h"

using namespace conjure;
using namespace conjure::audio;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

audio_backend::audio_backend() {}

audio_backend::~audio_backend() {}


/*===========================================================================*/
/*===========================================================================*/
// Free functions

bool conjure::audio::is_backend_compiled(audio_backend_kind kind) noexcept {
    switch (kind) {
        case audio_backend_kind::AUTO:
        case audio_backend_kind::NULL_BACKEND:
            return true;
        case audio_backend_kind::MINIAUDIO:
            return miniaudio_backend::is_compiled();
    }
    return false;
}

core::result<std::unique_ptr<audio_backend>> conjure::audio::create_backend(audio_backend_kind kind) {
    audio_backend_kind resolved = kind;
    if (resolved == audio_backend_kind::AUTO) {
        resolved = miniaudio_backend::is_compiled() ? audio_backend_kind::MINIAUDIO : audio_backend_kind::NULL_BACKEND;
    }

    switch (resolved) {
        case audio_backend_kind::NULL_BACKEND:
            return std::unique_ptr<audio_backend>(std::make_unique<null_backend>());
        case audio_backend_kind::MINIAUDIO:
            if (!miniaudio_backend::is_compiled()) {
                return core::make_error(core::error_code::UNSUPPORTED, "the miniaudio backend is not compiled in");
            }
            return std::unique_ptr<audio_backend>(std::make_unique<miniaudio_backend>());
        case audio_backend_kind::AUTO:
            break;
    }
    return core::make_error(core::error_code::INVALID_ARGUMENT, "unknown audio backend kind");
}
