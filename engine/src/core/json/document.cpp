#include "conjure/core/json/document.h"

#include "core/json/json_internal.h"

#include "conjure/core/assert.h"
#include "conjure/core/log.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::json;

/*===========================================================================*/
/*===========================================================================*/
// Internal helpers

namespace {
    std::string path_to_utf8(const std::filesystem::path &path) {
        const std::u8string utf8 = path.u8string();
        return std::string(reinterpret_cast<const char *>(utf8.data()), utf8.size());
    }

    struct sortable_member {
        std::string_view key = {};
        yyjson_mut_val *item = nullptr;
    };

    // Copies a scalar, or creates the empty container that a copy_frame then fills.
    yyjson_mut_val *shallow_copy(yyjson_mut_doc *target, yyjson_mut_val *source) {
        switch (yyjson_mut_get_type(source)) {
            case YYJSON_TYPE_NULL: {
                return yyjson_mut_null(target);
            }
            case YYJSON_TYPE_BOOL: {
                return yyjson_mut_bool(target, yyjson_mut_get_bool(source));
            }
            case YYJSON_TYPE_NUM: {
                if (yyjson_mut_is_uint(source)) {
                    return yyjson_mut_uint(target, yyjson_mut_get_uint(source));
                }
                if (yyjson_mut_is_sint(source)) {
                    return yyjson_mut_sint(target, yyjson_mut_get_sint(source));
                }
                return yyjson_mut_real(target, yyjson_mut_get_real(source));
            }
            case YYJSON_TYPE_STR: {
                return json::detail::make_string(target, json::detail::node_string(source));
            }
            case YYJSON_TYPE_RAW: {
                return yyjson_mut_rawncpy(target, yyjson_mut_get_raw(source), yyjson_mut_get_len(source));
            }
            case YYJSON_TYPE_ARR: {
                return yyjson_mut_arr(target);
            }
            case YYJSON_TYPE_OBJ: {
                return yyjson_mut_obj(target);
            }
            default: {
                return nullptr;
            }
        }
    }

    // One source container whose children are still being copied into 'copy'.
    struct copy_frame {
        yyjson_mut_val *copy = nullptr;
        yyjson_mut_val *cursor = nullptr;               // arrays: next source element
        usize remaining = 0;                            // arrays: elements left
        std::vector<sortable_member> members = {};      // objects: members in key order
        usize next = 0;                                 // objects: index of the next member
        bool isObject = false;
    };

    void begin_copy_frame(std::vector<copy_frame> &stack, yyjson_mut_val *source, yyjson_mut_val *copy) {
        copy_frame frame = {};
        frame.copy = copy;
        if (yyjson_mut_is_arr(source)) {
            frame.cursor = yyjson_mut_arr_get_first(source);
            frame.remaining = yyjson_mut_arr_size(source);
            stack.push_back(std::move(frame));
            return;
        }

        frame.isObject = true;
        frame.members.reserve(yyjson_mut_obj_size(source));
        yyjson_mut_obj_iter iterator = yyjson_mut_obj_iter_with(source);
        while (yyjson_mut_obj_iter_has_next(&iterator)) {
            yyjson_mut_val *key = yyjson_mut_obj_iter_next(&iterator);
            frame.members.push_back({json::detail::node_string(key), yyjson_mut_obj_iter_get_val(key)});
        }

        // Byte order of the UTF-8 keys; stable so duplicate keys keep their relative order.
        std::stable_sort(
          frame.members.begin(),
          frame.members.end(),
          [](const sortable_member &left, const sortable_member &right) {
              return left.key < right.key;
          }
        );
        stack.push_back(std::move(frame));
    }
}

