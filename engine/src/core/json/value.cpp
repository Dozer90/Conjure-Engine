#include "conjure/core/json/value.h"

#include "core/json/json_internal.h"

#include <cmath>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using namespace conjure;
using namespace conjure::core;
using namespace conjure::core::json;

/*===========================================================================*/
/*===========================================================================*/
// Free functions

std::string_view conjure::core::json::to_string(json_type type) noexcept {
    switch (type) {
        case json_type::NULL_VALUE: {
            return "null";
        }
        case json_type::BOOL: {
            return "bool";
        }
        case json_type::NUMBER: {
            return "number";
        }
        case json_type::STRING: {
            return "string";
        }
        case json_type::ARRAY: {
            return "array";
        }
        case json_type::OBJECT: {
            return "object";
        }
    }
    return "unknown";
}

json_type conjure::core::json::detail::type_of(yyjson_mut_val *node) noexcept {
    switch (yyjson_mut_get_type(node)) {
        case YYJSON_TYPE_BOOL: {
            return json_type::BOOL;
        }
        case YYJSON_TYPE_NUM: {
            return json_type::NUMBER;
        }
        case YYJSON_TYPE_STR: {
            return json_type::STRING;
        }
        case YYJSON_TYPE_ARR: {
            return json_type::ARRAY;
        }
        case YYJSON_TYPE_OBJ: {
            return json_type::OBJECT;
        }
        default: {
            return json_type::NULL_VALUE;
        }
    }
}


/*===========================================================================*/
/*===========================================================================*/
// Conversion helpers

namespace {
    yyjson_mut_val *node_of(const value &source) noexcept {
        return json::detail::as_node(json::detail::value_access::node(source));
    }

    error missing_error() {
        return error(error_code::NOT_FOUND, "value is missing");
    }

    error type_error(std::string_view expected, json_type found) {
        return error(error_code::INVALID_ARGUMENT, std::format("expected {}, found {}", expected, to_string(found)));
    }

    error range_error(std::string_view number, std::string_view target) {
        return error(error_code::OUT_OF_RANGE, std::format("{} is out of range for {}", number, target));
    }

    // True when the double is a whole number that a 64-bit integer type could hold.
    bool is_integral_real(f64 number) noexcept {
        return std::isfinite(number) && std::trunc(number) == number;
    }

    // True when both objects list the same keys in the same order; the common case, which lets
    // the members be compared pairwise.
    bool same_key_sequence(yyjson_mut_val *lhs, yyjson_mut_val *rhs) noexcept {
        yyjson_mut_obj_iter left = yyjson_mut_obj_iter_with(lhs);
        yyjson_mut_obj_iter right = yyjson_mut_obj_iter_with(rhs);
        while (yyjson_mut_obj_iter_has_next(&left)) {
            yyjson_mut_val *leftKey = yyjson_mut_obj_iter_next(&left);
            yyjson_mut_val *rightKey = yyjson_mut_obj_iter_next(&right);
            if (json::detail::node_string(leftKey) != json::detail::node_string(rightKey)) {
                return false;
            }
        }
        return true;
    }

    // The value of the 'occurrence'-th member named 'key' (0-based), or nullptr.
    yyjson_mut_val *nth_member(yyjson_mut_val *object, std::string_view key, usize occurrence) noexcept {
        usize seen = 0;
        yyjson_mut_obj_iter iterator = yyjson_mut_obj_iter_with(object);
        while (yyjson_mut_obj_iter_has_next(&iterator)) {
            yyjson_mut_val *candidate = yyjson_mut_obj_iter_next(&iterator);
            if (json::detail::node_string(candidate) != key) {
                continue;
            }
            if (seen == occurrence) {
                return yyjson_mut_obj_iter_get_val(candidate);
            }
            ++seen;
        }
        return nullptr;
    }

