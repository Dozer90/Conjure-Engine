# Sanitizer support. Applied to every Conjure target through conjure::sanitizers.
#
# CONJURE_ENABLE_ASAN / CONJURE_ENABLE_UBSAN are declared in the root CMakeLists.txt;
# CONJURE_ENABLE_TSAN lives here (08_testing.md 3.8). ThreadSanitizer is exclusive with
# AddressSanitizer and is a GCC/Clang feature: the nightly linux-clang-tsan preset turns it on.

option(CONJURE_ENABLE_TSAN "Enable ThreadSanitizer (GCC/Clang; exclusive with ASan)" OFF)

if(CONJURE_ENABLE_TSAN AND CONJURE_ENABLE_ASAN)
    message(FATAL_ERROR "CONJURE_ENABLE_TSAN and CONJURE_ENABLE_ASAN cannot be combined")
endif()
if(CONJURE_ENABLE_TSAN AND MSVC)
    message(FATAL_ERROR "CONJURE_ENABLE_TSAN requires GCC or Clang")
endif()

add_library(conjure_sanitizers INTERFACE)
add_library(conjure::sanitizers ALIAS conjure_sanitizers)

if(CONJURE_ENABLE_ASAN)
    if(MSVC)
        target_compile_options(conjure_sanitizers INTERFACE /fsanitize=address)
    else()
        target_compile_options(conjure_sanitizers INTERFACE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(conjure_sanitizers INTERFACE -fsanitize=address)
    endif()
endif()

if(CONJURE_ENABLE_UBSAN AND NOT MSVC)
    target_compile_options(conjure_sanitizers INTERFACE -fsanitize=undefined -fno-omit-frame-pointer)
    target_link_options(conjure_sanitizers INTERFACE -fsanitize=undefined)
endif()

if(CONJURE_ENABLE_TSAN)
    target_compile_options(conjure_sanitizers INTERFACE -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(conjure_sanitizers INTERFACE -fsanitize=thread)
endif()
