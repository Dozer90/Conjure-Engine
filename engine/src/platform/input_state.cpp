#include "conjure/platform/input_state.h"

#include <cstddef>

// input_state is header-only. scripting_subsystem::write_frame_block copies the three key_bitset word
// arrays into conjure_input_block (docs/design/03_scripting.md 2.1) with memcpy and every other field one
// by one, so only the key_bitset size and alignment below are ABI for the C# side. The remaining asserts
// keep gamepad_state and the snapshot compact (end_frame copies it by value); they are not a layout mirror.
// The C block itself is pinned in scripting/native_api.h (sizeof(conjure_input_block) == 392).

using namespace conjure;
using namespace conjure::platform;

static_assert(sizeof(key_bitset) == 64);
static_assert(alignof(key_bitset) == 8);
static_assert(sizeof(gamepad_state) == 40);
static_assert(offsetof(gamepad_state, buttonsDown) == 4);
static_assert(offsetof(gamepad_state, axes) == 16);
static_assert(sizeof(input_state::gamepads) == MAX_GAMEPADS * sizeof(gamepad_state));
static_assert(sizeof(input_state::textCodepoints) == MAX_TEXT_CODEPOINTS_PER_FRAME * sizeof(u32));
static_assert(offsetof(input_state, keysDown) == 8);
static_assert(offsetof(input_state, keysPressed) == 72);
static_assert(offsetof(input_state, keysReleased) == 136);
static_assert(sizeof(input_state) <= 512, "the snapshot copy in end_frame must stay a few cache lines");