    // How many of the first 'position' keys of 'object' equal 'key': the occurrence index of the
    // member at 'position'.
    usize earlier_occurrences(yyjson_mut_val *object, std::string_view key, usize position) noexcept {
        usize occurrence = 0;
        usize scanned = 0;
        yyjson_mut_obj_iter iterator = yyjson_mut_obj_iter_with(object);
        while (scanned < position && yyjson_mut_obj_iter_has_next(&iterator)) {
            if (json::detail::node_string(yyjson_mut_obj_iter_next(&iterator)) == key) {
                ++occurrence;
            }
            ++scanned;
        }
        return occurrence;
    }

    // One pair of containers whose children are still being compared. Elements and members live
    // on circular lists (key -> value -> next key), so a cursor plus a count walks them.
    struct compare_frame {
        yyjson_mut_val *lhs = nullptr;          // left container; scanned for occurrence counts
        yyjson_mut_val *rhs = nullptr;          // right container; looked up by key on the slow path
        yyjson_mut_val *leftCursor = nullptr;   // next left element, or next left key
        yyjson_mut_val *rightCursor = nullptr;  // next right element or key (lockstep only)
        usize remaining = 0;
        usize position = 0;                     // index of leftCursor within the left object
        bool isObject = false;
        bool lockstep = true;                   // same key sequence: compare members pairwise
    };

    // Checks a pair of values; containers of equal size are pushed so their children follow.
    bool enter(yyjson_mut_val *lhs, yyjson_mut_val *rhs, std::vector<compare_frame> &stack) {
        const yyjson_type type = yyjson_mut_get_type(lhs);
        if (type != yyjson_mut_get_type(rhs)) {
            return false;
        }

        if (type == YYJSON_TYPE_ARR) {
            if (yyjson_mut_arr_size(lhs) != yyjson_mut_arr_size(rhs)) {
                return false;
            }
            compare_frame frame = {};
            frame.lhs = lhs;
            frame.rhs = rhs;
            frame.leftCursor = yyjson_mut_arr_get_first(lhs);
            frame.rightCursor = yyjson_mut_arr_get_first(rhs);
            frame.remaining = yyjson_mut_arr_size(lhs);
            stack.push_back(frame);
            return true;
        }

        if (type == YYJSON_TYPE_OBJ) {
            if (yyjson_mut_obj_size(lhs) != yyjson_mut_obj_size(rhs)) {
                return false;
            }
            yyjson_mut_obj_iter left = yyjson_mut_obj_iter_with(lhs);
            yyjson_mut_obj_iter right = yyjson_mut_obj_iter_with(rhs);
            compare_frame frame = {};
            frame.lhs = lhs;
            frame.rhs = rhs;
            frame.leftCursor = yyjson_mut_obj_iter_next(&left);
            frame.rightCursor = yyjson_mut_obj_iter_next(&right);
            frame.remaining = yyjson_mut_obj_size(lhs);
            frame.isObject = true;
            frame.lockstep = same_key_sequence(lhs, rhs);
            stack.push_back(frame);
            return true;
        }

        return yyjson_mut_equals(lhs, rhs);
    }

    // yyjson's own equality looks members up by key, which misjudges objects with duplicate keys.
    // Objects are equal when their key-sorted member lists are equal pairwise (the canonical
    // text). Sorting stably pairs the n-th occurrence of a key on one side with the n-th on the
    // other, so matching every left member against that occurrence on the right gives the same
    // answer without sorting: equal sizes make the injective mapping a bijection. The walk uses
    // an explicit stack so nesting depth never threatens the call stack.
    bool structurally_equal(yyjson_mut_val *lhs, yyjson_mut_val *rhs) {
        std::vector<compare_frame> stack = {};
        if (!enter(lhs, rhs, stack)) {
            return false;
        }

        while (!stack.empty()) {
            compare_frame &top = stack.back();
            if (top.remaining == 0) {
                stack.pop_back();
                continue;
            }
            --top.remaining;

            yyjson_mut_val *leftItem = nullptr;
            yyjson_mut_val *rightItem = nullptr;
            if (!top.isObject) {
                leftItem = top.leftCursor;
                rightItem = top.rightCursor;
                top.leftCursor = leftItem->next;
                top.rightCursor = rightItem->next;
            } else if (top.lockstep) {
                leftItem = top.leftCursor->next;
                rightItem = top.rightCursor->next;
                top.leftCursor = leftItem->next;
                top.rightCursor = rightItem->next;
            } else {
                yyjson_mut_val *leftKey = top.leftCursor;
                const std::string_view key = json::detail::node_string(leftKey);
                leftItem = leftKey->next;
                top.leftCursor = leftItem->next;
                rightItem = nth_member(top.rhs, key, earlier_occurrences(top.lhs, key, top.position));
                ++top.position;
                if (rightItem == nullptr) {
                    return false;
                }
            }

            // 'top' may dangle once enter() pushes; nothing below touches it.
            if (!enter(leftItem, rightItem, stack)) {
                return false;
            }
        }
        return true;
    }

