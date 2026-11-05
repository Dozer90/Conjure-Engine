#include "conjure_test.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <random>

using namespace conjure::test;

/*===========================================================================*/
/*===========================================================================*/
// log_capture

struct log_capture::capture_sink final : public conjure::core::log_sink {
    struct entry {
        conjure::core::log_level level = conjure::core::log_level::TRACE;
        std::string text = {};
    };

    mutable std::mutex mutex = {};
    std::vector<entry> entries = {};

    void write(const conjure::core::log_message &message) override {
        const std::scoped_lock lock(mutex);
        entries.push_back({message.level, std::string(message.text)});
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
}

std::vector<std::string> log_capture::messages() const {
    const std::scoped_lock lock(mSink->mutex);
    std::vector<std::string> result = {};
    result.reserve(mSink->entries.size());
    for (const capture_sink::entry &entry : mSink->entries) {
        result.push_back(entry.text);
    }
    return result;
}

std::vector<std::string> log_capture::messages(conjure::core::log_level level) const {
    const std::scoped_lock lock(mSink->mutex);
    std::vector<std::string> result = {};
    for (const capture_sink::entry &entry : mSink->entries) {
        if (entry.level == level) {
            result.push_back(entry.text);
        }
    }
    return result;
}

bool log_capture::contains(std::string_view text) const {
    const std::scoped_lock lock(mSink->mutex);
    for (const capture_sink::entry &entry : mSink->entries) {
        if (entry.text.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

conjure::usize log_capture::count(conjure::core::log_level minimumLevel) const {
    const std::scoped_lock lock(mSink->mutex);
    conjure::usize total = 0;
    for (const capture_sink::entry &entry : mSink->entries) {
        if (entry.level >= minimumLevel) {
            ++total;
        }
    }
    return total;
}

void log_capture::clear() {
    const std::scoped_lock lock(mSink->mutex);
    mSink->entries.clear();
}


/*===========================================================================*/
/*===========================================================================*/
// assert_capture

namespace {
    std::mutex sAssertMutex = {};
    conjure::usize sAssertCount = 0;
    std::string sLastAssertMessage = {};

    conjure::core::assert_action capturing_assert_handler(const conjure::core::assert_info &info) {
        const std::scoped_lock lock(sAssertMutex);
        ++sAssertCount;
        sLastAssertMessage = info.message.empty() ? std::string(info.expression) : info.message;
        return conjure::core::assert_action::CONTINUE;
    }
}

assert_capture::assert_capture() :
    mPreviousHandler(conjure::core::set_assert_handler(&capturing_assert_handler)) {
    reset();
}

assert_capture::~assert_capture() {
    conjure::core::set_assert_handler(mPreviousHandler);
}

conjure::usize assert_capture::count() const {
    const std::scoped_lock lock(sAssertMutex);
    return sAssertCount;
}

std::string assert_capture::last_message() const {
    const std::scoped_lock lock(sAssertMutex);
    return sLastAssertMessage;
}

void assert_capture::reset() {
    const std::scoped_lock lock(sAssertMutex);
    sAssertCount = 0;
    sLastAssertMessage.clear();
}


/*===========================================================================*/
/*===========================================================================*/
// temp_directory

namespace {
    std::atomic<conjure::u32> sTempDirectoryCounter = 0;
}

temp_directory::temp_directory(std::string_view prefix) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const conjure::u32 sequence = sTempDirectoryCounter.fetch_add(1);
    const std::string name = std::string(prefix) + "_" + std::to_string(now) + "_" + std::to_string(sequence);

    mPath = std::filesystem::temp_directory_path() / "conjure" / name;
    std::filesystem::create_directories(mPath);
}

temp_directory::~temp_directory() {
    std::error_code ignored = {};
    std::filesystem::remove_all(mPath, ignored);
}

std::filesystem::path temp_directory::write_file(std::string_view relativePath, std::string_view contents) const {
    const std::filesystem::path fullPath = mPath / std::filesystem::path(relativePath);
    std::filesystem::create_directories(fullPath.parent_path());

    std::ofstream stream(fullPath, std::ios::binary | std::ios::trunc);
    stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return fullPath;
}