yyjson_mut_val *conjure::core::json::detail::sorted_copy(yyjson_mut_doc *target, yyjson_mut_val *source) {
    yyjson_mut_val *root = shallow_copy(target, source);
    if (root == nullptr || !yyjson_mut_is_ctn(source)) {
        return root;
    }

    // Explicit stack instead of recursion so nesting depth never threatens the call stack.
    std::vector<copy_frame> stack = {};
    begin_copy_frame(stack, source, root);
    while (!stack.empty()) {
        copy_frame &top = stack.back();
        yyjson_mut_val *child = nullptr;
        yyjson_mut_val *key = nullptr;
        if (!top.isObject) {
            if (top.remaining == 0) {
                stack.pop_back();
                continue;
            }
            child = top.cursor;
            top.cursor = child->next;
            --top.remaining;
        } else {
            if (top.next == top.members.size()) {
                stack.pop_back();
                continue;
            }
            const sortable_member &entry = top.members[top.next];
            ++top.next;
            key = make_string(target, entry.key);
            if (key == nullptr) {
                return nullptr;
            }
            child = entry.item;
        }

        yyjson_mut_val *copy = shallow_copy(target, child);
        if (copy == nullptr) {
            return nullptr;
        }
        const bool added = key == nullptr ? yyjson_mut_arr_append(top.copy, copy) :
                                            yyjson_mut_obj_add(top.copy, key, copy);
        if (!added) {
            return nullptr;
        }

        // 'top' may dangle once a frame is pushed; nothing below touches it.
        if (yyjson_mut_is_ctn(child)) {
            begin_copy_frame(stack, child, copy);
        }
    }
    return root;
}

yyjson_mut_val *conjure::core::json::detail::make_real(yyjson_mut_doc *target, f64 number) {
    if (!std::isfinite(number)) {
        CONJURE_ASSERT(false, "json: {} is not representable in JSON; storing null", number);
        return yyjson_mut_null(target);
    }
    return yyjson_mut_real(target, number);
}

bool conjure::core::json::detail::within_nesting_limit(yyjson_val *root, usize limit) {
    if (!yyjson_is_ctn(root)) {
        return true;
    }

    // Only containers add depth, so the explicit stack holds containers waiting to be entered.
    struct pending {
        yyjson_val *container = nullptr;
        usize depth = 0;
    };
    std::vector<pending> stack = {};
    stack.push_back({root, 1});

    while (!stack.empty()) {
        const pending current = stack.back();
        stack.pop_back();
        if (current.depth > limit) {
            return false;
        }

        if (yyjson_is_arr(current.container)) {
            yyjson_arr_iter iterator = yyjson_arr_iter_with(current.container);
            while (yyjson_arr_iter_has_next(&iterator)) {
                yyjson_val *element = yyjson_arr_iter_next(&iterator);
                if (yyjson_is_ctn(element)) {
                    stack.push_back({element, current.depth + 1});
                }
            }
            continue;
        }

        yyjson_obj_iter iterator = yyjson_obj_iter_with(current.container);
        while (yyjson_obj_iter_has_next(&iterator)) {
            yyjson_val *key = yyjson_obj_iter_next(&iterator);
            yyjson_val *item = yyjson_obj_iter_get_val(key);
            if (yyjson_is_ctn(item)) {
                stack.push_back({item, current.depth + 1});
            }
        }
    }
    return true;
}

bool conjure::core::json::detail::is_valid_utf8(std::string_view text) noexcept {
    const usize size = text.size();
    usize index = 0;
    while (index < size) {
        const u8 lead = static_cast<u8>(text[index]);
        if (lead < 0x80) {
            ++index;
            continue;
        }

        // Decode the sequence so overlong forms, surrogates and values past U+10FFFF can be
        // rejected exactly as yyjson's reader rejects them.
        usize length = 0;
        u32 minimum = 0;
        u32 codePoint = 0;
        if ((lead & 0xE0) == 0xC0) {
            length = 2;
            minimum = 0x80;
            codePoint = static_cast<u32>(lead & 0x1F);
        } else if ((lead & 0xF0) == 0xE0) {
            length = 3;
            minimum = 0x800;
            codePoint = static_cast<u32>(lead & 0x0F);
        } else if ((lead & 0xF8) == 0xF0) {
            length = 4;
            minimum = 0x10000;
            codePoint = static_cast<u32>(lead & 0x07);
        } else {
            return false;
        }

        if (size - index < length) {
            return false;
        }
        for (usize offset = 1; offset < length; ++offset) {
            const u8 continuation = static_cast<u8>(text[index + offset]);
            if ((continuation & 0xC0) != 0x80) {
                return false;
            }
            codePoint = (codePoint << 6) | static_cast<u32>(continuation & 0x3F);
        }

        const bool surrogate = codePoint >= 0xD800 && codePoint <= 0xDFFF;
        if (codePoint < minimum || codePoint > 0x10FFFF || surrogate) {
            return false;
        }
        index += length;
    }
    return true;
}

