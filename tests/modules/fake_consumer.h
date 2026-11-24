#pragma once

#include "conjure/modules/module_consumer.h"
#include "conjure/modules/module_context.h"
#include "conjure/modules/module_error.h"
#include "conjure/modules/module_registry.h"
#include "conjure/modules/module_types.h"

#include "conjure/core/result.h"
#include "conjure/core/string_id.h"
#include "conjure/core/types.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// A recording module_consumer for the manager tests. Every call appends "<call>:<consumer>:<module>"
// to a log the test owns (several consumers may share one log to check ordering). The outcomes of
// validate() and can_detach() and the live-use count are plain fields the test sets.

namespace conjure::modules::testing {

    class fake_consumer final : public module_consumer {
        private:
            std::string mTag = {};
            core::string_id mName = {};
            core::string_id mSnapshotKey = {};      // detach() stores under it, restore() reads it back
            std::vector<std::string> *mLog = nullptr;
            std::vector<std::string> mOwnLog = {};
            const module_registry *mRegistry = nullptr;
            std::vector<module_id> mAttached = {};

        public:
            core::result<void> validateResult = {};
            core::result<void> canDetachResult = {};
            u32 liveUses = 0;                       // reported while attached under REFUSE_IF_IN_USE
            u32 liveUsesAfterDetach = 0;            // reported after detach; non-zero faults the module
            bool participatesInManagedReload = false;
            std::string snapshotPayload = {};       // stored on detach when a reload_context is given
            std::string restoredPayload = {};       // what the last restore() fetched
            u32 restoreCount = 0;

        public:
            explicit fake_consumer(std::string tag, std::vector<std::string> *sharedLog = nullptr) :
                mTag(std::move(tag)),
                mName(core::string_id::intern(mTag)),
                mSnapshotKey(core::string_id::intern("fake.snapshot." + mTag)),
                mLog(sharedLog) {}

            ~fake_consumer() override = default;

        public:
            /// <summary>
            /// Lets the log name modules instead of printing ids.
            /// </summary>
            void bind_registry(const module_registry &registry) noexcept {
                mRegistry = &registry;
            }

            /// <summary>
            /// The reload_context key this consumer's snapshot travels under.
            /// </summary>
            [[nodiscard]] core::string_id snapshot_key() const noexcept {
                return mSnapshotKey;
            }

            [[nodiscard]] const std::vector<std::string> &calls() const noexcept {
                return mLog != nullptr ? *mLog : mOwnLog;
            }

            [[nodiscard]] std::span<const module_id> attached() const noexcept {
                return mAttached;
            }

            [[nodiscard]] bool is_attached(module_id id) const noexcept {
                return std::find(mAttached.begin(), mAttached.end(), id) != mAttached.end();
            }

            void clear_calls() {
                if (mLog != nullptr) {
                    mLog->clear();
                }
                mOwnLog.clear();
            }

            [[nodiscard]] core::string_id name() const noexcept override {
                return mName;
            }

            [[nodiscard]] core::result<void> validate(
              module_id id,
              const module_registration_set &,
              const module_registry &registry
            ) override {
                mRegistry = &registry;
                record("validate", id);
                return validateResult;
            }

            void attach(module_id id, module_context &context, const module_registration_set &) override {
                record("attach", id);
                mAttached.push_back(id);
                CONJURE_UNUSED(context);
            }

            [[nodiscard]] core::result<void> can_detach(module_id id, unload_policy policy) override {
                record(policy == unload_policy::REFUSE_IF_IN_USE ? "can_detach(refuse)" : "can_detach(remove)", id);
                if (!canDetachResult.has_value()) {
                    return canDetachResult;
                }
                if (policy == unload_policy::REFUSE_IF_IN_USE && liveUses > 0) {
                    return core::make_error(
                      module_error::IN_USE,
                      "consumer '{}' has {} live use(s) of module {}",
                      mTag,
                      liveUses,
                      module_name(id)
                    );
                }
                return {};
            }

            void detach(module_id id, reload_context *reload) override {
                record(reload != nullptr ? "detach(reload)" : "detach", id);
                mAttached.erase(std::remove(mAttached.begin(), mAttached.end(), id), mAttached.end());
                if (reload != nullptr) {
                    const std::span<const std::byte> bytes(
                      reinterpret_cast<const std::byte *>(snapshotPayload.data()),
                      snapshotPayload.size()
                    );
                    reload->store(mSnapshotKey, bytes);
                }
            }

            void restore(module_id id, const reload_context &reload) override {
                record("restore", id);
                const std::span<const std::byte> bytes = reload.fetch(mSnapshotKey);
                restoredPayload.assign(reinterpret_cast<const char *>(bytes.data()), bytes.size());
                ++restoreCount;
            }

            [[nodiscard]] u32 live_use_count(module_id id) const override {
                return is_attached(id) ? liveUses : liveUsesAfterDetach;
            }

            [[nodiscard]] bool participates_in_managed_reload() const noexcept override {
                return participatesInManagedReload;
            }

        private:
            [[nodiscard]] std::string module_name(module_id id) const {
                if (mRegistry != nullptr && mRegistry->is_valid(id)) {
                    return mRegistry->manifest(id).name;
                }
                return to_string(id);
            }

            void record(std::string_view call, module_id id) {
                std::string entry = std::format("{}:{}:{}", call, mTag, module_name(id));
                if (mLog != nullptr) {
                    mLog->push_back(std::move(entry));
                } else {
                    mOwnLog.push_back(std::move(entry));
                }
            }
    };
}
