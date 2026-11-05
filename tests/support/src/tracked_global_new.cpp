// Global operator new/delete replacement for test executables (docs/design/08_testing.md D3).
//
// Compiled into every test executable by conjure_add_test_module(), never into the engine or the
// support library: here every heap allocation of the process (std containers included) goes through
// core::memory::tracked_allocate / tracked_free and therefore bumps the allocation counters, which
// gives memory_guard and allocation_scope exact counts. Shipped binaries keep the C runtime's
// operator new.
//
// Engine code never throws, so an exhausted heap ends the process instead of raising
// std::bad_alloc; a test that runs out of memory has failed in any case.
//
// Constraint (Windows): every operator delete below releases through tracked_free, which is
// _aligned_free on Windows. That is only correct for memory that came from this executable's
// replaced operator new. A replacement of the global allocation functions never reaches code
// compiled into a DLL, whose operator new is the C runtime's (malloc-compatible), so a heap object
// allocated inside a shared library and released by the test executable (a std::string, std::vector
// or unique_ptr handed across the boundary) would be freed with _aligned_free on a malloc pointer:
// heap corruption. Today's test DLLs only pass integers across, so nothing breaks; before
// conjure_add_test_module(LINK_ENGINE_SHARED) links conjure::shared (07_modules D1) this backend has
// to become CRT-compatible: malloc/free for alignments <= __STDCPP_DEFAULT_NEW_ALIGNMENT__ (MSVC x64
// malloc already returns 16-byte aligned blocks, matching the CRT's operator new) and
// _aligned_malloc/_aligned_free only for the align_val_t overloads (matching the CRT's aligned
// operator new/delete). That needs either a tracked_free that knows the alignment or counting hooks
// (record_allocation / record_free) the delete overloads can pair with the matching system free;
// it is an open core::memory API request. On POSIX posix_memalign memory is free()-compatible, so
// only the counters would skew there.

#include "conjure/core/memory/tracked_allocation.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace {
    // alignof(std::max_align_t) is 8 on MSVC x64 while operator new guarantees 16: pass the
    // guarantee explicitly (see the note on DEFAULT_ALIGNMENT in tracked_allocation.h).
    constexpr std::size_t NEW_ALIGNMENT = __STDCPP_DEFAULT_NEW_ALIGNMENT__;

    void *allocate_or_terminate(std::size_t size, std::size_t alignment) noexcept {
        void *pointer = conjure::core::memory::tracked_allocate(size, alignment);
        if (pointer == nullptr) {
            std::fprintf(stderr, "conjure tests: out of memory (%zu bytes, alignment %zu)\n", size, alignment);
            std::fflush(stderr);
            std::abort();
        }
        return pointer;
    }
}

/*===========================================================================*/
/*===========================================================================*/
// Allocation

void *operator new(std::size_t size) {
    return allocate_or_terminate(size, NEW_ALIGNMENT);
}

void *operator new[](std::size_t size) {
    return allocate_or_terminate(size, NEW_ALIGNMENT);
}

void *operator new(std::size_t size, std::align_val_t alignment) {
    return allocate_or_terminate(size, static_cast<std::size_t>(alignment));
}

void *operator new[](std::size_t size, std::align_val_t alignment) {
    return allocate_or_terminate(size, static_cast<std::size_t>(alignment));
}

void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
    return conjure::core::memory::tracked_allocate(size, NEW_ALIGNMENT);
}

void *operator new[](std::size_t size, const std::nothrow_t &) noexcept {
    return conjure::core::memory::tracked_allocate(size, NEW_ALIGNMENT);
}

void *operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept {
    return conjure::core::memory::tracked_allocate(size, static_cast<std::size_t>(alignment));
}

void *operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t &) noexcept {
    return conjure::core::memory::tracked_allocate(size, static_cast<std::size_t>(alignment));
}


/*===========================================================================*/
/*===========================================================================*/
// Deallocation (the size is informational for tracked_free, so the unsized forms pass 0)

void operator delete(void *pointer) noexcept {
    conjure::core::memory::tracked_free(pointer, 0);
}

void operator delete[](void *pointer) noexcept {
    conjure::core::memory::tracked_free(pointer, 0);
}

void operator delete(void *pointer, std::size_t size) noexcept {
    conjure::core::memory::tracked_free(pointer, size);
}

void operator delete[](void *pointer, std::size_t size) noexcept {
    conjure::core::memory::tracked_free(pointer, size);
}

void operator delete(void *pointer, std::align_val_t) noexcept {
    conjure::core::memory::tracked_free(pointer, 0);
}

void operator delete[](void *pointer, std::align_val_t) noexcept {
    conjure::core::memory::tracked_free(pointer, 0);
}

void operator delete(void *pointer, std::size_t size, std::align_val_t) noexcept {
    conjure::core::memory::tracked_free(pointer, size);
}

void operator delete[](void *pointer, std::size_t size, std::align_val_t) noexcept {
    conjure::core::memory::tracked_free(pointer, size);
}

void operator delete(void *pointer, const std::nothrow_t &) noexcept {
    conjure::core::memory::tracked_free(pointer, 0);
}

void operator delete[](void *pointer, const std::nothrow_t &) noexcept {
    conjure::core::memory::tracked_free(pointer, 0);
}

void operator delete(void *pointer, std::align_val_t, const std::nothrow_t &) noexcept {
    conjure::core::memory::tracked_free(pointer, 0);
}

void operator delete[](void *pointer, std::align_val_t, const std::nothrow_t &) noexcept {
    conjure::core::memory::tracked_free(pointer, 0);
}
