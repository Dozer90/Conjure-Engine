#include "conjure/editor/protocol/access_token.h"

#include <random>
#include <string>

namespace conjure::editor {

    namespace {
        constexpr char HEX_DIGITS[] = "0123456789abcdef";
        constexpr std::string_view FULL_CONTROL_TOKEN_FILE = "ai_token";
        constexpr std::string_view INSPECT_TOKEN_FILE = "ai_token_inspect";
        constexpr char ENTROPY_DEVICE[] = "/dev/urandom";
    }

    std::string_view to_string(access_level level) noexcept {
        switch (level) {
            case access_level::NONE:
                return "none";
            case access_level::INSPECT:
                return "inspect";
            case access_level::FULL_CONTROL:
                return "full_control";
        }
        return "none";
    }

    std::string generate_access_token() {
        // The device is named: asked for nothing, libstdc++ on x86 takes the processor's RDSEED or RDRAND
        // instruction before the kernel's generator. Named, it reads /dev/urandom; libc++ accepts the name in
        // its /dev/urandom, getentropy and arc4random builds, and the Microsoft library ignores it (rand_s).
        // No pseudo-random generator in between: every byte of the token comes from that source, so the token
        // cannot be reconstructed from a seed.
        std::random_device entropy(ENTROPY_DEVICE);
        std::string token = {};
        token.reserve(ACCESS_TOKEN_LENGTH);
        usize produced = 0;
        while (produced < ACCESS_TOKEN_BYTES) {
            std::random_device::result_type word = entropy();
            for (usize index = 0; index < sizeof(word) && produced < ACCESS_TOKEN_BYTES; ++index) {
                const u8 value = static_cast<u8>(word & 0xFFu);
                word >>= 8;
                token.push_back(HEX_DIGITS[value >> 4]);
                token.push_back(HEX_DIGITS[value & 0x0Fu]);
                ++produced;
            }
        }
        return token;
    }

    bool constant_time_equals(std::string_view presented, std::string_view expected) noexcept {
        if (expected.empty()) {
            return presented.empty();
        }
        // Every byte of 'presented' is compared, whatever the outcome so far: an early return at the first
        // difference would tell a client how long a prefix of its guess was right. A length that differs is
        // folded into the same accumulator, and 'expected' is read cyclically so that the loop runs the
        // same way for a guess of any length.
        u8 difference = presented.size() == expected.size() ? u8{0} : u8{1};
        for (usize index = 0; index < presented.size(); ++index) {
            const u8 left = static_cast<u8>(presented[index]);
            const u8 right = static_cast<u8>(expected[index % expected.size()]);
            difference = static_cast<u8>(difference | (left ^ right));
        }
        return difference == 0;
    }

    bool constant_time_starts_at(std::string_view text, usize offset, std::string_view token) noexcept {
        if (token.empty() || offset > text.size() || text.size() - offset < token.size()) {
            return false;
        }
        return constant_time_equals(text.substr(offset, token.size()), token);
    }

    bool constant_time_contains(std::string_view text, std::string_view token) noexcept {
        if (token.empty() || text.size() < token.size()) {
            return false;
        }
        // No early return at a match either: a text that holds the token takes as long as one that does not.
        bool found = false;
        const usize lastOffset = text.size() - token.size();
        for (usize offset = 0; offset <= lastOffset; ++offset) {
            const bool here = constant_time_starts_at(text, offset, token);
            found = found || here;
        }
        return found;
    }

    std::string_view token_file_name(access_level level) noexcept {
        switch (level) {
            case access_level::NONE:
                return {};
            case access_level::INSPECT:
                return INSPECT_TOKEN_FILE;
            case access_level::FULL_CONTROL:
                return FULL_CONTROL_TOKEN_FILE;
        }
        return {};
    }
}