std::string conjure::core::json::detail::write_node(yyjson_mut_val *node, u32 flags) {
    if (node == nullptr) {
        return {};
    }

    yyjson_write_err writeError = {};
    usize length = 0;
    char *text = yyjson_mut_val_write_opts(node, flags, nullptr, &length, &writeError);
    if (text == nullptr) {
        CONJURE_LOG_WARN(LOG_CORE, "json: serialisation failed: {}", writeError.msg == nullptr ? "" : writeError.msg);
        return {};
    }

    std::string output(text, length);
    // yyjson hands out a buffer from the C allocator; releasing it is the only way to give it back.
    std::free(text);
    return output;
}

std::string conjure::core::json::detail::write_canonical(yyjson_mut_val *node) {
    if (node == nullptr) {
        return {};
    }

    yyjson_mut_doc *scratch = yyjson_mut_doc_new(nullptr);
    if (scratch == nullptr) {
        CONJURE_LOG_WARN(LOG_CORE, "json: out of memory while building the canonical form");
        return {};
    }

    yyjson_mut_val *sorted = sorted_copy(scratch, node);
    if (sorted == nullptr) {
        CONJURE_LOG_WARN(LOG_CORE, "json: out of memory while building the canonical form");
        yyjson_mut_doc_free(scratch);
        return {};
    }

    std::string output = write_node(sorted, YYJSON_WRITE_NOFLAG);
    yyjson_mut_doc_free(scratch);
    return output;
}

usize conjure::core::json::detail::format_real(f64 number, char *buffer) noexcept {
    // A stack pool keeps yyjson's single-value writer off the heap: it asks for 34 bytes.
    alignas(16) char pool[256] = {};
    yyjson_alc allocator = {};
    const bool poolReady = yyjson_alc_pool_init(&allocator, pool, sizeof(pool));
    CONJURE_VERIFY(poolReady, "json: stack pool too small for yyjson's allocator");
    if (!poolReady) {
        std::memcpy(buffer, "null", 4);
        return 4;
    }

    yyjson_mut_val node = {};
    yyjson_mut_set_real(&node, number);

    yyjson_write_err writeError = {};
    usize length = 0;
    char *text = yyjson_mut_val_write_opts(&node, YYJSON_WRITE_NOFLAG, &allocator, &length, &writeError);
    if (text == nullptr) {
        std::memcpy(buffer, "null", 4);
        return 4;
    }

    std::memcpy(buffer, text, length);
    allocator.free(allocator.ctx, text);
    return length;
}


/*===========================================================================*/
/*===========================================================================*/
// Constructors and destructors

document::document() :
    mHandle(yyjson_mut_doc_new(nullptr)) {
    yyjson_mut_doc *handle = detail::as_document(mHandle);
    yyjson_mut_doc_set_root(handle, yyjson_mut_null(handle));
}

document::document(void *handle) noexcept :
    mHandle(handle) {}

document::~document() {
    yyjson_mut_doc_free(detail::as_document(mHandle));
}

document::document(const document &other) :
    mHandle(yyjson_mut_doc_mut_copy(detail::as_document(other.mHandle), nullptr)) {}

document &document::operator=(const document &other) {
    if (this == &other) {
        return *this;
    }
    document copy(other);
    std::swap(mHandle, copy.mHandle);
    return *this;
}

document::document(document &&other) noexcept :
    mHandle(std::exchange(other.mHandle, nullptr)) {}

