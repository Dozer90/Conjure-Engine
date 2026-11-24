#include "conjure/editor/undo_stack.h"

#include "conjure/editor/editor_error.h"
#include "conjure/editor/log_category.h"

#include <algorithm>
#include <format>
#include <utility>

namespace conjure::editor {

    /*===========================================================================*/
    /*===========================================================================*/
    // undo_entry

    std::string_view undo_entry::single_command_name() const noexcept {
        if (redo.size() != 1) {
            return "";
        }
        return redo.front().name;
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // undo_stack

    undo_stack::undo_stack() = default;
    undo_stack::~undo_stack() = default;

    usize undo_stack::measure(const undo_entry &entry) {
        usize bytes = entry.label.size();
        for (const command_inverse &inverse : entry.undo) {
            bytes += inverse.name.size() + inverse.args.to_string().size();
        }
        for (const command_request &replay : entry.redo) {
            bytes += replay.name.size() + replay.args.to_string().size();
        }
        return bytes;
    }

    void undo_stack::record(
      command_request replay,
      command_inverse inverse,
      std::string_view label,
      u32 clientId,
      u64 coalesceKey
    ) {
        if (mFrozen) {
            return;
        }
        replay.clientId = clientId;
        replay.coalesceKey = coalesceKey;

        if (undo_entry *transaction = find_transaction(clientId); transaction != nullptr) {
            // Coalescing inside the transaction: a drag that keeps re-issuing the same command with the same
            // key replaces the last replay and keeps the first inverse, so a gizmo session is one step, not
            // one per frame.
            if (
              coalesceKey != 0 && !transaction->redo.empty() && transaction->redo.back().coalesceKey == coalesceKey &&
              transaction->redo.back().name == replay.name
            ) {
                transaction->redo.back() = std::move(replay);
                return;
            }
            transaction->undo.push_back(std::move(inverse));
            transaction->redo.push_back(std::move(replay));
            if (transaction->label.empty()) {
                transaction->label = std::string(label);
            }
            return;
        }

        // Coalescing: the top undoable entry has the same non-zero key and the same single command.
        if (coalesceKey != 0 && mCursor > 0 && mCursor == mEntries.size()) {
            undo_entry &top = mEntries[mCursor - 1];
            if (
              top.coalesceKey == coalesceKey && top.clientId == clientId && top.single_command_name() == replay.name
            ) {
                mBytes -= top.byteSize;
                top.redo.front() = std::move(replay);
                top.byteSize = measure(top);
                mBytes += top.byteSize;
                return;
            }
        }

        undo_entry entry = {};
        entry.label = std::string(label);
        entry.undo.push_back(std::move(inverse));
        entry.redo.push_back(std::move(replay));
        entry.coalesceKey = coalesceKey;
        entry.clientId = clientId;
        push_entry(std::move(entry));
    }

    void undo_stack::push(undo_entry entry) {
        if (mFrozen || (entry.undo.empty() && entry.redo.empty())) {
            return;
        }
        push_entry(std::move(entry));
    }

    core::result<void> undo_stack::begin_transaction(std::string_view label, u32 clientId) {
        if (find_transaction(clientId) != nullptr) {
            return core::make_error(
              protocol_error_code::INVALID_REQUEST,
              std::format("client {} already has an open transaction", clientId)
            );
        }
        undo_entry entry = {};
        entry.label = std::string(label);
        entry.clientId = clientId;
        mTransactions.emplace_back(clientId, std::move(entry));
        return core::result<void>::success();
    }

    core::result<void> undo_stack::end_transaction(u32 clientId) {
        const auto position = std::find_if(mTransactions.begin(), mTransactions.end(), [clientId](const auto &pair) {
            return pair.first == clientId;
        });
        if (position == mTransactions.end()) {
            return core::make_error(
              protocol_error_code::INVALID_REQUEST,
              std::format("client {} has no open transaction", clientId)
            );
        }
        undo_entry entry = std::move(position->second);
        mTransactions.erase(position);
        if (!entry.undo.empty() || !entry.redo.empty()) {
            push_entry(std::move(entry));
        }
        return core::result<void>::success();
    }

    bool undo_stack::has_transaction(u32 clientId) const noexcept {
        for (const auto &pair : mTransactions) {
            if (pair.first == clientId) {
                return true;
            }
        }
        return false;
    }

    std::optional<undo_entry> undo_stack::pop_for_undo() {
        if (mCursor == 0) {
            return std::nullopt;
        }
        --mCursor;
        return mEntries[mCursor];
    }

    std::optional<undo_entry> undo_stack::pop_for_redo() {
        if (mCursor >= mEntries.size()) {
            return std::nullopt;
        }
        const undo_entry entry = mEntries[mCursor];
        ++mCursor;
        return entry;
    }

    bool undo_stack::discard_at_cursor(bool afterUndo) {
        usize index = 0;
        if (afterUndo) {
            if (mCursor >= mEntries.size()) {
                return false;
            }
            index = mCursor;
        } else {
            if (mCursor == 0) {
                return false;
            }
            index = mCursor - 1;
            --mCursor;
        }
        mBytes -= mEntries[index].byteSize;
        mEntries.erase(mEntries.begin() + static_cast<isize>(index));
        return true;
    }

    void undo_stack::set_byte_budget(usize bytes) noexcept {
        mByteBudget = bytes;
        evict();
    }

    void undo_stack::set_max_depth(usize depth) noexcept {
        mMaxDepth = std::max<usize>(depth, 1);
        evict();
    }

    std::vector<const undo_entry *> undo_stack::entries() const {
        std::vector<const undo_entry *> list = {};
        list.reserve(mEntries.size());
        for (const undo_entry &entry : mEntries) {
            list.push_back(&entry);
        }
        return list;
    }

    void undo_stack::clear() {
        mEntries.clear();
        mTransactions.clear();
        mCursor = 0;
        mBytes = 0;
    }

    void undo_stack::push_entry(undo_entry entry) {
        // A new record discards the redo tail.
        while (mEntries.size() > mCursor) {
            mBytes -= mEntries.back().byteSize;
            mEntries.pop_back();
        }
        entry.byteSize = measure(entry);
        mBytes += entry.byteSize;
        mEntries.push_back(std::move(entry));
        mCursor = mEntries.size();
        evict();
    }

    void undo_stack::evict() {
        while (!mEntries.empty() && (mEntries.size() > mMaxDepth || (mBytes > mByteBudget && mEntries.size() > 1))) {
            mBytes -= mEntries.front().byteSize;
            mEntries.pop_front();
            if (mCursor > 0) {
                --mCursor;
            }
        }
        if (!mEntries.empty() && mBytes > mByteBudget) {
            // A single entry above the budget stays: evicting it would lose the only undo step.
            CONJURE_LOG_DEBUG(
              LOG_EDITOR,
              "undo stack: one entry of {} bytes exceeds the budget of {}",
              mBytes,
              mByteBudget
            );
        }
    }

    undo_entry *undo_stack::find_transaction(u32 clientId) noexcept {
        for (auto &pair : mTransactions) {
            if (pair.first == clientId) {
                return &pair.second;
            }
        }
        return nullptr;
    }

    /*===========================================================================*/
    /*===========================================================================*/
    // transaction_scope

    transaction_scope::transaction_scope(undo_stack &stack, std::string_view label, u32 clientId) :
        mStack(&stack),
        mClientId(clientId) {
        mActive = stack.begin_transaction(label, clientId).has_value();
    }

    transaction_scope::~transaction_scope() {
        end();
    }

    void transaction_scope::end() {
        if (!mActive) {
            return;
        }
        mActive = false;
        const core::result<void> ended = mStack->end_transaction(mClientId);
        if (!ended.has_value()) {
            CONJURE_LOG_WARN(LOG_EDITOR, "transaction_scope: {}", ended.get_error().message());
        }
    }
}
