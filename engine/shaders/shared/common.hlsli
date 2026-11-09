// Shared declarations for every Conjure shader (docs/design/02_renderer.md 3.12).
//
// Shaders are written to the HLSL 2018 subset dxc and glslang both accept: no templates, no wave
// intrinsics, no 16-bit types, no enum class. Register space N is descriptor set N on Vulkan
// (registerSpaceIsDescriptorSet = true everywhere), and the SPIR-V binding shifts applied by
// conjure_add_shaders() match NVRHI's default VulkanBindingOffsets (t:0, s:128, b:256, u:384).

#ifndef CONJURE_COMMON_HLSLI
#define CONJURE_COMMON_HLSLI

// Push constants are declared as `CONJURE_PUSH_CONSTANTS cbuffer draw_constants : register(b1, space0)`:
// a plain constant buffer on D3D12 (root constants through NVRHI), a push-constant block on Vulkan.
#if defined(CONJURE_VULKAN)
    #define CONJURE_PUSH_CONSTANTS [[vk::push_constant]]
#else
    #define CONJURE_PUSH_CONSTANTS
#endif

// NVRHI's Vulkan backend flips Y through a negative viewport height, so clip-space code is shared.
static const float CONJURE_PI = 3.14159265358979323846;

#endif // CONJURE_COMMON_HLSLI
