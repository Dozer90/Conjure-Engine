#include "conjure/core/uuid.h"

#include <array>
#include <random>

using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// Generation

namespace {
    std::mt19937_64 &thread_generator() {
        // One generator per thread avoids locking; each is seeded from the OS entropy source.
        thread_local std::mt19937_64 generator = [] {
            std::random_device device = {};
            std::array<std::uint32_t, std::mt19937_64::state_size * 2> seedData = {};
            for (std::uint32_t &value : seedData) {
                value = device();
            }
            std::seed_seq sequence(seedData.begin(), seedData.end());
            return std::mt19937_64(sequence);
        }();
        return generator;
    }

    constexpr char HEX_DIGITS[] = "0123456789abcdef";

    void append_hex(std::string &output, conjure::u64 value, int digits) {
        for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4) {
            output.push_back(HEX_DIGITS[(value >> shift) & 0xF]);
        }
    }

    int hex_value(char character) noexcept {
        if (character >= '0' && character <= '9') {
            return character - '0';
        }
        if (character >= 'a' && character <= 'f') {
            return 10 + (character - 'a');
        }
        if (character >= 'A' && character <= 'F') {
            return 10 + (character - 'A');
        }
        return -1;
    }
}

uuid uuid::generate() {
    std::mt19937_64 &generator = thread_generator();
    conjure::u64 high = generator();
    conjure::u64 low = generator();

    // RFC 4122 version 4: set the version nibble to 4 and the variant bits to 10xx.
    high = (high & 0xFFFFFFFFFFFF0FFFull) | 0x0000000000004000ull;
    low = (low & 0x3FFFFFFFFFFFFFFFull) | 0x8000000000000000ull;

    return uuid(high, low);
}


/*===========================================================================*/
/*===========================================================================*/
// Parsing and formatting

std::optional<uuid> uuid::parse(std::string_view text) noexcept {
    if (text.size() == 38 && text.front() == '{' && text.back() == '}') {
        text = text.substr(1, 36);
    }
    if (text.size() != 36) {
        return std::nullopt;
    }

    conjure::u64 high = 0;
    conjure::u64 low = 0;
    int nibbleIndex = 0;

    for (conjure::usize position = 0; position < text.size(); ++position) {
        const char character = text[position];
        const bool expectDash = position == 8 || position == 13 || position == 18 || position == 23;

        if (expectDash) {
            if (character != '-') {
                return std::nullopt;
            }
            continue;
        }

        const int value = hex_value(character);
        if (value < 0) {
            return std::nullopt;
        }

        if (nibbleIndex < 16) {
            high = (high << 4) | static_cast<conjure::u64>(value);
        } else {
            low = (low << 4) | static_cast<conjure::u64>(value);
        }
        ++nibbleIndex;
    }

    return uuid(high, low);
}

std::string uuid::to_string() const {
    std::string output = {};
    output.reserve(36);

    append_hex(output, mHigh >> 32, 8);
    output.push_back('-');
    append_hex(output, (mHigh >> 16) & 0xFFFF, 4);
    output.push_back('-');
    append_hex(output, mHigh & 0xFFFF, 4);
    output.push_back('-');
    append_hex(output, mLow >> 48, 4);
    output.push_back('-');
    append_hex(output, mLow & 0xFFFFFFFFFFFFull, 12);

    return output;
}
