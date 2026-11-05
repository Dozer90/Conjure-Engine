#include "conjure/core/jobs/dedicated_thread.h"

#include "core/jobs/thread_name.h"

#include "conjure/core/assert.h"

#include <utility>

using namespace conjure::core::jobs;

/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

dedicated_thread::dedicated_thread(std::string name, std::function<void(std::stop_token)> body) :
    mName(std::move(name)) {
    CONJURE_ASSERT(body != nullptr, "dedicated_thread '{}': empty body", mName);
    if (!body) {
        return;
    }

    // The name is copied into the thread so it outlives a moved-from dedicated_thread.
    mThread = std::jthread([threadName = mName, threadBody = std::move(body)](std::stop_token token) {
        detail::set_current_thread_name(threadName);
        threadBody(token);
    });
}

dedicated_thread::~dedicated_thread() {
    if (!mThread.joinable()) {
        return;
    }
    mThread.request_stop();
    mThread.join();
}

/*===========================================================================*/
/*===========================================================================*/
// Public methods

void dedicated_thread::request_stop() noexcept {
    static_cast<void>(mThread.request_stop());
}

bool dedicated_thread::stop_requested() const noexcept {
    return mThread.get_stop_token().stop_requested();
}

bool dedicated_thread::joinable() const noexcept {
    return mThread.joinable();
}

void dedicated_thread::join() {
    if (!mThread.joinable()) {
        return;
    }
    mThread.join();
}

const std::string &dedicated_thread::name() const noexcept {
    return mName;
}

std::thread::id dedicated_thread::get_id() const noexcept {
    return mThread.get_id();
}
