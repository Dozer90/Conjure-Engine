#include "conjure/testing/assert_capture.h"

#include "conjure/testing/log_capture.h"
#include "conjure/testing/test_environment.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>

using namespace conjure::testing;

/*===========================================================================*/
/*===========================================================================*/
// Scope tracking

namespace {
    // The innermost scope of the calling thread, and the chain of every live scope in the process
    // (newest first, linked through mPreviousGlobalScope) for asserts raised on helper threads that
    // have no scope of their own. The handler reads the head without the lock; the chain itself is
    // only modified under sGlobalScopeMutex, so scopes created and destroyed on different threads
    // with interleaved lifetimes unlink themselves from the middle instead of leaving a dangling head.
    thread_local expect_assert *tThreadScope = nullptr;
    std::atomic<expect_assert *> sGlobalScope = nullptr;
    std::mutex sGlobalScopeMutex = {};

    void flush_everything() {
        std::cout.flush();
        std::fflush(stdout);
        std::fflush(stderr);
        conjure::core::log::flush();
    }
}


/*===========================================================================*/
/*===========================================================================*/
// expect_assert

expect_assert::expect_assert() :
    mPreviousHandler(conjure::core::set_assert_handler(&detail::testing_assert_handler)),
    mPreviousThreadScope(tThreadScope) {
    tThreadScope = this;

    const std::scoped_lock lock(sGlobalScopeMutex);
    mPreviousGlobalScope = sGlobalScope.load();
    sGlobalScope.store(this);
}

expect_assert::~expect_assert() {
    tThreadScope = mPreviousThreadScope;
    unlink_global_scope();
    conjure::core::set_assert_handler(mPreviousHandler);
}

conjure::u32 expect_assert::count() const noexcept {
    const std::scoped_lock lock(mMutex);
    return mCount;
}

std::span<const asserted_failure> expect_assert::failures() const noexcept {
    const std::scoped_lock lock(mMutex);
    return std::span<const asserted_failure>(mFailures.data(), std::min(mCount, CAPACITY));
}

std::string expect_assert::last_message() const {
    const std::scoped_lock lock(mMutex);
    if (mCount == 0) {
        return {};
    }
    return std::string(mFailures[(mCount - 1) % CAPACITY].message_view());
}

void expect_assert::reset() noexcept {
    const std::scoped_lock lock(mMutex);
    mCount = 0;
}

void expect_assert::record(const conjure::core::assert_info &info) noexcept {
    const std::scoped_lock lock(mMutex);
    asserted_failure &failure = mFailures[mCount % CAPACITY];
    failure.expression = info.expression;
    failure.file = info.file;
    failure.line = static_cast<u32>(std::max(info.line, 0));

    const std::string_view text = info.message.empty() ? std::string_view(info.expression) : info.message;
    const usize length = std::min(text.size(), asserted_failure::MAX_MESSAGE_LENGTH);
    std::memcpy(failure.message.data(), text.data(), length);
    failure.message[length] = '\0';

    ++mCount;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

void expect_assert::unlink_global_scope() noexcept {
    const std::scoped_lock lock(sGlobalScopeMutex);

    // The common case: this scope is the newest one.
    if (sGlobalScope.load() == this) {
        sGlobalScope.store(mPreviousGlobalScope);
        return;
    }

    // Otherwise a scope created later on another thread is still alive: splice this one out of the
    // chain so that scope's own destruction never restores a pointer to a dead object.
    for (expect_assert *scope = sGlobalScope.load(); scope != nullptr; scope = scope->mPreviousGlobalScope) {
        if (scope->mPreviousGlobalScope == this) {
            scope->mPreviousGlobalScope = mPreviousGlobalScope;
            return;
        }
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Handler

expect_assert *conjure::testing::detail::active_assert_scope() noexcept {
    if (tThreadScope != nullptr) {
        return tThreadScope;
    }
    return sGlobalScope.load();
}

conjure::core::assert_action conjure::testing::detail::testing_assert_handler(const conjure::core::assert_info &info) {
    expect_assert *scope = active_assert_scope();
    if (scope != nullptr) {
        scope->record(info);
        return conjure::core::assert_action::CONTINUE;
    }

    // Unexpected: fail the case where the assert happened. doctest's assertion machinery is
    // thread-safe, so this is valid from a worker as well.
    ADD_FAIL_CHECK_AT(
      info.file,
      info.line,
      "Unexpected assertion failure: " << info.expression << " - " << info.message
    );

    // Continuing past a violated precondition is undefined behaviour in the code under test, so the
    // process ends like the engine would (08_testing 3.1). A developer may opt in to continuing on
    // the test thread to see every failure of a direct run; off the test thread no case could
    // continue anyway.
    if (environment().continueOnAssert && is_test_thread()) {
        return conjure::core::assert_action::CONTINUE;
    }

    // The test_case_end listener will not run, so the captured log is printed here; the reporter's
    // buffered output is flushed first because abort() skips every stream destructor.
    flush_everything();
    std::fputs("--- log captured before the assertion ---\n", stderr);
    for (const std::string &line : global_log_ring_lines()) {
        std::fputs(line.c_str(), stderr);
        std::fputc('\n', stderr);
    }
    std::fflush(stderr);
    return conjure::core::assert_action::ABORT;
}