document &document::operator=(document &&other) noexcept {
    if (this == &other) {
        return *this;
    }
    yyjson_mut_doc_free(detail::as_document(mHandle));
    mHandle = std::exchange(other.mHandle, nullptr);
    return *this;
}


/*===========================================================================*/
/*===========================================================================*/
// Construction and parsing

document document::object() {
    document created = {};
    yyjson_mut_set_obj(detail::as_node(created.root_node()));
    return created;
}

document document::array() {
    document created = {};
    yyjson_mut_set_arr(detail::as_node(created.root_node()));
    return created;
}

document document::from_value(const value &source) {
    document created = {};
    yyjson_mut_val *node = detail::as_node(detail::value_access::node(source));
    if (node == nullptr) {
        return created;
    }
    yyjson_mut_doc *handle = detail::as_document(created.mHandle);
    yyjson_mut_val *copy = yyjson_mut_val_mut_copy(handle, node);
    if (copy != nullptr) {
        yyjson_mut_doc_set_root(handle, copy);
    }
    return created;
}

result<document> document::parse(std::string_view text) {
    if (text.empty()) {
        return error(error_code::PARSE_FAILURE, "parse error at line 1, column 1: empty input");
    }

    // yyjson only writes to the buffer with YYJSON_READ_INSITU, which is not requested.
    char *data = const_cast<char *>(text.data());
    yyjson_read_err readError = {};
    yyjson_doc *immutable = yyjson_read_opts(data, text.size(), YYJSON_READ_NOFLAG, nullptr, &readError);
    if (immutable == nullptr) {
        usize line = 1;
        usize column = 1;
        usize character = 0;
        if (!yyjson_locate_pos(text.data(), text.size(), readError.pos, &line, &column, &character)) {
            line = 1;
            column = 1;
        }
        const char *reason = readError.msg == nullptr ? "unknown error" : readError.msg;
        std::string message = std::format("parse error at line {}, column {}: {}", line, column, reason);
        return error(error_code::PARSE_FAILURE, std::move(message));
    }

    // The reader and the immutable-to-mutable copy are iterative, but yyjson's deep copy (copy
    // constructor, from_value, set/push_back of a value) and merge patch recurse once per level;
    // reject what they could not process before it becomes a document.
    if (!detail::within_nesting_limit(yyjson_doc_get_root(immutable), MAX_NESTING_DEPTH)) {
        yyjson_doc_free(immutable);
        std::string message = std::format("parse error: nesting deeper than {} levels", MAX_NESTING_DEPTH);
        return error(error_code::PARSE_FAILURE, std::move(message));
    }

    yyjson_mut_doc *mutableDocument = yyjson_doc_mut_copy(immutable, nullptr);
    yyjson_doc_free(immutable);
    if (mutableDocument == nullptr) {
        return error(error_code::OUT_OF_MEMORY, "out of memory while copying the parsed document");
    }
    return document(static_cast<void *>(mutableDocument));
}

