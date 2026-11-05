#include "conjure/core/event.h"

using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// subscription_target

detail::subscription_target::subscription_target() {}

detail::subscription_target::~subscription_target() {}


/*===========================================================================*/
/*===========================================================================*/
// event_subscription

event_subscription::event_subscription() noexcept {}

event_subscription::event_subscription(std::weak_ptr<detail::subscription_target> target, u64 id) noexcept :
    mTarget(std::move(target)),
    mId(id) {}

event_subscription::~event_subscription() {
    release();
}

event_subscription::event_subscription(event_subscription &&other) noexcept :
    mTarget(std::move(other.mTarget)),
    mId(other.mId) {
    other.mTarget.reset();
    other.mId = 0;
}

event_subscription &event_subscription::operator=(event_subscription &&other) noexcept {
    if (this == &other) {
        return *this;
    }

    // The subscription being replaced ends now, exactly like going out of scope would end it.
    release();
    mTarget = std::move(other.mTarget);
    mId = other.mId;
    other.mTarget.reset();
    other.mId = 0;
    return *this;
}

void event_subscription::release() noexcept {
    if (mId != 0) {
        const std::shared_ptr<detail::subscription_target> target = mTarget.lock();
        if (target != nullptr) {
            target->unsubscribe(mId);
        }
    }
    mTarget.reset();
    mId = 0;
}

bool event_subscription::is_active() const noexcept {
    if (mId == 0) {
        return false;
    }

    const std::shared_ptr<detail::subscription_target> target = mTarget.lock();
    if (target == nullptr) {
        return false;
    }
    return target->is_subscribed(mId);
}
