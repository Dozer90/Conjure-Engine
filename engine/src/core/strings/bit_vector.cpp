#include "conjure/core/bit_vector.h"

#include <algorithm>
#include <bit>

using namespace conjure::core;

/*===========================================================================*/
/*===========================================================================*/
// Helpers

namespace {
    [[nodiscard]] constexpr conjure::usize words_for(conjure::usize bits) noexcept {
        return (bits + bit_vector::WORD_BITS - 1) / bit_vector::WORD_BITS;
    }

    constexpr conjure::u64 ALL_ONES = ~conjure::u64{0};
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

bit_vector::bit_vector(usize size, bool value) :
    mWords(words_for(size), value ? ALL_ONES : 0u),
    mSize(size) {
    clear_trailing_bits();
}


/*===========================================================================*/
/*===========================================================================*/
// Size

void bit_vector::reserve(usize bits) {
    mWords.reserve(words_for(bits));
}

void bit_vector::resize(usize size, bool value) {
    if (size == mSize) {
        return;
    }

    if (size < mSize) {
        mWords.resize(words_for(size));
        mSize = size;
        clear_trailing_bits();
        return;
    }

    // Growing: the padding bits of the current last word are zero and must take 'value' too.
    const usize oldSize = mSize;
    mWords.resize(words_for(size), value ? ALL_ONES : 0u);
    mSize = size;
    if (value && oldSize % WORD_BITS != 0) {
        mWords[oldSize / WORD_BITS] |= ALL_ONES << (oldSize % WORD_BITS);
    }
    clear_trailing_bits();
}

void bit_vector::push_back(bool value) {
    resize(mSize + 1, value);
}

void bit_vector::clear() noexcept {
    mWords.clear();
    mSize = 0;
}


/*===========================================================================*/
/*===========================================================================*/
// All bits

void bit_vector::set_all() noexcept {
    std::fill(mWords.begin(), mWords.end(), ALL_ONES);
    clear_trailing_bits();
}

void bit_vector::reset_all() noexcept {
    std::fill(mWords.begin(), mWords.end(), u64{0});
}

void bit_vector::flip_all() noexcept {
    for (u64 &word : mWords) {
        word = ~word;
    }
    clear_trailing_bits();
}

bool bit_vector::any() const noexcept {
    for (const u64 word : mWords) {
        if (word != 0) {
            return true;
        }
    }
    return false;
}

bool bit_vector::all() const noexcept {
    if (mSize == 0) {
        return true;
    }

    const usize fullWords = mSize / WORD_BITS;
    for (usize index = 0; index < fullWords; ++index) {
        if (mWords[index] != ALL_ONES) {
            return false;
        }
    }

    const usize remainder = mSize % WORD_BITS;
    if (remainder == 0) {
        return true;
    }
    const u64 mask = ALL_ONES >> (WORD_BITS - remainder);
    return (mWords[fullWords] & mask) == mask;
}

conjure::usize bit_vector::count() const noexcept {
    usize total = 0;
    for (const u64 word : mWords) {
        total += static_cast<usize>(std::popcount(word));
    }
    return total;
}


/*===========================================================================*/
/*===========================================================================*/
// Searching

conjure::usize bit_vector::find_first_set(usize from) const noexcept {
    if (from >= mSize) {
        return NPOS;
    }

    usize wordIndex = from / WORD_BITS;
    // Mask off the bits below 'from' in the first word; later words are taken whole.
    u64 word = mWords[wordIndex] & (ALL_ONES << (from % WORD_BITS));
    while (true) {
        if (word != 0) {
            return wordIndex * WORD_BITS + static_cast<usize>(std::countr_zero(word));
        }
        ++wordIndex;
        if (wordIndex >= mWords.size()) {
            return NPOS;
        }
        word = mWords[wordIndex];
    }
}

conjure::usize bit_vector::find_first_clear(usize from) const noexcept {
    if (from >= mSize) {
        return NPOS;
    }

    usize wordIndex = from / WORD_BITS;
    u64 inverted = ~mWords[wordIndex] & (ALL_ONES << (from % WORD_BITS));
    while (true) {
        if (inverted != 0) {
            const usize index = wordIndex * WORD_BITS + static_cast<usize>(std::countr_zero(inverted));
            // The padding bits past size() read as clear but are not real bits.
            return index < mSize ? index : NPOS;
        }
        ++wordIndex;
        if (wordIndex >= mWords.size()) {
            return NPOS;
        }
        inverted = ~mWords[wordIndex];
    }
}

conjure::usize bit_vector::find_last_set() const noexcept {
    for (usize wordIndex = mWords.size(); wordIndex > 0; --wordIndex) {
        const u64 word = mWords[wordIndex - 1];
        if (word != 0) {
            return (wordIndex - 1) * WORD_BITS + (WORD_BITS - 1 - static_cast<usize>(std::countl_zero(word)));
        }
    }
    return NPOS;
}


/*===========================================================================*/
/*===========================================================================*/
// Bitwise operations

bit_vector &bit_vector::operator&=(const bit_vector &other) noexcept {
    CONJURE_ASSERT(mSize == other.mSize, "bit_vector size mismatch ({} vs {})", mSize, other.mSize);
    const usize common = std::min(mWords.size(), other.mWords.size());
    for (usize index = 0; index < common; ++index) {
        mWords[index] &= other.mWords[index];
    }
    // Bits this vector has beyond the other's end have no counterpart: and-ing with "absent" clears them.
    for (usize index = common; index < mWords.size(); ++index) {
        mWords[index] = 0;
    }
    return *this;
}

bit_vector &bit_vector::operator|=(const bit_vector &other) noexcept {
    CONJURE_ASSERT(mSize == other.mSize, "bit_vector size mismatch ({} vs {})", mSize, other.mSize);
    const usize common = std::min(mWords.size(), other.mWords.size());
    for (usize index = 0; index < common; ++index) {
        mWords[index] |= other.mWords[index];
    }
    clear_trailing_bits();
    return *this;
}

bit_vector &bit_vector::operator^=(const bit_vector &other) noexcept {
    CONJURE_ASSERT(mSize == other.mSize, "bit_vector size mismatch ({} vs {})", mSize, other.mSize);
    const usize common = std::min(mWords.size(), other.mWords.size());
    for (usize index = 0; index < common; ++index) {
        mWords[index] ^= other.mWords[index];
    }
    clear_trailing_bits();
    return *this;
}

bit_vector &bit_vector::remove(const bit_vector &other) noexcept {
    CONJURE_ASSERT(mSize == other.mSize, "bit_vector size mismatch ({} vs {})", mSize, other.mSize);
    const usize common = std::min(mWords.size(), other.mWords.size());
    for (usize index = 0; index < common; ++index) {
        mWords[index] &= ~other.mWords[index];
    }
    return *this;
}

bool bit_vector::is_subset_of(const bit_vector &other) const noexcept {
    CONJURE_ASSERT(mSize == other.mSize, "bit_vector size mismatch ({} vs {})", mSize, other.mSize);
    const usize common = std::min(mWords.size(), other.mWords.size());
    for (usize index = 0; index < common; ++index) {
        if ((mWords[index] & ~other.mWords[index]) != 0) {
            return false;
        }
    }
    for (usize index = common; index < mWords.size(); ++index) {
        if (mWords[index] != 0) {
            return false;
        }
    }
    return true;
}

bool bit_vector::intersects(const bit_vector &other) const noexcept {
    CONJURE_ASSERT(mSize == other.mSize, "bit_vector size mismatch ({} vs {})", mSize, other.mSize);
    const usize common = std::min(mWords.size(), other.mWords.size());
    for (usize index = 0; index < common; ++index) {
        if ((mWords[index] & other.mWords[index]) != 0) {
            return true;
        }
    }
    return false;
}

bit_vector conjure::core::operator&(const bit_vector &left, const bit_vector &right) {
    bit_vector result = left;
    result &= right;
    return result;
}

bit_vector conjure::core::operator|(const bit_vector &left, const bit_vector &right) {
    bit_vector result = left;
    result |= right;
    return result;
}

bit_vector conjure::core::operator^(const bit_vector &left, const bit_vector &right) {
    bit_vector result = left;
    result ^= right;
    return result;
}

bit_vector conjure::core::operator~(const bit_vector &value) {
    bit_vector result = value;
    result.flip_all();
    return result;
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

void bit_vector::clear_trailing_bits() noexcept {
    const usize remainder = mSize % WORD_BITS;
    if (remainder == 0 || mWords.empty()) {
        return;
    }
    mWords.back() &= ALL_ONES >> (WORD_BITS - remainder);
}