result<document> document::parse_file(const std::filesystem::path &path) {
    // POSIX opens a directory as a stream that reads nothing; check the kind up front so that
    // case is an I/O failure everywhere instead of an "empty input" parse failure.
    std::error_code statusError = {};
    const std::filesystem::file_status fileStatus = std::filesystem::status(path, statusError);
    if (!std::filesystem::exists(fileStatus)) {
        return error(error_code::IO_FAILURE, std::format("cannot open '{}'", path_to_utf8(path)));
    }
    if (!std::filesystem::is_regular_file(fileStatus)) {
        return error(error_code::IO_FAILURE, std::format("'{}' is not a regular file", path_to_utf8(path)));
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        return error(error_code::IO_FAILURE, std::format("cannot open '{}'", path_to_utf8(path)));
    }

    std::string content((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad()) {
        return error(error_code::IO_FAILURE, std::format("cannot read '{}'", path_to_utf8(path)));
    }

    result<document> parsed = parse(content);
    if (!parsed.has_value()) {
        const error &failure = parsed.get_error();
        return error(failure.generic_code(), std::format("{}: {}", path_to_utf8(path), failure.message()));
    }
    return parsed;
}


/*===========================================================================*/
/*===========================================================================*/
// Reading

value document::root() const noexcept {
    return detail::value_access::make(root_node(), this);
}

value document::at_pointer(std::string_view pointer) const noexcept {
    if (pointer.empty()) {
        return root();
    }
    yyjson_mut_val *found = yyjson_mut_doc_ptr_getn(detail::as_document(mHandle), pointer.data(), pointer.size());
    return detail::value_access::make(found, this);
}

bool document::operator==(const document &other) const {
    return root() == other.root();
}


/*===========================================================================*/
/*===========================================================================*/
// Object mutation

void document::set(std::string_view key, bool item) {
    set_node(key, yyjson_mut_bool(detail::as_document(mHandle), item));
}

void document::set(std::string_view key, i32 item) {
    set(key, static_cast<i64>(item));
}

void document::set(std::string_view key, u32 item) {
    set(key, static_cast<u64>(item));
}

void document::set(std::string_view key, i64 item) {
    set_node(key, detail::make_integer(detail::as_document(mHandle), item));
}

void document::set(std::string_view key, u64 item) {
    set_node(key, yyjson_mut_uint(detail::as_document(mHandle), item));
}

void document::set(std::string_view key, f32 item) {
    set(key, static_cast<f64>(item));
}

void document::set(std::string_view key, f64 item) {
    set_node(key, detail::make_real(detail::as_document(mHandle), item));
}

void document::set(std::string_view key, std::string_view item) {
    set_node(key, detail::make_string(detail::as_document(mHandle), item));
}

void document::set(std::string_view key, const char *item) {
    if (item == nullptr) {
        set_null(key);
        return;
    }
    set(key, std::string_view(item));
}

void document::set(std::string_view key, std::nullptr_t) {
    set_null(key);
}

void document::set(std::string_view key, const document &item) {
    set(key, item.root());
}

void document::set(std::string_view key, const value &item) {
    yyjson_mut_val *source = detail::as_node(detail::value_access::node(item));
    if (source == nullptr) {
        CONJURE_ASSERT(false, "json: cannot set '{}' from an invalid value", key);
        return;
    }
    set_node(key, yyjson_mut_val_mut_copy(detail::as_document(mHandle), source));
}

void document::set_null(std::string_view key) {
    set_node(key, yyjson_mut_null(detail::as_document(mHandle)));
}

bool document::erase(std::string_view key) {
    yyjson_mut_val *root = detail::as_node(root_node());
    if (!yyjson_mut_is_obj(root)) {
        return false;
    }
    return yyjson_mut_obj_remove_keyn(root, detail::key_data(key), key.size()) != nullptr;
}


/*===========================================================================*/
/*===========================================================================*/
// Array mutation

void document::push_back(bool item) {
    push_node(yyjson_mut_bool(detail::as_document(mHandle), item));
}

void document::push_back(i32 item) {
    push_back(static_cast<i64>(item));
}

void document::push_back(u32 item) {
    push_back(static_cast<u64>(item));
}

void document::push_back(i64 item) {
    push_node(detail::make_integer(detail::as_document(mHandle), item));
}

void document::push_back(u64 item) {
    push_node(yyjson_mut_uint(detail::as_document(mHandle), item));
}

void document::push_back(f32 item) {
    push_back(static_cast<f64>(item));
}

void document::push_back(f64 item) {
    push_node(detail::make_real(detail::as_document(mHandle), item));
}

void document::push_back(std::string_view item) {
    push_node(detail::make_string(detail::as_document(mHandle), item));
}

void document::push_back(const char *item) {
    if (item == nullptr) {
        push_back(nullptr);
        return;
    }
    push_back(std::string_view(item));
}

void document::push_back(std::nullptr_t) {
    push_node(yyjson_mut_null(detail::as_document(mHandle)));
}

void document::push_back(const document &item) {
    push_back(item.root());
}

void document::push_back(const value &item) {
    yyjson_mut_val *source = detail::as_node(detail::value_access::node(item));
    if (source == nullptr) {
        CONJURE_ASSERT(false, "json: cannot push an invalid value");
        return;
    }
    push_node(yyjson_mut_val_mut_copy(detail::as_document(mHandle), source));
}

bool document::erase(usize index) {
    yyjson_mut_val *root = detail::as_node(root_node());
    if (!yyjson_mut_is_arr(root)) {
        return false;
    }
    return yyjson_mut_arr_remove(root, index) != nullptr;
}

void document::clear() {
    yyjson_mut_val *root = detail::as_node(root_node());
    if (yyjson_mut_is_obj(root)) {
        yyjson_mut_obj_clear(root);
        return;
    }
    if (yyjson_mut_is_arr(root)) {
        yyjson_mut_arr_clear(root);
        return;
    }
    yyjson_mut_set_null(root);
}


/*===========================================================================*/
/*===========================================================================*/
// JSON Pointer mutation

bool document::set_pointer(std::string_view pointer, bool item) {
    return set_pointer_node(pointer, yyjson_mut_bool(detail::as_document(mHandle), item));
}

bool document::set_pointer(std::string_view pointer, i32 item) {
    return set_pointer(pointer, static_cast<i64>(item));
}

bool document::set_pointer(std::string_view pointer, u32 item) {
    return set_pointer(pointer, static_cast<u64>(item));
}

bool document::set_pointer(std::string_view pointer, i64 item) {
    return set_pointer_node(pointer, detail::make_integer(detail::as_document(mHandle), item));
}

bool document::set_pointer(std::string_view pointer, u64 item) {
    return set_pointer_node(pointer, yyjson_mut_uint(detail::as_document(mHandle), item));
}

bool document::set_pointer(std::string_view pointer, f32 item) {
    return set_pointer(pointer, static_cast<f64>(item));
}

bool document::set_pointer(std::string_view pointer, f64 item) {
    return set_pointer_node(pointer, detail::make_real(detail::as_document(mHandle), item));
}

bool document::set_pointer(std::string_view pointer, std::string_view item) {
    return set_pointer_node(pointer, detail::make_string(detail::as_document(mHandle), item));
}

bool document::set_pointer(std::string_view pointer, const char *item) {
    if (item == nullptr) {
        return set_pointer(pointer, nullptr);
    }
    return set_pointer(pointer, std::string_view(item));
}

bool document::set_pointer(std::string_view pointer, std::nullptr_t) {
    return set_pointer_node(pointer, yyjson_mut_null(detail::as_document(mHandle)));
}

bool document::set_pointer(std::string_view pointer, const document &item) {
    return set_pointer(pointer, item.root());
}

bool document::set_pointer(std::string_view pointer, const value &item) {
    yyjson_mut_val *source = detail::as_node(detail::value_access::node(item));
    if (source == nullptr) {
        CONJURE_ASSERT(false, "json: cannot set '{}' from an invalid value", pointer);
        return false;
    }
    return set_pointer_node(pointer, yyjson_mut_val_mut_copy(detail::as_document(mHandle), source));
}


/*===========================================================================*/
/*===========================================================================*/
// Whole-document operations

bool document::merge_patch(const value &patch) {
    yyjson_mut_val *patchNode = detail::as_node(detail::value_access::node(patch));
    if (patchNode == nullptr) {
        CONJURE_ASSERT(false, "json: merge_patch requires a valid patch value");
        return false;
    }

    yyjson_mut_doc *handle = detail::as_document(mHandle);
    if (handle == nullptr) {
        return false;
    }
    yyjson_mut_val *merged = yyjson_mut_merge_patch(handle, detail::as_node(root_node()), patchNode);
    if (merged == nullptr) {
        return false;
    }
    yyjson_mut_doc_set_root(handle, merged);
    return true;
}

bool document::merge_patch(const document &patch) {
    return merge_patch(patch.root());
}

std::string document::to_string(bool pretty) const {
    const u32 flags = pretty ? YYJSON_WRITE_PRETTY : YYJSON_WRITE_NOFLAG;
    return detail::write_node(detail::as_node(root_node()), flags);
}

std::string document::to_canonical_string() const {
    return detail::write_canonical(detail::as_node(root_node()));
}

result<void> document::write_file(const std::filesystem::path &path, bool pretty) const {
    const std::string text = to_string(pretty);
    if (text.empty()) {
        return error(error_code::INVALID_STATE, "document cannot be serialised");
    }

    const std::filesystem::path parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code ignored = {};
        std::filesystem::create_directories(parent, ignored);
    }

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) {
        return error(error_code::IO_FAILURE, std::format("cannot open '{}' for writing", path_to_utf8(path)));
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.flush();
    if (!stream.good()) {
        return error(error_code::IO_FAILURE, std::format("cannot write '{}'", path_to_utf8(path)));
    }
    return result<void>::success();
}


