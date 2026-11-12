#include "conjure/editor/console_buffer.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <utility>

namespace conjure::editor {

    /// <summary>
    /// The log sink that feeds the ring. Holds the buffer by raw pointer under its own mutex: the logger calls
    /// write() from any thread off a snapshot that keeps the sink alive, and the buffer's destructor detaches
    /// the pointer under the same mutex, so a write in flight finishes before the ring goes away and a later
    /// one finds no owner.
    /// </summary>
    class console_buffer::sink final : public core::log_sink {
        private:
            std::mutex mMutex = {};
            console_buffer *mOwner = nullptr;               // guarded by mMutex

        public:
            explicit sink(console_buffer &owner) noexcept :
                mOwner(&owner) {}

            ~sink() override = default;

        public:
            void write(const core::log_message &message) override {
                const std::lock_guard<std::mutex> lock(mMutex);
                if (mOwner != nullptr) {
                    mOwner->push(message.level, message.category, message.text);
                }
            }

            /// <summary>
            /// Forgets the owner; waits for a write in progress first.
            /// </summary>
            void detach() {
                const std::lock_guard<std::mutex> lock(mMutex);
                mOwner = nullptr;
            }
    };

    console_buffer::console_buffer(usize entryCapacity, usize textCapacity) {
        mEntries.resize(std::max<usize>(entryCapacity, 1));
        mText.resize(std::max<usize>(textCapacity, 256));
    }

    console_buffer::~console_buffer() {
        remove_sink();
    }

    void console_buffer::push(core::log_level level, std::string_view category, std::string_view text) {
        const std::lock_guard<std::mutex> lock(mMutex);
        push_locked(level, category, text);
    }

    void console_buffer::install_sink() {
        if (mSink != nullptr) {
            return;
        }
        mSink = std::make_shared<sink>(*this);
        core::log::add_sink(mSink);
    }

    void console_buffer::remove_sink() {
        if (mSink == nullptr) {
            return;
        }
        core::log::remove_sink(mSink);
        mSink->detach();
        mSink.reset();
    }

    std::vector<console_record> console_buffer::read(const console_query &query) const {
        std::vector<console_record> records = {};
        const std::lock_guard<std::mutex> lock(mMutex);
        const usize capacity = mEntries.size();
        const usize first = (mEntryHead + capacity - mEntryCount) % capacity;
        // Sequence numbers in the ring are contiguous (push numbers every entry, eviction drops only the oldest,
        // clear() empties), so the entries up to sinceSeq are skipped without walking them under the lock the
        // log sink's push needs.
        const u64 firstSeq = mNextSeq - mEntryCount;
        usize skip = 0;
        if (query.sinceSeq >= firstSeq) {
            skip = static_cast<usize>(std::min<u64>(query.sinceSeq - firstSeq + 1, mEntryCount));
        }
        // Copies one entry into the result when it passes the filter.
        const auto take = [this, &query, &records](const entry &item) {
            if (item.seq <= query.sinceSeq || item.level < query.minLevel) {
                return;
            }
            const std::string_view category = text_at(item.textOffset, item.categoryLength);
            if (!query.category.empty() && category != query.category) {
                return;
            }
            console_record record = {};
            record.seq = item.seq;
            record.frame = item.frame;
            record.level = item.level;
            record.category = std::string(category);
            record.text = std::string(text_at(item.textOffset + item.categoryLength, item.textLength));
            records.push_back(std::move(record));
        };
        if (!query.newest) {
            for (usize index = skip; index < mEntryCount && records.size() < query.limit; ++index) {
                take(mEntries[(first + index) % capacity]);
            }
            return records;
        }
        // Newest first from the head back to sinceSeq, then reversed so the caller still reads oldest first.
        for (usize index = mEntryCount; index > skip && records.size() < query.limit; --index) {
            take(mEntries[(first + index - 1) % capacity]);
        }
        std::reverse(records.begin(), records.end());
        return records;
    }

    u64 console_buffer::last_seq() const {
        const std::lock_guard<std::mutex> lock(mMutex);
        return mNextSeq - 1;
    }

    usize console_buffer::size() const {
        const std::lock_guard<std::mutex> lock(mMutex);
        return mEntryCount;
    }

    void console_buffer::clear() {
        const std::lock_guard<std::mutex> lock(mMutex);
        mEntryCount = 0;
        mEntryHead = 0;
        mTextHead = 0;
    }

    void console_buffer::push_locked(core::log_level level, std::string_view category, std::string_view text) {
        const usize capacity = mText.size();
        // Each record is category + text stored contiguously; a record never exceeds half the ring so
        // overwriting the oldest records (whose entries are dropped below) always frees enough room.
        const usize maxRecord = capacity / 2;
        if (category.size() > maxRecord / 4) {
            category = category.substr(0, maxRecord / 4);
        }
        if (category.size() + text.size() > maxRecord) {
            text = text.substr(0, maxRecord - category.size());
            ++mDropped;
        }
        const usize total = category.size() + text.size();
        // Every record advances the head by at least one byte, so no two held records share an offset and the
        // offset alone tells which records a write covers (an empty record would otherwise sit on the start of
        // the next one).
        const usize footprint = std::max<usize>(total, 1);
        const usize entryCapacity = mEntries.size();
        const auto oldestIndex = [this, entryCapacity]() {
            return (mEntryHead + entryCapacity - mEntryCount) % entryCapacity;
        };
        if (mTextHead + footprint > capacity) {
            // Wrap. A record still held at or past the head is a leftover of the lap before the one that just
            // ended, older than every record of that lap, which the new lap starts overwriting at offset 0; the
            // oldest go first, so the leftovers are dropped before the overlap test below.
            const usize lapEnd = mTextHead;
            while (mEntryCount > 0 && mEntries[oldestIndex()].textOffset >= lapEnd) {
                --mEntryCount;
            }
            mTextHead = 0;
        }
        const usize offset = mTextHead;
        if (!category.empty()) {
            std::memcpy(mText.data() + offset, category.data(), category.size());
        }
        if (!text.empty()) {
            std::memcpy(mText.data() + offset + category.size(), text.data(), text.size());
        }
        mTextHead = offset + footprint;

        // Drop the entries whose text the new record overwrote: the oldest are the previous lap's records at or
        // after the write position, in offset order; an oldest record before it belongs to the current lap and,
        // like every newer one, was not touched.
        while (mEntryCount > 0) {
            const entry &oldest = mEntries[oldestIndex()];
            if (oldest.textOffset < offset || oldest.textOffset >= mTextHead) {
                break;
            }
            --mEntryCount;
        }

        entry added = {};
        added.seq = mNextSeq;
        ++mNextSeq;
        added.frame = mFrame.load(std::memory_order_relaxed);
        added.textOffset = offset;
        added.textLength = static_cast<u32>(text.size());
        added.categoryLength = static_cast<u32>(category.size());
        added.level = level;
        mEntries[mEntryHead] = added;
        mEntryHead = (mEntryHead + 1) % entryCapacity;
        if (mEntryCount < entryCapacity) {
            ++mEntryCount;
        }
    }

    std::string_view console_buffer::text_at(usize offset, u32 length) const noexcept {
        if (offset + length > mText.size()) {
            return {};
        }
        return std::string_view(mText.data() + offset, length);
    }
}
