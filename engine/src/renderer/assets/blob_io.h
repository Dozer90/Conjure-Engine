#pragma once

#include "conjure/renderer/renderer_error.h"

#include "conjure/core/error.h"
#include "conjure/core/math.h"
#include "conjure/core/result.h"
#include "conjure/core/types.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Little-endian field I/O for the renderer's cooked blobs (.cmesh, .ctex, .cenv). The writer
// appends fields, the reader validates every length before it copies so a truncated or corrupt blob
// fails with renderer_error::INVALID_ASSET instead of reading out of bounds. Multi-byte fields are
// copied verbatim on little-endian hosts (asserted at compile time) which keeps the formats
// deterministic across the supported platforms.

namespace conjure::renderer::detail {

    static_assert(std::endian::native == std::endian::little, "renderer blobs are little-endian and written verbatim");

    class blob_writer {
        private:
            std::vector<std::byte> mBytes = {};

        public:
            blob_writer() = default;
            ~blob_writer() = default;

        public:
            void write_u8(u8 value) {
                mBytes.push_back(static_cast<std::byte>(value));
            }

            void write_u16(u16 value) {
                write_raw(&value, sizeof(value));
            }

            void write_u32(u32 value) {
                write_raw(&value, sizeof(value));
            }

            void write_u64(u64 value) {
                write_raw(&value, sizeof(value));
            }

            void write_i32(i32 value) {
                write_raw(&value, sizeof(value));
            }

            void write_f32(f32 value) {
                write_raw(&value, sizeof(value));
            }

            void write_vec3(const math::vec3 &value) {
                write_f32(value.x);
                write_f32(value.y);
                write_f32(value.z);
            }

            void write_magic(const std::array<char, 4> &magic) {
                write_raw(magic.data(), magic.size());
            }

            /// <summary>
            /// A u32 length followed by the UTF-8 bytes.
            /// </summary>
            void write_string(std::string_view text) {
                write_u32(static_cast<u32>(text.size()));
                write_raw(text.data(), text.size());
            }

            void write_bytes(std::span<const std::byte> bytes) {
                mBytes.insert(mBytes.end(), bytes.begin(), bytes.end());
            }

            /// <summary>
            /// Copies a trivially copyable array verbatim.
            /// </summary>
            template <typename T>
            void write_array(std::span<const T> values) {
                write_raw(values.data(), values.size_bytes());
            }

            [[nodiscard]] usize size() const noexcept {
                return mBytes.size();
            }

            [[nodiscard]] std::vector<std::byte> finish() {
                return std::move(mBytes);
            }

        private:
            void write_raw(const void *data, usize size) {
                if (size == 0) {
                    return;
                }
                const usize offset = mBytes.size();
                mBytes.resize(offset + size);
                std::memcpy(mBytes.data() + offset, data, size);
            }
    };

    class blob_reader {
        private:
            std::span<const std::byte> mBytes = {};
            usize mPosition = 0;
            std::string_view mFormat = "";

        public:
            blob_reader(std::span<const std::byte> bytes, std::string_view formatName) noexcept :
                mBytes(bytes),
                mFormat(formatName) {}
            ~blob_reader() = default;

        public:
            [[nodiscard]] core::error short_read(std::string_view what) const {
                return core::make_error(
                  renderer_error::INVALID_ASSET,
                  "{}: truncated blob while reading {} at offset {} of {} bytes",
                  mFormat,
                  what,
                  mPosition,
                  mBytes.size()
                );
            }

            [[nodiscard]] core::error invalid(std::string_view detail) const {
                return core::make_error(renderer_error::INVALID_ASSET, "{}: {}", mFormat, detail);
            }

            [[nodiscard]] core::result<void> expect_magic(const std::array<char, 4> &magic) {
                if (mBytes.size() - mPosition < magic.size()) {
                    return short_read("magic");
                }
                if (std::memcmp(mBytes.data() + mPosition, magic.data(), magic.size()) != 0) {
                    return invalid("wrong magic");
                }
                mPosition += magic.size();
                return core::result<void>();
            }

            [[nodiscard]] core::result<u8> read_u8() {
                return read_value<u8>("u8");
            }

            [[nodiscard]] core::result<u16> read_u16() {
                return read_value<u16>("u16");
            }

            [[nodiscard]] core::result<u32> read_u32() {
                return read_value<u32>("u32");
            }

            [[nodiscard]] core::result<u64> read_u64() {
                return read_value<u64>("u64");
            }

            [[nodiscard]] core::result<i32> read_i32() {
                return read_value<i32>("i32");
            }

            [[nodiscard]] core::result<f32> read_f32() {
                return read_value<f32>("f32");
            }

            [[nodiscard]] core::result<math::vec3> read_vec3() {
                math::vec3 value = {};
                CONJURE_TRY_ASSIGN(value.x, read_f32());
                CONJURE_TRY_ASSIGN(value.y, read_f32());
                CONJURE_TRY_ASSIGN(value.z, read_f32());
                return value;
            }

            [[nodiscard]] core::result<std::string> read_string(usize maxLength) {
                u32 length = 0;
                CONJURE_TRY_ASSIGN(length, read_u32());
                if (length > maxLength) {
                    return invalid("string length exceeds the limit");
                }
                std::span<const std::byte> bytes = {};
                CONJURE_TRY_ASSIGN(bytes, read_bytes(length, "string"));
                return std::string(reinterpret_cast<const char *>(bytes.data()), bytes.size());
            }

            /// <summary>
            /// A view of the next 'count' bytes.
            /// </summary>
            [[nodiscard]] core::result<std::span<const std::byte>> read_bytes(usize count, std::string_view what) {
                if (mBytes.size() - mPosition < count) {
                    return short_read(what);
                }
                const std::span<const std::byte> view = mBytes.subspan(mPosition, count);
                mPosition += count;
                return view;
            }

            /// <summary>
            /// Copies 'count' trivially copyable elements.
            /// </summary>
            template <typename T>
            [[nodiscard]] core::result<std::vector<T>> read_array(usize count, std::string_view what) {
                std::span<const std::byte> bytes = {};
                CONJURE_TRY_ASSIGN(bytes, read_bytes(count * sizeof(T), what));
                std::vector<T> values(count);
                if (count > 0) {
                    std::memcpy(values.data(), bytes.data(), bytes.size());
                }
                return values;
            }

            [[nodiscard]] usize remaining() const noexcept {
                return mBytes.size() - mPosition;
            }

            [[nodiscard]] usize position() const noexcept {
                return mPosition;
            }

        private:
            template <typename T>
            [[nodiscard]] core::result<T> read_value(std::string_view what) {
                if (mBytes.size() - mPosition < sizeof(T)) {
                    return short_read(what);
                }
                T value = {};
                std::memcpy(&value, mBytes.data() + mPosition, sizeof(T));
                mPosition += sizeof(T);
                return value;
            }
    };

    /// <summary>
    /// Upper bound on the name strings stored in blobs.
    /// </summary>
    inline constexpr usize MAX_BLOB_NAME_LENGTH = 4096;
}