/*===========================================================================*/
/*===========================================================================*/
// Private methods

void *document::root_node() const noexcept {
    return yyjson_mut_doc_get_root(detail::as_document(mHandle));
}

void *document::object_root() {
    yyjson_mut_val *root = detail::as_node(root_node());
    if (yyjson_mut_is_obj(root)) {
        return root;
    }
    if (yyjson_mut_is_null(root)) {
        yyjson_mut_set_obj(root);
        return root;
    }
    CONJURE_ASSERT(false, "json: object mutation requires an object root, found {}", detail::type_of(root));
    return nullptr;
}

void *document::array_root() {
    yyjson_mut_val *root = detail::as_node(root_node());
    if (yyjson_mut_is_arr(root)) {
        return root;
    }
    if (yyjson_mut_is_null(root)) {
        yyjson_mut_set_arr(root);
        return root;
    }
    CONJURE_ASSERT(false, "json: array mutation requires an array root, found {}", detail::type_of(root));
    return nullptr;
}

void document::set_node(std::string_view key, void *node) {
    yyjson_mut_val *object = detail::as_node(object_root());
    yyjson_mut_val *item = detail::as_node(node);
    if (object == nullptr || item == nullptr) {
        return;
    }

    // Replacing in place keeps member order; the probe key only lives for the lookup.
    yyjson_mut_val probe = {};
    yyjson_mut_set_strn(&probe, detail::key_data(key), key.size());
    if (yyjson_mut_obj_replace(object, &probe, item)) {
        return;
    }

    yyjson_mut_val *keyNode = detail::make_string(detail::as_document(mHandle), key);
    if (keyNode == nullptr) {
        return;
    }
    yyjson_mut_obj_add(object, keyNode, item);
}

