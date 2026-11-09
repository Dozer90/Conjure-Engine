/*
 * Single translation unit holding the miniaudio implementation. The engine only uses the
 * low-level device and decoder APIs; the convenience engine API, the node graph and the waveform
 * generators are excluded to keep the symbol count down.
 *
 * Ogg Vorbis: miniaudio decodes .ogg only when stb_vorbis is compiled into the same translation
 * unit (docs/design/12_audio.md A8). The header-only pass before miniaudio.h declares the API
 * miniaudio's Vorbis backend needs; the implementation pass after it provides the code.
 */

#define STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENGINE
#define MA_NO_NODE_GRAPH
#define MA_NO_GENERATION
#include "miniaudio.h"

#undef STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"
