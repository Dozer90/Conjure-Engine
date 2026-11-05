#include "conjure/testing/log_capture.h"

#include "conjure/testing/doctest.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>

using namespace conjure::testing;

/*===========================================================================*/
/*===========================================================================*/
// log_capture

struct log_capture::capture_sink final : public conjure::core::log_sink {
    mutable std::mutex mutex = {};
    std::vector<captured_log_entry> entries = {};

    void write(const conjure::core::log_message &message) override {
        const std::scoped_lock lock(mutex);
        entries.push_back({message.level, std::string(message.category), std::string(message.text)});
    }
};

log_capture::log_capture() :
    mSink(std::make_shared<capture_sink>()),
    mPreviousConsoleEnabled(conjure::core::log::console_enabled()) {
    conjure::core::log::set_console_enabled(false);
    conjure::core::log::add_sink(mSink);
}

log_capture::~log_capture() {
    conjure::core::log::remove_sink(mSink);
    conjure::core::log::set_console_enabled(mPreviousConsoleEnabled);

    if (mErrorsAllowed) {
        return;
    }

    // Name the first offending message so a failing fixture points at the cause.
    const std::scoped_lock lock(mSink->mutex);
    for (const captured_log_entry &entry : mSink->entries) {
        if (entry.level >= conjure::core::log_level::ERR) {
            FAIL_CHECK(
              "log_capture: unexpected " << conjure::core::to_string(entry.level) << " logged by ["
                                         << entry.category << "]: " << entry.text
                                         << " (call allow_errors() when the case provokes it)"
            );
            return;
        }
    }
}

bool log_capture::contains(conjure::core::log_level level, std::string_view substring) const {
    const std::scoped_lock lock(mSink->mutex);
    for (const captured_log_entry &entry : mSink->entries) {
        if (entry.level == level && entry.text.find(substring) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool log_capture::contains(std::string_view substring) const {
    const std::scoped_lock lock(mSink->mutex);
    for (const captured_log_entry &entry : mSink->entries) {
        if (entry.text.find(substring) != std::string::npos) {
            return true;
        }
    }
    return false;
}

conjure::u32 log_capture::count(conjure::core::log_level level) const noexcept {
    const std::scoped_lock lock(mSink->mutex);
    conjure::u32 total = 0;
    for (const captured_log_entry &entry : mSink->entries) {
        if (entry.level == level) {
            ++total;
        }
    }
    return total;
}

conjure::u32 log_capture::count() const noexcept {
    const std::scoped_lock lock(mSink->mutex);
    return static_cast<conjure::u32>(mSink->entries.size());
}

std::vector<captured_log_entry> log_capture::entries() const {
    const std::scoped_lock lock(mSink->mutex);
    return mSink->entries;
}

std::vector<std::string> log_capture::messages() const {
    const std::scoped_lock lock(mSink->mutex);
    std::vector<std::string> result = {};
    result.reserve(mSink->entries.size());
    for (const captured_log_entry &entry : mSink->entries) {
        result.push_back(entry.text);
    }
    return result;
}

std::vector<std::string> log_capture::messages(conjure::core::log_level level) const {
    const std::scoped_lock lock(mSink->mutex);
    std::vector<std::string> result = {};
    for (const captured_log_entry &entry : mSink->entries) {
        if (entry.level == level) {
            result.push_back(entry.text);
        }
    }
    return result;
}

void log_capture::allow_errors() noexcept {
    mErrorsAllowed = true;
}

void log_capture::clear() {
    const std::scoped_lock lock(mSink->mutex);
    mSink->entries.clear();
}


/*===========================================================================*/
/*===========================================================================*/
// Global ring buffer

namespace {
    // Fixed-size entries: the ring is written on every log call of every test, so it must not
    // allocate (allocation gates would otherwise see the logging infrastructure).
    struct ring_entry {
        static constexpr conjure::usize CATEGORY_LENGTH = 31;
        static constexpr conjure::usize TEXT_LENGTH = 223;

        conjure::core::log_level level = conjure::core::log_level::TRACE;
        std::chrono::system_clock::time_point timestamp = {};
        std::array<char, CATEGORY_LENGTH + 1> category = {};
        std::array<char, TEXT_LENGTH + 1> text = {};
    };

    template <conjure::usize Length>
    void copy_truncated(std::array<char, Length> &destination, std::string_view source) noexcept {
        const conjure::usize count = std::min(source.size(), Length - 1);
        std::memcpy(destination.data(), source.data(), count);
        destination[count] = '\0';
    }

    class ring_sink final : public conjure::core::log_sink {
        private:
            mutable std::mutex mMutex = {};
            std::vector<ring_entry> mEntries = {};
            conjure::usize mNext = 0;
            conjure::usize mCount = 0;

        public:
            explicit ring_sink(conjure::usize capacity) {
                mEntries.resize(std::max<conjure::usize>(capacity, 1));
            }

            ~ring_sink() override = default;

        public:
            void write(const conjure::core::log_message &message) override {
                const std::scoped_lock lock(mMutex);
                ring_entry &entry = mEntries[mNext];
                entry.level = message.level;
                entry.timestamp = message.timestamp;
                copy_truncated(entry.category, message.category);
                copy_truncated(entry.text, message.text);
                mNext = (mNext + 1) % mEntries.size();
                mCount = std::min(mCount + 1, mEntries.size());
            }

            void clear() {
                const std::scoped_lock lock(mMutex);
                mNext = 0;
                mCount = 0;
            }

            [[nodiscard]] conjure::usize capacity() const noexcept {
                return mEntries.size();
            }

            [[nodiscard]] std::vector<std::string> lines() const {
                const std::scoped_lock lock(mMutex);
                std::vector<std::string> result = {};
                result.reserve(mCount);
                const conjure::usize first = (mNext + mEntries.size() - mCount) % mEntries.size();
                for (conjure::usize index = 0; index < mCount; ++index) {
                    const ring_entry &entry = mEntries[(first + index) % mEntries.size()];
                    conjure::core::log_message message = {};
                    message.level = entry.level;
                    message.category = entry.category.data();
                    message.text = entry.text.data();
                    message.timestamp = entry.timestamp;
                    result.push_back(conjure::core::format_log_line(message));
                }
                return result;
            }
    };

    std::shared_ptr<ring_sink> sRing = nullptr;
}

void conjure::testing::detail::install_global_log_ring(usize capacity) {
    if (sRing == nullptr || sRing->capacity() != std::max<usize>(capacity, 1)) {
        if (sRing != nullptr) {
            conjure::core::log::remove_sink(sRing);
        }
        sRing = std::make_shared<ring_sink>(capacity);
    }

    // Exactly one registration whatever a test did to the sink list in between.
    conjure::core::log::remove_sink(sRing);
    conjure::core::log::add_sink(sRing);
}

void conjure::testing::detail::clear_global_log_ring() {
    if (sRing != nullptr) {
        sRing->clear();
    }
}

std::vector<std::string> conjure::testing::detail::global_log_ring_lines() {
    if (sRing == nullptr) {
        return {};
    }
    return sRing->lines();
}

conjure::usize conjure::testing::detail::global_log_ring_capacity() noexcept {
    return sRing == nullptr ? 0 : sRing->capacity();
}