void document::push_node(void *node) {
    yyjson_mut_val *array = detail::as_node(array_root());
    yyjson_mut_val *item = detail::as_node(node);
    if (array == nullptr || item == nullptr) {
        return;
    }
    yyjson_mut_arr_append(array, item);
}

bool document::set_pointer_node(std::string_view pointer, void *node) {
    yyjson_mut_doc *handle = detail::as_document(mHandle);
    yyjson_mut_val *item = detail::as_node(node);
    if (handle == nullptr || item == nullptr) {
        return false;
    }

    if (pointer.empty()) {
        yyjson_mut_doc_set_root(handle, item);
        return true;
    }

    // A fresh document has a null root; the first pointer write turns it into an object so that
    // "/a/b" works without an explicit document::object().
    yyjson_mut_val *root = detail::as_node(root_node());
    if (yyjson_mut_is_null(root)) {
        yyjson_mut_set_obj(root);
    }

    // An existing target is swapped in place so member order survives; yyjson's own "set" would
    // move the member to the end. A missing target is added with RFC 6902 "add" semantics, which
    // also appends for a "-" token or an index equal to the array size.
    yyjson_ptr_ctx context = {};
    yyjson_ptr_err pointerError = {};
    yyjson_mut_val *existing = yyjson_mut_doc_ptr_getx(handle, pointer.data(), pointer.size(), &context, &pointerError);
    if (existing != nullptr) {
        return yyjson_ptr_ctx_replace(&context, item);
    }
    if (pointerError.code != YYJSON_PTR_ERR_RESOLVE) {
        return false;
    }
    return yyjson_mut_doc_ptr_addn(handle, pointer.data(), pointer.size(), item);
}


