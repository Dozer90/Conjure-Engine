#pragma once

#include "conjure/editor/editor_command.h"

#include "conjure/core/api.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <deque>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Undo as inverse commands (docs/design/04_editor.md D3, 2.2). An entry holds the inverses (applied in reverse
// by history.undo) and the replays (applied forwards by history.redo); the dispatcher records one entry per
// mutating command, or appends to the client's open transaction / the running batch. Depth 1024, 256 MiB of
// serialised arguments (oldest evicted first), coalescing of drag-style edits, frozen during play.

namespace conjure::editor {

    struct CONJURE_API undo_entry {
        std::string label = "";
        std::vector<command_inverse> undo = {};             // applied in reverse order
        std::vector<command_request> redo = {};             // replays, applied forwards
        u64 coalesceKey = 0;
        u32 clientId = 0;
        usize byteSize = 0;                                 // serialised size of undo + redo arguments

        /// <summary>
        /// The name of the only redo command, or "" when the entry holds several (coalescing needs one).
        /// </summary>
        [[nodiscard]] std::string_view single_command_name() const noexcept;
    };

    class CONJURE_API undo_stack {
        public:
            static constexpr usize DEFAULT_MAX_DEPTH = 1024;
            static constexpr usize DEFAULT_BYTE_BUDGET = 256 * MIB;

        private:
            std::deque<undo_entry> mEntries = {};
            std::vector<std::pair<u32, undo_entry>> mTransactions = {};    // one open transaction per client
            usize mCursor = 0;                              // entries below the cursor can be undone
            usize mMaxDepth = DEFAULT_MAX_DEPTH;
            usize mByteBudget = DEFAULT_BYTE_BUDGET;
            usize mBytes = 0;
            bool mFrozen = false;

        public:
            undo_stack();
            ~undo_stack();

            undo_stack(const undo_stack &) = delete;
            undo_stack &operator=(const undo_stack &) = delete;

        public:
            /// <summary>
            /// Records one mutating command: appended to the client's open transaction when there is one
            /// (coalesced with the transaction's last record when the key and command match), coalesced into
            /// the top entry when the keys match (same non-zero key, same single command name: the new replay
            /// replaces the old, the first inverse is kept), otherwise pushed as a new entry after discarding
            /// the redo tail. Nothing is recorded while frozen.
            /// </summary>
            void record(
              command_request replay,
              command_inverse inverse,
              std::string_view label,
              u32 clientId,
              u64 coalesceKey
            );

            /// <summary>
            /// Pushes a complete entry (a batch). Empty entries are ignored.
            /// </summary>
            void push(undo_entry entry);

            /// <summary>
            /// Opens a transaction for the client. INVALID_REQUEST when one is already open.
            /// </summary>
            [[nodiscard]] core::result<void> begin_transaction(std::string_view label, u32 clientId);

            /// <summary>
            /// Closes the client's transaction and pushes it when it recorded anything. INVALID_REQUEST
            /// when none is open.
            /// </summary>
            [[nodiscard]] core::result<void> end_transaction(u32 clientId);

            /// <summary>
            /// True while the client has an open transaction.
            /// </summary>
            [[nodiscard]] bool has_transaction(u32 clientId) const noexcept;

            /// <summary>
            /// True while any client has an open transaction (history.undo, redo and clear are refused then:
            /// ending the transaction would push past the cursor and drop the redo tail).
            /// </summary>
            [[nodiscard]] bool has_open_transaction() const noexcept {
                return !mTransactions.empty();
            }

            /// <summary>
            /// The entry to undo next (the cursor moves down); nullopt when nothing can be undone.
            /// </summary>
            [[nodiscard]] std::optional<undo_entry> pop_for_undo();

            /// <summary>
            /// The entry to redo next (the cursor moves up); nullopt when nothing can be redone.
            /// </summary>
            [[nodiscard]] std::optional<undo_entry> pop_for_redo();

            /// <summary>
            /// Removes the entry a failed undo or redo just popped (the one at the cursor for a failed undo,
            /// the one below it for a failed redo). False when there is no such entry.
            /// </summary>
            bool discard_at_cursor(bool afterUndo);

            void set_frozen(bool frozen) noexcept {
                mFrozen = frozen;
            }

            [[nodiscard]] bool frozen() const noexcept {
                return mFrozen;
            }

            [[nodiscard]] bool can_undo() const noexcept {
                return mCursor > 0;
            }

            [[nodiscard]] bool can_redo() const noexcept {
                return mCursor < mEntries.size();
            }

            /// <summary>
            /// Changes the budget and evicts the oldest entries until it holds.
            /// </summary>
            void set_byte_budget(usize bytes) noexcept;

            /// <summary>
            /// Changes the depth cap and evicts the oldest entries until it holds (at least 1).
            /// </summary>
            void set_max_depth(usize depth) noexcept;

            [[nodiscard]] usize byte_budget() const noexcept {
                return mByteBudget;
            }

            [[nodiscard]] usize max_depth() const noexcept {
                return mMaxDepth;
            }

            /// <summary>
            /// Serialised bytes currently held.
            /// </summary>
            [[nodiscard]] usize byte_size() const noexcept {
                return mBytes;
            }

            /// <summary>
            /// Every entry, oldest first (entries below cursor() are undoable).
            /// </summary>
            [[nodiscard]] std::vector<const undo_entry *> entries() const;

            [[nodiscard]] usize cursor() const noexcept {
                return mCursor;
            }

            [[nodiscard]] usize size() const noexcept {
                return mEntries.size();
            }

            /// <summary>
            /// Forgets every entry and every open transaction.
            /// </summary>
            void clear();

            /// <summary>
            /// The serialised byte size an entry accounts for.
            /// </summary>
            [[nodiscard]] static usize measure(const undo_entry &entry);

        private:
            void push_entry(undo_entry entry);
            void evict();
            [[nodiscard]] undo_entry *find_transaction(u32 clientId) noexcept;
    };

    /// <summary>
    /// RAII transaction: begins in the constructor (inactive when a transaction is already open) and ends in
    /// the destructor.
    /// </summary>
    class CONJURE_API transaction_scope {
        private:
            undo_stack *mStack = nullptr;
            u32 mClientId = 0;
            bool mActive = false;

        public:
            transaction_scope(undo_stack &stack, std::string_view label, u32 clientId);
            ~transaction_scope();

            transaction_scope(const transaction_scope &) = delete;
            transaction_scope &operator=(const transaction_scope &) = delete;

        public:
            [[nodiscard]] bool active() const noexcept {
                return mActive;
            }

            /// <summary>
            /// Ends the transaction now; the destructor then does nothing.
            /// </summary>
            void end();
    };
}
