/*
 * Single translation unit holding the miniaudio implementation. The engine only uses the
 * low-level device and resource-manager APIs; the convenience engine API is excluded to keep
 * the symbol count down.
 */

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENGINE
#define MA_NO_NODE_GRAPH
#define MA_NO_GENERATION
#include "miniaudio.h"