    template <typename Target>
    result<Target> convert_integer(const value &source, std::string_view targetName) {
        yyjson_mut_val *node = node_of(source);
        if (node == nullptr) {
            return missing_error();
        }
        if (!yyjson_mut_is_num(node)) {
            return type_error("integer", source.type());
        }

        if (yyjson_mut_is_uint(node)) {
            const u64 number = yyjson_mut_get_uint(node);
            if (!std::in_range<Target>(number)) {
                return range_error(std::format("{}", number), targetName);
            }
            return static_cast<Target>(number);
        }

        if (yyjson_mut_is_sint(node)) {
            const i64 number = yyjson_mut_get_sint(node);
            if (!std::in_range<Target>(number)) {
                return range_error(std::format("{}", number), targetName);
            }
            return static_cast<Target>(number);
        }

        // A real is accepted when it is a whole number inside the target range. The upper bound is
        // exclusive because casting the maximum of a 64-bit type to double rounds up to 2^63/2^64.
        const f64 number = yyjson_mut_get_real(node);
        if (!is_integral_real(number)) {
            return error(error_code::INVALID_ARGUMENT, std::format("expected integer, found real {}", number));
        }
        const f64 lowest = static_cast<f64>(std::numeric_limits<Target>::min());
        const f64 pastHighest = static_cast<f64>(std::numeric_limits<Target>::max()) + 1.0;
        if (number < lowest || number >= pastHighest) {
            return range_error(std::format("{}", number), targetName);
        }
        return static_cast<Target>(number);
    }
}


/*===========================================================================*/
/*===========================================================================*/
// value

json_type value::type() const noexcept {
    return detail::type_of(detail::as_node(mNode));
}

bool value::is_null() const noexcept {
    return mNode == nullptr || yyjson_mut_is_null(detail::as_node(mNode));
}

bool value::is_bool() const noexcept {
    return yyjson_mut_is_bool(detail::as_node(mNode));
}

bool value::is_number() const noexcept {
    return yyjson_mut_is_num(detail::as_node(mNode));
}

bool value::is_string() const noexcept {
    return yyjson_mut_is_str(detail::as_node(mNode));
}

bool value::is_array() const noexcept {
    return yyjson_mut_is_arr(detail::as_node(mNode));
}

bool value::is_object() const noexcept {
    return yyjson_mut_is_obj(detail::as_node(mNode));
}

bool value::is_integer() const noexcept {
    return yyjson_mut_is_int(detail::as_node(mNode));
}

bool value::as_bool(bool fallback) const noexcept {
    yyjson_mut_val *node = detail::as_node(mNode);
    if (!yyjson_mut_is_bool(node)) {
        return fallback;
    }
    return yyjson_mut_get_bool(node);
}

i64 value::as_i64(i64 fallback) const noexcept {
    yyjson_mut_val *node = detail::as_node(mNode);
    if (yyjson_mut_is_sint(node)) {
        return yyjson_mut_get_sint(node);
    }
    if (yyjson_mut_is_uint(node)) {
        const u64 number = yyjson_mut_get_uint(node);
        if (!std::in_range<i64>(number)) {
            return fallback;
        }
        return static_cast<i64>(number);
    }
    if (yyjson_mut_is_real(node)) {
        const f64 number = yyjson_mut_get_real(node);
        if (!is_integral_real(number) || number < -9223372036854775808.0 || number >= 9223372036854775808.0) {
            return fallback;
        }
        return static_cast<i64>(number);
    }
    return fallback;
}

