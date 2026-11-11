#pragma once

#include "conjure/core/platform_detect.h"

// The spin-wait hint of the target CPU, used by the job queue's lock and idle loops. The compiler and
// architecture branches live here, in the jobs area's backend header next to thread naming, so
// job_system.cpp itself carries no platform conditional (AGENTS.md rule 10).

#if CONJURE_COMPILER_MSVC
    #include <intrin.h>
#endif

namespace conjure::core::jobs::detail {

    /// <summary>
    /// One spin-wait step: hands the core's execution resources to its SMT sibling for a few dozen
    /// cycles (x64 `pause`, AArch64 `yield`) where a yield() syscall would keep them busy.
    /// </summary>
    inline void cpu_pause() noexcept {
        #if CONJURE_ARCH_X64
            #if CONJURE_COMPILER_MSVC
                _mm_pause();
            #else
                __builtin_ia32_pause();
            #endif
        #elif CONJURE_ARCH_ARM64
            #if CONJURE_COMPILER_MSVC
                __yield();
            #else
                __asm__ __volatile__("yield");
            #endif
        #else
            #error "Conjure: cpu_pause has no implementation for this architecture"
        #endif
    }
}
