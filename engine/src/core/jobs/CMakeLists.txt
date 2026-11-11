# core/jobs - worker pool, job handles, parallel_for, the main-thread queue and dedicated threads.
# The only place in the engine that creates std::thread / std::jthread objects.

# dedicated_thread's public signature takes a std::stop_token (docs/design/10_core.md 2.7), which
# needs libstdc++ 10+, MSVC 19.28+ or libc++ 18+ (libc++ 16 has no <stop_token>; 17 hides it behind
# -fexperimental-library). Fail at configure time with a clear message instead of in the first
# translation unit that includes the umbrella header.
include(CheckCXXSourceCompiles)
include(CMakePushCheckState)
find_package(Threads REQUIRED)
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_LIBRARIES Threads::Threads)
check_cxx_source_compiles("
#include <stop_token>
#include <thread>
int main() {
    std::jthread thread([](std::stop_token token) { static_cast<void>(token.stop_requested()); });
    thread.request_stop();
    return 0;
}
" CONJURE_HAVE_STD_JTHREAD)
cmake_pop_check_state()
if(NOT CONJURE_HAVE_STD_JTHREAD)
    message(FATAL_ERROR
        "core/jobs: the C++ standard library has no std::jthread / <stop_token>, which the public "
        "dedicated_thread API requires. Use libstdc++ 10+, MSVC 19.28+ or libc++ 18+ (Xcode 16+); "
        "libc++ 17 needs -fexperimental-library."
    )
endif()

conjure_add_core_area(jobs
    PUBLIC_HEADERS
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/jobs/dedicated_thread.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/jobs/job_handle.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/jobs/job_system.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/jobs/jobs.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/jobs/main_thread.h
        ${PROJECT_SOURCE_DIR}/engine/include/conjure/core/jobs/parallel_for.h
    SOURCES
        cpu_pause.h
        dedicated_thread.cpp
        job_handle.cpp
        job_system.cpp
        main_thread.cpp
        thread_name.cpp
        thread_name.h
    PUBLIC_DEPS
        conjure::core_foundation
)
