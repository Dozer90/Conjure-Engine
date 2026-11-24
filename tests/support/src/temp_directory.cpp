#include "conjure/testing/temp_directory.h"

#include "conjure/testing/doctest.h"
#include "conjure/testing/test_environment.h"

#include "conjure/core/error.h"
#include "conjure/core/platform_detect.h"
#include "conjure/core/uuid.h"

#include <cstddef>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

using namespace conjure::testing;

/*===========================================================================*/
/*===========================================================================*/
// Helpers

namespace {
    std::filesystem::path temporary_root() {
        const std::filesystem::path &configured = environment().temporaryRoot;
        if (!configured.empty()) {
            return configured;
        }
        std::error_code ignored = {};
        return std::filesystem::temp_directory_path(ignored) / "conjure" / "test_tmp";
    }

    // Eight hex digits (32 random bits of a version 4 uuid) instead of the whole uuid keep the paths short:
    // a Windows checkout in a deep folder put the module tests past MAX_PATH (code sweep FU-LONGPATH). The
    // folder is created with create_directory, which reports a name another test process already took, and
    // then a new name is drawn.
    constexpr std::size_t UNIQUE_SUFFIX_LENGTH = 8;
    constexpr int MAX_NAME_ATTEMPTS = 16;

    std::filesystem::path create_unique_directory(std::string_view prefix) {
        const std::filesystem::path root = temporary_root();
        std::error_code failure = {};
        std::filesystem::create_directories(root, failure);
        REQUIRE_MESSAGE(!failure, "temp_directory: cannot create " << root.string() << ": " << failure.message());

        for (int attempt = 0; attempt < MAX_NAME_ATTEMPTS; ++attempt) {
            std::string name = conjure::core::uuid::generate().to_string().substr(0, UNIQUE_SUFFIX_LENGTH);
            if (!prefix.empty()) {
                name = std::string(prefix) + "_" + name;
            }
            const std::filesystem::path path = root / name;
            if (std::filesystem::create_directory(path, failure)) {
                return path;
            }
        }
        FAIL("temp_directory: no free name under " << root.string() << ": " << failure.message());
        return {};
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

temp_directory::temp_directory() :
    mPath(create_unique_directory({})) {}

temp_directory::temp_directory(std::string_view prefix) :
    mPath(create_unique_directory(prefix)) {}

temp_directory::~temp_directory() {
    // A case may have built a folder past MAX_PATH inside, which remove_all reaches only below the long form.
    std::error_code failure = {};
    std::filesystem::remove_all(extended_length_path(mPath), failure);
    if (failure) {
        WARN_MESSAGE(false, "temp_directory: could not remove " << mPath.string() << ": " << failure.message());
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Public methods

std::filesystem::path temp_directory::file(std::string_view name) const {
    return mPath / std::filesystem::path(name);
}

conjure::core::result<void> temp_directory::write_text(std::string_view name, std::string_view text) const {
    const std::filesystem::path fullPath = file(name);

    std::error_code failure = {};
    std::filesystem::create_directories(extended_length_path(fullPath.parent_path()), failure);
    if (failure) {
        return conjure::core::make_error(
          conjure::core::error_code::IO_FAILURE,
          "cannot create {}: {}",
          fullPath.parent_path().string(),
          failure.message()
        );
    }

    std::ofstream stream(extended_length_path(fullPath), std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        return conjure::core::make_error(conjure::core::error_code::IO_FAILURE, "cannot open {}", fullPath.string());
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!stream.good()) {
        return conjure::core::make_error(conjure::core::error_code::IO_FAILURE, "cannot write {}", fullPath.string());
    }
    return conjure::core::result<void>::success();
}

std::filesystem::path temp_directory::write_file(std::string_view relativePath, std::string_view contents) const {
    const conjure::core::result<void> written = write_text(relativePath, contents);
    REQUIRE_MESSAGE(written.has_value(), "temp_directory: " << describe_error(written));
    return file(relativePath);
}

std::filesystem::path conjure::testing::extended_length_path(const std::filesystem::path &path) {
    if constexpr (CONJURE_PLATFORM_WINDOWS == 1) {
        // The prefix rule of the module system's always_extended_path (engine/src/modules/long_path.h), which the
        // support library cannot link: any length takes the prefix, a share \\?\UNC\, anything else \\?\. A path
        // given in a literal form is returned as it is, not normalised as the module helpers do; one that
        // absolute() completes to a literal form (as it does "//?/C:/...") is returned normalised, not prefixed.
        const auto hasLiteralPrefix = [](const std::filesystem::path::string_type &text) {
            return text.starts_with(std::filesystem::path(R"(\\?\)").native())
                || text.starts_with(std::filesystem::path(R"(\\.\)").native())
                || text.starts_with(std::filesystem::path(R"(\??\)").native());
        };
        if (path.empty() || hasLiteralPrefix(path.native())) {
            return path;
        }
        std::error_code ignored = {};
        const std::filesystem::path absolute =
          std::filesystem::absolute(path, ignored).lexically_normal().make_preferred();
        const std::filesystem::path::string_type &text = absolute.native();
        if (hasLiteralPrefix(text)) {
            return absolute;
        }
        const std::filesystem::path::string_type share(2, std::filesystem::path::preferred_separator);
        std::filesystem::path::string_type extended = {};
        if (text.starts_with(share)) {
            extended = std::filesystem::path(R"(\\?\UNC\)").native();
            extended.append(text.substr(share.size()));
        } else {
            extended = std::filesystem::path(R"(\\?\)").native();
            extended.append(text);
        }
        return std::filesystem::path(std::move(extended));
    } else {
        return path;
    }
}

std::filesystem::path conjure::testing::long_directory(const temp_directory &directory, usize minimumLength) {
    const std::string segment(40, 'd');
    std::filesystem::path deep = directory.path();
    while (deep.native().size() < minimumLength) {
        deep /= segment;
    }
    std::error_code failure = {};
    std::filesystem::create_directories(extended_length_path(deep), failure);
    REQUIRE_MESSAGE(!failure, "temp_directory: cannot create " << deep.string() << ": " << failure.message());
    return deep;
}

conjure::core::result<std::filesystem::path> conjure::testing::replaying_copy(
  const temp_directory &directory,
  const std::filesystem::path &program,
  std::string_view name,
  std::string_view replay
) {
    std::filesystem::path copy = directory.file(name);
    copy += program.extension();
    std::error_code failure = {};
    std::filesystem::copy_file(program, copy, std::filesystem::copy_options::overwrite_existing, failure);
    if (failure) {
        return conjure::core::make_error(
          conjure::core::error_code::IO_FAILURE,
          "cannot copy {} to {}: {}",
          program.string(),
          copy.string(),
          failure.message()
        );
    }
    // copy_file keeps the permissions with libstdc++ and libc++ alike; set them anyway, so the copy runs wherever
    // the program does.
    const std::filesystem::perms permissions = std::filesystem::status(program, failure).permissions();
    if (!failure) {
        std::filesystem::permissions(copy, permissions, std::filesystem::perm_options::replace, failure);
    }
    if (failure) {
        return conjure::core::make_error(
          conjure::core::error_code::IO_FAILURE,
          "cannot give {} the permissions of {}: {}",
          copy.string(),
          program.string(),
          failure.message()
        );
    }
    const std::u8string fileName = copy.filename().u8string();
    CONJURE_TRY(directory.write_text(std::string(fileName.begin(), fileName.end()) + ".replay", replay));
    return copy;
}
