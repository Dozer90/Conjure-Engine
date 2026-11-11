#pragma once

#include "conjure/core/api.h"
#include "conjure/core/result.h"

#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <type_traits>

// Shared libraries (DLL / .so / .dylib) loaded at runtime, used for engine modules and plugins.
//
//   CONJURE_TRY_ASSIGN(dynamic_library plugin, dynamic_library::load(dynamic_library::decorate_name("conjure_foo")));
//   auto create = plugin.symbol_as<module_desc *(*)()>("conjure_create_module");

namespace conjure::core {

    /// <summary>
    /// RAII handle to a loaded shared library. Move-only; the library is unloaded when the last
    /// owner is destroyed. Symbols looked up through it are invalid after that.
    /// </summary>
    class CONJURE_API dynamic_library {
        private:
            void *mHandle = nullptr;
            std::filesystem::path mPath = {};

        public:
            dynamic_library();
            ~dynamic_library();

            dynamic_library(const dynamic_library &) = delete;
            dynamic_library &operator=(const dynamic_library &) = delete;

            dynamic_library(dynamic_library &&other) noexcept;
            dynamic_library &operator=(dynamic_library &&other) noexcept;

        public:
            /// <summary>
            /// Loads a library (LoadLibraryW / dlopen with RTLD_NOW | RTLD_LOCAL). A bare file
            /// name is resolved through the operating system's library search path. Fails with
            /// INVALID_ARGUMENT for an empty path, NOT_FOUND when the file does not exist and
            /// BACKEND_FAILURE otherwise; the message carries the operating system's error text.
            /// </summary>
            [[nodiscard]] static result<dynamic_library> load(const std::filesystem::path &path);

            /// <summary>
            /// Unloads the library now. Safe to call when nothing is loaded.
            /// </summary>
            void unload() noexcept;

            /// <summary>
            /// True while a library is loaded.
            /// </summary>
            [[nodiscard]] bool is_loaded() const noexcept {
                return mHandle != nullptr;
            }

            /// <summary>
            /// The path given to load(), empty when nothing is loaded.
            /// </summary>
            [[nodiscard]] const std::filesystem::path &path() const noexcept {
                return mPath;
            }

            /// <summary>
            /// Address of an exported symbol, nullptr when it does not exist or nothing is loaded.
            /// </summary>
            [[nodiscard]] void *symbol(const char *name) const noexcept;

            /// <summary>
            /// symbol() converted to a pointer type, typically a function pointer. Returns nullptr
            /// when the symbol does not exist.
            /// </summary>
            template <typename Fn>
            [[nodiscard]] Fn symbol_as(const char *name) const noexcept {
                static_assert(std::is_pointer_v<Fn>, "symbol_as requires a pointer type");
                static_assert(sizeof(Fn) == sizeof(void *), "symbol_as requires a plain pointer type");

                // Converting an object pointer to a function pointer is only conditionally
                // supported by the language; copying the bits is well defined on every target.
                void *address = symbol(name);
                Fn typed = nullptr;
                std::memcpy(&typed, &address, sizeof(typed));
                return typed;
            }

            /// <summary>
            /// The platform file name of a library: "conjure_foo" becomes "conjure_foo.dll",
            /// "libconjure_foo.so" or "libconjure_foo.dylib". A name that already ends in the
            /// platform extension is returned unchanged.
            /// </summary>
            [[nodiscard]] static std::string decorate_name(std::string_view baseName);

            /// <summary>
            /// The platform's library extension including the dot: ".dll", ".so" or ".dylib".
            /// </summary>
            [[nodiscard]] static std::string_view extension() noexcept;

            /// <summary>
            /// The platform's library prefix: "" on Windows, "lib" elsewhere.
            /// </summary>
            [[nodiscard]] static std::string_view prefix() noexcept;
    };
}
