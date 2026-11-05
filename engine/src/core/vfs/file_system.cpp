#include "conjure/core/vfs/file_system.h"

#include "core/vfs/vfs_internal.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::vfs;

/*===========================================================================*/
/*===========================================================================*/
// file

file::file() {}

file::~file() {}


/*===========================================================================*/
/*===========================================================================*/
// file_system

file_system::file_system() {}

file_system::~file_system() {}

result<std::string> file_system::read_text_file(std::string_view path) {
    CONJURE_TRY_ASSIGN(const std::vector<std::byte> bytes, read_file(path));

    std::string_view text(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    constexpr std::string_view UTF8_BOM = "\xEF\xBB\xBF";
    if (text.starts_with(UTF8_BOM)) {
        text.remove_prefix(UTF8_BOM.size());
    }
    return std::string(text);
}

result<void> file_system::write_text_file(std::string_view path, std::string_view text) {
    return write_file(path, std::as_bytes(std::span<const char>(text.data(), text.size())));
}

bool file_system::exists(std::string_view path) {
    return stat(path).has_value();
}

result<void> file_system::remove_all(std::string_view path) {
    CONJURE_TRY_ASSIGN(const std::string normalized, internal::resolve_backend_path(path));
    if (normalized.empty()) {
        return make_error(error_code::INVALID_ARGUMENT, "the root of a file system cannot be removed");
    }

    CONJURE_TRY_ASSIGN(const file_stat info, stat(normalized));
    if (!info.isDirectory) {
        return remove(normalized);
    }

    // Collect first: a backend may not tolerate removals while it is enumerating. Deeper entries
    // sort after their parents, so removing in reverse order empties every directory before it
    // is removed itself.
    std::vector<std::string> entries = {};
    const enumerate_callback collect = [&entries](std::string_view entry, const file_stat &) {
        entries.emplace_back(entry);
    };
    CONJURE_TRY(enumerate(normalized, enumerate_flags::RECURSIVE, collect));
    std::sort(entries.begin(), entries.end());
    for (auto iterator = entries.rbegin(); iterator != entries.rend(); ++iterator) {
        CONJURE_TRY(remove(*iterator));
    }
    return remove(normalized);
}

bool file_system::is_read_only() const noexcept {
    return false;
}
