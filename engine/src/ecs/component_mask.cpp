#include "conjure/ecs/component.h"

#include "conjure/core/hash.h"

using namespace conjure;
using namespace conjure::ecs;

u64 component_mask::hash() const noexcept {
    // Hash the words byte by byte in little-endian order so the value is identical on every
    // platform (the C# mirror computes the same thing over its ulong array).
    u64 hash = core::hash::FNV1A64_OFFSET_BASIS;
    for (const u64 word : mWords) {
        for (u32 shift = 0; shift < 64; shift += 8) {
            hash = core::hash::fnv1a64_step(hash, static_cast<u8>((word >> shift) & 0xFFu));
        }
    }
    return hash;
}