u64 value::as_u64(u64 fallback) const noexcept {
    yyjson_mut_val *node = detail::as_node(mNode);
    if (yyjson_mut_is_uint(node)) {
        return yyjson_mut_get_uint(node);
    }
    if (yyjson_mut_is_sint(node)) {
        const i64 number = yyjson_mut_get_sint(node);
        if (number < 0) {
            return fallback;
        }
        return static_cast<u64>(number);
    }
    if (yyjson_mut_is_real(node)) {
        const f64 number = yyjson_mut_get_real(node);
        if (!is_integral_real(number) || number < 0.0 || number >= 18446744073709551616.0) {
            return fallback;
        }
        return static_cast<u64>(number);
    }
    return fallback;
}

f64 value::as_f64(f64 fallback) const noexcept {
    yyjson_mut_val *node = detail::as_node(mNode);
    if (!yyjson_mut_is_num(node)) {
        return fallback;
    }
    return yyjson_mut_get_num(node);
}

f32 value::as_f32(f32 fallback) const noexcept {
    yyjson_mut_val *node = detail::as_node(mNode);
    if (!yyjson_mut_is_num(node)) {
        return fallback;
    }
    // Narrowing a double outside the float range is undefined behaviour, not a saturation.
    const f64 number = yyjson_mut_get_num(node);
    if (std::fabs(number) > static_cast<f64>(std::numeric_limits<f32>::max())) {
        return fallback;
    }
    return static_cast<f32>(number);
}

std::string_view value::as_string(std::string_view fallback) const noexcept {
    yyjson_mut_val *node = detail::as_node(mNode);
    if (!yyjson_mut_is_str(node)) {
        return fallback;
    }
    return detail::node_string(node);
}

usize value::size() const noexcept {
    yyjson_mut_val *node = detail::as_node(mNode);
    if (yyjson_mut_is_arr(node)) {
        return yyjson_mut_arr_size(node);
    }
    if (yyjson_mut_is_obj(node)) {
        return yyjson_mut_obj_size(node);
    }
    return 0;
}

value value::operator[](usize index) const noexcept {
    yyjson_mut_val *node = detail::as_node(mNode);
    if (!yyjson_mut_is_arr(node)) {
        return value();
    }
    return value(yyjson_mut_arr_get(node, index), mOwner);
}

value value::operator[](std::string_view key) const noexcept {
    yyjson_mut_val *node = detail::as_node(mNode);
    if (!yyjson_mut_is_obj(node)) {
        return value();
    }
    return value(yyjson_mut_obj_getn(node, detail::key_data(key), key.size()), mOwner);
}

bool value::contains(std::string_view key) const noexcept {
    return (*this)[key].is_valid();
}

std::optional<value> value::get(std::string_view key) const noexcept {
    const value found = (*this)[key];
    if (!found.is_valid()) {
        return std::nullopt;
    }
    return found;
}

value::array_iterator value::begin() const noexcept {
    yyjson_mut_val *node = detail::as_node(mNode);
    const usize count = yyjson_mut_arr_size(node);
    if (count == 0) {
        return array_iterator();
    }
    return array_iterator(yyjson_mut_arr_get_first(node), count, mOwner);
}

value::array_iterator value::end() const noexcept {
    return array_iterator();
}

value::object_range value::members() const noexcept {
    yyjson_mut_val *node = detail::as_node(mNode);
    const usize count = yyjson_mut_obj_size(node);
    if (count == 0) {
        return object_range();
    }
    yyjson_mut_obj_iter iterator = yyjson_mut_obj_iter_with(node);
    return object_range(object_iterator(yyjson_mut_obj_iter_next(&iterator), count, mOwner));
}

bool value::operator==(const value &other) const {
    if (mNode == nullptr || other.mNode == nullptr) {
        return mNode == other.mNode;
    }
    return structurally_equal(detail::as_node(mNode), detail::as_node(other.mNode));
}


/*===========================================================================*/
/*===========================================================================*/
// Iterators

value value::array_iterator::operator*() const noexcept {
    return value(mNode, mOwner);
}

