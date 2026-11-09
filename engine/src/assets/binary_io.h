#pragma once

#include "conjure/assets/asset_types.h"

#include "conjure/core/types.h"
#include "conjure/core/uuid.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Little-endian serialisation shared by cooked blobs, packs and the database snapshot. Every
// on-disk format of the module is written field by field through these helpers so the layout
// never depends on struct padding or host endianness.

namespace conjure::assets::detail {

    /// <summary>
    /// Appends little-endian integers, uuids and raw bytes to a byte vector.
    /// </summary>
    class byte_writer {
        private:
            std::vector<std::byte> *mOut = nullptr;

        public:
            explicit byte_writer(std::vector<std::byte> &out) noexcept :
                mOut(&out) {}

        public:
            void u8_value(u8 value) {
                mOut->push_back(static_cast<std::byte>(value));
            }

            void u32_value(u32 value) {
                for (u32 shift = 0; shift < 32; shift += 8) {
                    mOut->push_back(static_cast<std::byte>((value >> shift) & 0xFFu));
                }
            }

            void i64_value(i64 value) {
                u64_value(static_cast<u64>(value));
            }

            void u64_value(u64 value) {
                for (u32 shift = 0; shift < 64; shift += 8) {
                    mOut->push_back(static_cast<std::byte>((value >> shift) & 0xFFu));
                }
            }

            void uuid_value(const core::uuid &value) {
                u64_value(value.high());
                u64_value(value.low());
            }

            void bytes(std::span<const std::byte> data) {
                mOut->insert(mOut->end(), data.begin(), data.end());
            }

            void chars(std::span<const char> data) {
                for (const char character : data) {
                    mOut->push_back(static_cast<std::byte>(character));
                }
            }

            /// <summary>
            /// A u32 length prefix followed by the bytes.
            /// </summary>
            void string_value(std::string_view text) {
                u32_value(static_cast<u32>(text.size()));
                chars(std::span<const char>(text.data(), text.size()));
            }

            /// <summary>
            /// Appends zero bytes until the size is a multiple of 'alignment'.
            /// </summary>
            void align(usize alignment) {
                while (mOut->size() % alignment != 0) {
                    mOut->push_back(std::byte{0});
                }
            }

            [[nodiscard]] usize size() const noexcept {
                return mOut->size();
            }
    };

    /// <summary>
    /// Reads little-endian values from a byte range with bounds checking: every read reports
    /// whether it stayed inside the buffer, and a failed read leaves the cursor unchanged.
    /// </summary>
    class byte_reader {
        private:
            std::span<const std::byte> mBytes = {};
            usize mCursor = 0;

        public:
            explicit byte_reader(std::span<const std::byte> bytes, usize offset = 0) noexcept :
                mBytes(bytes),
                mCursor(offset) {}

        public:
            [[nodiscard]] usize cursor() const noexcept {
                return mCursor;
            }

            [[nodiscard]] usize remaining() const noexcept {
                return mCursor > mBytes.size() ? 0 : mBytes.size() - mCursor;
            }

            [[nodiscard]] bool can_read(usize count) const noexcept {
                return remaining() >= count;
            }

            void seek(usize offset) noexcept {
                mCursor = offset;
            }

            [[nodiscard]] bool u8_value(u8 &out) noexcept {
                if (!can_read(1)) {
                    return false;
                }
                out = static_cast<u8>(mBytes[mCursor]);
                ++mCursor;
                return true;
            }

            [[nodiscard]] bool u32_value(u32 &out) noexcept {
                if (!can_read(4)) {
                    return false;
                }
                u32 value = 0;
                for (u32 index = 0; index < 4; ++index) {
                    value |= static_cast<u32>(static_cast<u8>(mBytes[mCursor + index])) << (index * 8);
                }
                mCursor += 4;
                out = value;
                return true;
            }

            [[nodiscard]] bool u64_value(u64 &out) noexcept {
                if (!can_read(8)) {
                    return false;
                }
                u64 value = 0;
                for (u32 index = 0; index < 8; ++index) {
                    value |= static_cast<u64>(static_cast<u8>(mBytes[mCursor + index])) << (index * 8);
                }
                mCursor += 8;
                out = value;
                return true;
            }

            [[nodiscard]] bool i64_value(i64 &out) noexcept {
                u64 raw = 0;
                if (!u64_value(raw)) {
                    return false;
                }
                out = static_cast<i64>(raw);
                return true;
            }

            [[nodiscard]] bool uuid_value(core::uuid &out) noexcept {
                if (!can_read(16)) {
                    return false;
                }
                u64 high = 0;
                u64 low = 0;
                static_cast<void>(u64_value(high));
                static_cast<void>(u64_value(low));
                out = core::uuid(high, low);
                return true;
            }

            [[nodiscard]] bool bytes(usize count, std::span<const std::byte> &out) noexcept {
                if (!can_read(count)) {
                    return false;
                }
                out = mBytes.subspan(mCursor, count);
                mCursor += count;
                return true;
            }

            [[nodiscard]] bool chars(std::span<char> out) noexcept {
                if (!can_read(out.size())) {
                    return false;
                }
                for (usize index = 0; index < out.size(); ++index) {
                    out[index] = static_cast<char>(mBytes[mCursor + index]);
                }
                mCursor += out.size();
                return true;
            }

            /// <summary>
            /// A u32 length prefix followed by the bytes. Rejects lengths above 'maxLength'.
            /// </summary>
            [[nodiscard]] bool string_value(std::string &out, usize maxLength = 1u << 20) noexcept {
                u32 length = 0;
                const usize start = mCursor;
                if (!u32_value(length)) {
                    return false;
                }
                if (length > maxLength || !can_read(length)) {
                    mCursor = start;
                    return false;
                }
                out.resize(length);
                for (usize index = 0; index < length; ++index) {
                    out[index] = static_cast<char>(mBytes[mCursor + index]);
                }
                mCursor += length;
                return true;
            }
    };
}
