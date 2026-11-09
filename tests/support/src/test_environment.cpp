#include "conjure/testing/test_environment.h"

#include "conjure/testing/doctest.h"

#include "conjure/core/log.h"

#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <thread>

using namespace conjure;
using namespace conjure::testing;

/*===========================================================================*/
/*===========================================================================*/
// State

namespace {
    test_environment sEnvironment = {};
    std::thread::id sTestThread = {};
    std::array<std::atomic<precondition_probe>, static_cast<usize>(precondition::COUNT)> sProbes = {};

    bool downloaded_assets_present() noexcept {
        const std::filesystem::path &root = sEnvironment.downloadedAssetRoot;
        if (root.empty()) {
            return false;
        }
        std::error_code ignored = {};
        const std::filesystem::path stamp = root.parent_path() / ".assets_ready";
        return std::filesystem::is_directory(root, ignored) && std::filesystem::exists(stamp, ignored);
    }

    bool probe_holds(precondition value) noexcept {
        const precondition_probe probe = sProbes[static_cast<usize>(value)].load(std::memory_order_acquire);
        return probe != nullptr && probe();
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Public functions

std::string_view conjure::testing::to_string(precondition value) noexcept {
    switch (value) {
        case precondition::DOTNET_RUNTIME: {
            return "DOTNET_RUNTIME";
        }
        case precondition::DOWNLOADED_ASSETS: {
            return "DOWNLOADED_ASSETS";
        }
        case precondition::SOFTWARE_GPU_ADAPTER: {
            return "SOFTWARE_GPU_ADAPTER";
        }
        case precondition::LOOPBACK_TCP: {
            return "LOOPBACK_TCP";
        }
        case precondition::REAL_WINDOW: {
            return "REAL_WINDOW";
        }
        case precondition::AUDIO_DEVICE: {
            return "AUDIO_DEVICE";
        }
        case precondition::COUNT: {
            break;
        }
    }
    return "UNKNOWN";
}

const test_environment &conjure::testing::environment() noexcept {
    return sEnvironment;
}

bool conjure::testing::precondition_holds(precondition value) noexcept {
    switch (value) {
        case precondition::DOWNLOADED_ASSETS: {
            return downloaded_assets_present();
        }
        case precondition::REAL_WINDOW: {
            return sEnvironment.realWindowAllowed;
        }
        case precondition::DOTNET_RUNTIME:
        case precondition::SOFTWARE_GPU_ADAPTER:
        case precondition::LOOPBACK_TCP:
        case precondition::AUDIO_DEVICE: {
            return probe_holds(value);
        }
        case precondition::COUNT: {
            break;
        }
    }
    return false;
}

bool conjure::testing::require_precondition(precondition value, const char *file, u32 line) {
    if (precondition_holds(value)) {
        return true;
    }

    const std::string reason = std::format("skipped: precondition {} not met ({}:{})", to_string(value), file, line);
    if (sEnvironment.isolatedProcess) {
        std::fputs(reason.c_str(), stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);
        core::log::flush();
        std::exit(SKIP_EXIT_CODE);
    }

    MESSAGE(reason);
    return false;
}

void conjure::testing::set_precondition_probe(precondition value, precondition_probe probe) noexcept {
    if (value >= precondition::COUNT) {
        return;
    }
    sProbes[static_cast<usize>(value)].store(probe, std::memory_order_release);
}


/*===========================================================================*/
/*===========================================================================*/
// Detail

void conjure::testing::detail::set_environment(const test_environment &value) {
    sEnvironment = value;
    sTestThread = std::this_thread::get_id();
}

bool conjure::testing::detail::is_test_thread() noexcept {
    return sTestThread != std::thread::id() && sTestThread == std::this_thread::get_id();
}
