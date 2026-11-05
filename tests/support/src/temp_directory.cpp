#include "conjure/testing/temp_directory.h"

#include "conjure/testing/doctest.h"
#include "conjure/testing/test_environment.h"

#include "conjure/core/error.h"
#include "conjure/core/uuid.h"

#include <fstream>
#include <string>
#include <system_error>

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

    std::filesystem::path create_unique_directory(std::string_view prefix) {
        std::string name = conjure::core::uuid::generate().to_string();
        if (!prefix.empty()) {
            name = std::string(prefix) + "_" + name;
        }

        const std::filesystem::path path = temporary_root() / name;
        std::error_code failure = {};
        std::filesystem::create_directories(path, failure);
        REQUIRE_MESSAGE(!failure, "temp_directory: cannot create " << path.string() << ": " << failure.message());
        return path;
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
    std::error_code failure = {};
    std::filesystem::remove_all(mPath, failure);
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
    std::filesystem::create_directories(fullPath.parent_path(), failure);
    if (failure) {
        return conjure::core::make_error(
          conjure::core::error_code::IO_FAILURE,
          "cannot create {}: {}",
          fullPath.parent_path().string(),
          failure.message()
        );
    }

    std::ofstream stream(fullPath, std::ios::binary | std::ios::trunc);
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
