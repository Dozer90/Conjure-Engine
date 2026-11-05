/*
 * Single translation unit holding the cgltf implementation.
 *
 * cgltf_write.h includes cgltf.h again, so the reader implementation macro must be undefined
 * before the writer is pulled in or the reader body is compiled twice.
 */

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#undef CGLTF_IMPLEMENTATION

#define CGLTF_WRITE_IMPLEMENTATION
#include "cgltf_write.h"
