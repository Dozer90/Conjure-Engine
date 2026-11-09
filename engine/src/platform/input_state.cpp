#include "conjure/platform/input_state.h"

#include <cstddef>

// input_state is header-only; this translation unit pins the layout the scripting frame block
// (conjure_input_block, docs/design/03_scripting.md 2.1) mirrors field by field (P6). A change to
// any of these numbers is an ABI change for the C# side.

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
