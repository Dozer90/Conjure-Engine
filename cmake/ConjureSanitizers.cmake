# Sanitizer support. Applied to every Conjure target through conjure::sanitizers.

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