/*===========================================================================*/
/*===========================================================================*/
// Builders

init_value::init_value(std::nullptr_t) noexcept :
    mStorage(std::in_place_type<std::nullptr_t>, nullptr) {}

init_value::init_value(bool item) noexcept :
    mStorage(std::in_place_type<bool>, item) {}

init_value::init_value(i32 item) noexcept :
    mStorage(std::in_place_type<i64>, static_cast<i64>(item)) {}

init_value::init_value(u32 item) noexcept :
    mStorage(std::in_place_type<u64>, static_cast<u64>(item)) {}

init_value::init_value(i64 item) noexcept :
    mStorage(std::in_place_type<i64>, item) {}

init_value::init_value(u64 item) noexcept :
    mStorage(std::in_place_type<u64>, item) {}

init_value::init_value(f32 item) noexcept :
    mStorage(std::in_place_type<f64>, static_cast<f64>(item)) {}

init_value::init_value(f64 item) noexcept :
    mStorage(std::in_place_type<f64>, item) {}

init_value::init_value(const char *item) :
    mStorage(
      item == nullptr ? storage(std::in_place_type<std::nullptr_t>, nullptr) :
                        storage(std::in_place_type<std::string>, item)
    ) {}

init_value::init_value(std::string_view item) :
    mStorage(std::in_place_type<std::string>, item) {}

init_value::init_value(const std::string &item) :
    mStorage(std::in_place_type<std::string>, item) {}

init_value::init_value(std::string &&item) noexcept :
    mStorage(std::in_place_type<std::string>, std::move(item)) {}

init_value::init_value(const document &item) :
    mStorage(std::in_place_type<document>, item) {}

init_value::init_value(document &&item) noexcept :
    mStorage(std::in_place_type<document>, std::move(item)) {}

init_value::init_value(const value &item) noexcept :
    mStorage(std::in_place_type<value>, item) {}

init_value::~init_value() {}

document init_value::to_document() const {
    document created = {};
    yyjson_mut_doc *handle = detail::as_document(created.mHandle);
    yyjson_mut_val *node = detail::as_node(make_node(created.mHandle));
    if (node != nullptr) {
        yyjson_mut_doc_set_root(handle, node);
    }
    return created;
}

void *init_value::make_node(void *target) const {
    yyjson_mut_doc *handle = detail::as_document(target);
    switch (mStorage.index()) {
        case 0: {
            return yyjson_mut_null(handle);
        }
        case 1: {
            return yyjson_mut_bool(handle, std::get<bool>(mStorage));
        }
        case 2: {
            return detail::make_integer(handle, std::get<i64>(mStorage));
        }
        case 3: {
            return yyjson_mut_uint(handle, std::get<u64>(mStorage));
        }
        case 4: {
            return detail::make_real(handle, std::get<f64>(mStorage));
        }
        case 5: {
            return detail::make_string(handle, std::get<std::string>(mStorage));
        }
        case 6: {
            // A moved-from document has no root; like an invalid value it contributes null.
            yyjson_mut_val *source = detail::as_node(std::get<document>(mStorage).root_node());
            if (source == nullptr) {
                return yyjson_mut_null(handle);
            }
            return yyjson_mut_val_mut_copy(handle, source);
        }
        default: {
            yyjson_mut_val *source = detail::as_node(detail::value_access::node(std::get<value>(mStorage)));
            if (source == nullptr) {
                return yyjson_mut_null(handle);
            }
            return yyjson_mut_val_mut_copy(handle, source);
        }
    }
}

document conjure::core::json::object(std::initializer_list<init_member> members) {
    document created = document::object();
    for (const init_member &entry : members) {
        created.set_node(entry.key, entry.item.make_node(created.mHandle));
    }
    return created;
}

document conjure::core::json::array(std::initializer_list<init_value> items) {
    document created = document::array();
    for (const init_value &item : items) {
        created.push_node(item.make_node(created.mHandle));
    }
    return created;
}
