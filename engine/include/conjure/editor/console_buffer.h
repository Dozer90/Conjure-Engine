#pragma once

#include "conjure/core/api.h"
#include "conjure/core/log.h"
#include "conjure/core/types.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// The console ring (docs/design/04_editor.md 2.6): 65,536 entries and 16 MiB of text fed by a log sink that
// may be called from any thread, read on the main thread by the console panel, the `log` event and the
// console.read command. The mutex is never held across an allocation: the text ring is preallocated.

namespace conjure::editor {

    /// <summary>
    /// One captured line as readers see it (copies; the ring may overwrite the original meanwhile).
    /// </summary>
    struct console_record {
        u64 seq = 0;
        u64 frame = 0;
        core::log_level level = core::log_level::INFO;
        std::string category = "";
        std::string text = "";
    };

    /// <summary>
    /// A filter for read().
    /// </summary>
    struct console_query {
        u64 sinceSeq = 0;                                   // entries with seq > sinceSeq
        core::log_level minLevel = core::log_level::TRACE;
        std::string category = "";                          // "" = every category
        usize limit = 1000;
    };

    class CONJURE_API console_buffer {
        public:
            static constexpr usize DEFAULT_ENTRY_CAPACITY = 65536;
            static constexpr usize DEFAULT_TEXT_CAPACITY = 16 * MIB;

        private:
            struct entry {
                u64 seq = 0;
                u64 frame = 0;
                usize textOffset = 0;
                u32 textLength = 0;
                u32 categoryLength = 0;
                core::log_level level = core::log_level::INFO;
            };

            class sink;

            mutable std::mutex mMutex = {};
            std::vector<entry> mEntries = {};               // ring, capacity fixed
            std::vector<char> mText = {};                   // ring of category + text bytes
            usize mEntryHead = 0;                           // index the next entry is written to
            usize mEntryCount = 0;
            usize mTextHead = 0;
            u64 mNextSeq = 1;
            u64 mDropped = 0;                               // entries whose text no longer fits (truncated)
            std::atomic<u64> mFrame = 0;
            std::shared_ptr<sink> mSink = nullptr;

        public:
            explicit console_buffer(
              usize entryCapacity = DEFAULT_ENTRY_CAPACITY,
              usize textCapacity = DEFAULT_TEXT_CAPACITY
            );

            /// <summary>
            /// Removes the sink from the logger when install_sink() added it.
            /// </summary>
            ~console_buffer();

            console_buffer(const console_buffer &) = delete;
            console_buffer &operator=(const console_buffer &) = delete;

        public:
            /// <summary>
            /// Appends one line. Thread-safe. Text longer than the text ring is truncated.
            /// </summary>
            void push(core::log_level level, std::string_view category, std::string_view text);

            /// <summary>
            /// Registers a core::log sink that forwards every message into push(). Idempotent.
            /// </summary>
            void install_sink();

            /// <summary>
            /// Removes the sink registered by install_sink().
            /// </summary>
            void remove_sink();

            /// <summary>
            /// The frame index stamped on new entries (set once per frame by the editor).
            /// </summary>
            void set_frame(u64 frame) noexcept {
                mFrame.store(frame, std::memory_order_relaxed);
            }

            /// <summary>
            /// Entries matching the query, oldest first, at most query.limit.
            /// </summary>
            [[nodiscard]] std::vector<console_record> read(const console_query &query) const;

            /// <summary>
            /// The most recent sequence number handed out (0 before the first push).
            /// </summary>
            [[nodiscard]] u64 last_seq() const;

            /// <summary>
            /// Entries currently in the ring.
            /// </summary>
            [[nodiscard]] usize size() const;

            [[nodiscard]] usize entry_capacity() const noexcept {
                return mEntries.size();
            }

            [[nodiscard]] usize text_capacity() const noexcept {
                return mText.size();
            }

            /// <summary>
            /// Forgets every entry; sequence numbers keep counting.
            /// </summary>
            void clear();

        private:
            void push_locked(core::log_level level, std::string_view category, std::string_view text);
            [[nodiscard]] std::string_view text_at(usize offset, u32 length) const noexcept;
    };
}