value::array_iterator &value::array_iterator::operator++() noexcept {
    if (mRemaining == 0) {
        return *this;
    }
    --mRemaining;
    // Elements form a circular singly linked list; the count stops us before wrapping around.
    mNode = mRemaining == 0 ? nullptr : detail::as_node(mNode)->next;
    return *this;
}

value::array_iterator value::array_iterator::operator++(int) noexcept {
    const array_iterator previous = *this;
    ++(*this);
    return previous;
}

member value::object_iterator::operator*() const noexcept {
    yyjson_mut_val *key = detail::as_node(mKey);
    return member{detail::node_string(key), value(key->next, mOwner)};
}

value::object_iterator &value::object_iterator::operator++() noexcept {
    if (mRemaining == 0) {
        return *this;
    }
    --mRemaining;
    // Keys and values alternate on one circular list: key -> value -> next key.
    mKey = mRemaining == 0 ? nullptr : detail::as_node(mKey)->next->next;
    return *this;
}

value::object_iterator value::object_iterator::operator++(int) noexcept {
    const object_iterator previous = *this;
    ++(*this);
    return previous;
}


/*===========================================================================*/
/*===========================================================================*/
// Built-in converters

result<bool> json_converter<bool>::from_json(const value &source) {
    yyjson_mut_val *node = node_of(source);
    if (node == nullptr) {
        return missing_error();
    }
    if (!yyjson_mut_is_bool(node)) {
        return type_error("bool", source.type());
    }
    return yyjson_mut_get_bool(node);
}

result<i8> json_converter<i8>::from_json(const value &source) {
    return convert_integer<i8>(source, "i8");
}

result<i16> json_converter<i16>::from_json(const value &source) {
    return convert_integer<i16>(source, "i16");
}

result<i32> json_converter<i32>::from_json(const value &source) {
    return convert_integer<i32>(source, "i32");
}

result<i64> json_converter<i64>::from_json(const value &source) {
    return convert_integer<i64>(source, "i64");
}

result<u8> json_converter<u8>::from_json(const value &source) {
    return convert_integer<u8>(source, "u8");
}

result<u16> json_converter<u16>::from_json(const value &source) {
    return convert_integer<u16>(source, "u16");
}

result<u32> json_converter<u32>::from_json(const value &source) {
    return convert_integer<u32>(source, "u32");
}

result<u64> json_converter<u64>::from_json(const value &source) {
    return convert_integer<u64>(source, "u64");
}

result<f32> json_converter<f32>::from_json(const value &source) {
    yyjson_mut_val *node = node_of(source);
    if (node == nullptr) {
        return missing_error();
    }
    if (!yyjson_mut_is_num(node)) {
        return type_error("number", source.type());
    }
    const f64 number = yyjson_mut_get_num(node);
    if (std::fabs(number) > static_cast<f64>(std::numeric_limits<f32>::max())) {
        return range_error(std::format("{}", number), "f32");
    }
    return static_cast<f32>(number);
}

result<f64> json_converter<f64>::from_json(const value &source) {
    yyjson_mut_val *node = node_of(source);
    if (node == nullptr) {
        return missing_error();
    }
    if (!yyjson_mut_is_num(node)) {
        return type_error("number", source.type());
    }
    return yyjson_mut_get_num(node);
}

result<std::string> json_converter<std::string>::from_json(const value &source) {
    yyjson_mut_val *node = node_of(source);
    if (node == nullptr) {
        return missing_error();
    }
    if (!yyjson_mut_is_str(node)) {
        return type_error("string", source.type());
    }
    return std::string(detail::node_string(node));
}

result<uuid> json_converter<uuid>::from_json(const value &source) {
    yyjson_mut_val *node = node_of(source);
    if (node == nullptr) {
        return missing_error();
    }
    if (!yyjson_mut_is_str(node)) {
        return type_error("uuid string", source.type());
    }
    const std::string_view text = detail::node_string(node);
    const std::optional<uuid> parsed = uuid::parse(text);
    if (!parsed.has_value()) {
        return error(error_code::PARSE_FAILURE, std::format("'{}' is not a uuid", text));
    }
    return *parsed;
}
