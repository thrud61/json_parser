/*
 * json_parser - A small, bounded JSON parser for C++14
 *
 * Copyright (c) 2026 Jim Wilson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef JSON_HPP
#define JSON_HPP

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace json {

/**
 * @brief Non-owning reference to a string in the input buffer.
 *
 * The parser decodes JSON string escapes in-place in the caller-provided
 * input buffer. A string_ref therefore contains a pointer and length rather
 * than owning a copy of the string.
 *
 * @note The referenced storage is owned by the caller and must remain valid
 * and unchanged for the lifetime of the reference. The input buffer must
 * also remain valid for as long as any parsed string is accessed.
 */
struct string_ref
{
    const char* data;
    std::size_t size;

    /** @brief Return an iterator to the first character. */
    const char* begin() const { return data; }

    /** @brief Return an iterator one past the last character. */
    const char* end() const { return data + size; }
};

/**
 * @brief JSON value types represented by the parser.
 *
 * JSON integers are represented separately from non-integer numbers so that
 * integral values within the signed 64-bit range can be accessed without a
 * floating-point conversion.
 */
enum class value_type : std::uint8_t
{
    null,
    boolean,
    integer,
    number,
    string,
    array,
    object
};

/**
 * @brief Errors that can be reported by the parser.
 *
 * The reported offset is zero-based and identifies the parser position at
 * which the error was detected. For errors detected after consuming input,
 * such as a capacity failure, the offset can therefore be after the last
 * consumed character.
 */
enum class error : std::uint8_t
{
    none,                 /**< Parsing completed successfully. */
    unexpected_end,       /**< The input ended before a complete value was found. */
    unexpected_character, /**< A character was encountered where it is not valid. */
    invalid_string,       /**< The string contains an invalid escape or control character. */
    invalid_number,       /**< The number does not follow JSON number syntax or cannot be represented. */
    invalid_literal,      /**< A JSON literal such as null, true or false is malformed. */
    expected_colon,       /**< An object member was not followed by ':'. */
    expected_comma,       /**< An array element or object member was not followed by ','. */
    expected_value,       /**< A JSON value was expected but none was present. */
    nesting_limit,        /**< MaxDepth was exceeded. */
    capacity_exceeded     /**< MaxValues was exceeded. */
};

/**
 * @brief Result returned by a parse operation.
 *
 * A successful result has @c code equal to @c error::none and can be tested
 * directly in a boolean context. On failure, @c code identifies the error
 * and @c offset gives its zero-based position in the input buffer.
 */
struct parse_result
{
    error code = error::none;
    std::size_t offset = 0;

    /** @brief Return true when parsing completed successfully. */
    explicit operator bool() const
    {
        return code == error::none;
    }
};

/**
 * @brief Fixed-capacity parsed JSON document.
 *
 * @tparam MaxValues Maximum number of value nodes stored by the document.
 * @tparam MaxDepth Maximum object/array nesting depth accepted by the parser.
 *
 * The document uses storage contained entirely within the object and performs
 * no dynamic allocation. The parsed tree is valid only for the lifetime of
 * the document and its caller-owned input buffer.
 *
 * Each JSON value, including an empty object or array, consumes one value
 * slot. Object member names are stored as non-owning string references and
 * therefore do not require a separate allocation or copy.
 *
 * @note MaxValues is limited to 65535 because value indexes are stored as
 * 16-bit unsigned integers.
 */
template <std::size_t MaxValues, std::size_t MaxDepth>
class document
{
    static_assert(MaxValues > 0, "MaxValues must be greater than zero");
    static_assert(MaxValues <= 65535, "MaxValues must fit in a 16-bit index");
    static_assert(MaxDepth > 0, "MaxDepth must be greater than zero");

    using index_type = std::uint16_t;

    struct node
    {
        value_type type;
        index_type first_child;
        index_type next_sibling;
        string_ref key;

        union data
        {
            bool boolean;
            std::int64_t integer;
            double number;
            string_ref string;

            data() : integer(0) {}
        } data;
    };

    static constexpr index_type invalid_index = 0xffffu;

    node values_[MaxValues];
    std::size_t value_count_;

public:
    /**
     * @brief Lightweight handle used to access a value in a document.
     *
     * A value does not own the underlying JSON data. It refers to a node in
     * its associated document and remains usable only while that document
     * and its input buffer remain valid.
     *
     * A default-constructed value, a missing object member, or an out-of-range
     * array access produces an invalid value. Test validity with
     * @c operator bool() before accessing the value.
     *
     * @warning Accessors other than @c operator bool() require a valid value.
     */
    class value
    {
        friend class document;

        const document* document_;
        index_type index_;

        value(const document* document, index_type index)
            : document_(document), index_(index)
        {
        }

        const node& node_ref() const
        {
            return document_->values_[index_];
        }

    public:
        /** @brief Construct an invalid value handle. */
        value() : document_(nullptr), index_(invalid_index) {}

        /** @brief Return whether this handle refers to a valid parsed value. */
        explicit operator bool() const
        {
            return document_ != nullptr && index_ != invalid_index;
        }

        /** @brief Return the JSON type of this value. */
        value_type type() const { return node_ref().type; }

        /** @brief Return true if this value is JSON null. */
        bool is_null() const { return type() == value_type::null; }

        /** @brief Return true if this value is a JSON boolean. */
        bool is_boolean() const { return type() == value_type::boolean; }

        /** @brief Return true if this value is an integer. */
        bool is_integer() const { return type() == value_type::integer; }

        /**
         * @brief Return true if this value is a JSON number.
         *
         * Both integer and non-integer numeric values return true.
         */
        bool is_number() const
        {
            return type() == value_type::integer || type() == value_type::number;
        }

        /** @brief Return true if this value is a JSON string. */
        bool is_string() const { return type() == value_type::string; }

        /** @brief Return true if this value is a JSON array. */
        bool is_array() const { return type() == value_type::array; }

        /** @brief Return true if this value is a JSON object. */
        bool is_object() const { return type() == value_type::object; }

        /**
         * @brief Return the boolean value.
         * @pre The value is valid and @c is_boolean() is true.
         */
        bool as_boolean() const { return node_ref().data.boolean; }

        /**
         * @brief Return the signed 64-bit integer value.
         * @pre The value is valid and @c is_integer() is true.
         */
        std::int64_t as_integer() const { return node_ref().data.integer; }

        /**
         * @brief Return the numeric value as a double.
         *
         * Integer values are converted to double when accessed through this
         * function. Such a conversion can lose integer precision for values
         * outside the exact range of double.
         *
         * @pre The value is valid and @c is_number() is true.
         */
        double as_number() const
        {
            return type() == value_type::integer
                ? static_cast<double>(node_ref().data.integer)
                : node_ref().data.number;
        }

        /**
         * @brief Return a non-owning reference to the string value.
         *
         * The returned string_ref points into the caller-owned input buffer.
         *
         * @pre The value is valid and @c is_string() is true.
         */
        string_ref as_string() const { return node_ref().data.string; }

        /**
         * @brief Return the number of immediate child values.
         *
         * For an array this is the number of elements. For an object this is
         * the number of members. Scalar values return zero.
         *
         * @pre The value is valid.
         */
        std::size_t size() const
        {
            std::size_t result = 0;
            for (index_type i = node_ref().first_child; i != invalid_index;
                 i = document_->values_[i].next_sibling)
                ++result;
            return result;
        }

        /**
         * @brief Return an array element by zero-based position.
         *
         * Returns an invalid value if @c position is outside the array's
         * element range.
         *
         * @pre The value is valid and represents an array.
         */
        value operator[](std::size_t position) const
        {
            index_type i = node_ref().first_child;
            while (i != invalid_index && position != 0)
            {
                i = document_->values_[i].next_sibling;
                --position;
            }
            return i == invalid_index ? value() : value(document_, i);
        }

        /**
         * @brief Return an object member by a string literal key.
         *
         * Returns an invalid value when the key is not present. This overload
         * accepts a character array so that a numeric zero cannot be confused
         * with a null pointer.
         *
         * If duplicate member names are present, the first matching member is
         * returned.
         *
         * @pre The value is valid and represents an object.
         */
        template <std::size_t N>
        value operator[](const char(&key)[N]) const
        {
            return (*this)[string_ref{ key, N - 1 }];
        }

        /**
         * @brief Return an object member by a non-owning string reference.
         *
         * Returns an invalid value when the key is not present. If duplicate
         * member names are present, the first matching member is returned.
         *
         * @pre The value is valid and represents an object.
         */
        value operator[](string_ref key) const
        {
            for (index_type i = node_ref().first_child; i != invalid_index;
                 i = document_->values_[i].next_sibling)
            {
                const string_ref candidate = document_->values_[i].key;
                if (candidate.size == key.size)
                {
                    bool equal = true;
                    for (std::size_t n = 0; n < key.size; ++n)
                    {
                        if (candidate.data[n] != key.data[n])
                        {
                            equal = false;
                            break;
                        }
                    }
                    if (equal)
                        return value(document_, i);
                }
            }
            return value();
        }
    };

    /** @brief Construct an empty document with its fixed storage available. */
    document() : values_{}, value_count_(0) {}

    /**
     * @brief Remove all parsed values from the document.
     *
     * This resets the document to the same logical state as a newly
     * constructed document. It does not modify the caller-owned input buffer.
     * Any value handles previously obtained from the document should be
     * considered invalid after this call.
     */
    void clear()
    {
        value_count_ = 0;
    }

    /**
     * @brief Return the number of value nodes currently stored.
     *
     * This includes container nodes and all their descendants. Object member
     * names do not consume additional persistent value slots.
     */
    std::size_t value_count() const { return value_count_; }

    /**
     * @brief Return the root value of the document.
     *
     * Returns an invalid value when the document is empty, such as before a
     * successful parse.
     */
    value root() const
    {
        return value_count_ == 0 ? value() : value(this, 0);
    }

private:
    template <std::size_t, std::size_t>
    friend class parser;

    index_type add(value_type type)
    {
        if (value_count_ == MaxValues)
            return invalid_index;

        const index_type index = static_cast<index_type>(value_count_++);
        node& result = values_[index];
        result.type = type;
        result.first_child = invalid_index;
        result.next_sibling = invalid_index;
        result.key = string_ref{nullptr, 0};
        return index;
    }
};

template <std::size_t MaxValues, std::size_t MaxDepth>
class parser;

/**
 * @brief Parser for a fixed-capacity JSON document.
 *
 * The parser uses the caller-provided writable input buffer for in-place
 * string decoding and performs no dynamic allocation.
 *
 * Parsing is recursive. @c MaxDepth therefore bounds both JSON nesting and
 * the parser's recursion depth; very large values of @c MaxDepth increase
 * stack usage accordingly.
 *
 * The parser accepts standard JSON whitespace and syntax. Unicode escape
 * sequences of the form @c \\uXXXX are deliberately not supported; raw bytes
 * in strings are preserved, but their encoding is not validated.
 */
template <std::size_t MaxValues, std::size_t MaxDepth>
class parser
{
    using document_type = document<MaxValues, MaxDepth>;
    using index_type = typename document_type::index_type;
    static constexpr index_type invalid_index = document_type::invalid_index;

    char* begin_;
    char* current_;
    char* end_;
    document_type& document_;

public:
    /**
     * @brief Construct a parser for a caller-owned writable input buffer.
     *
     * @param buffer Start of the input buffer.
     * @param size Number of bytes in the input buffer.
     * @param document Document into which the parsed values are stored.
     *
     * The buffer must be non-null, even when @p size is zero, and must be
     * writable because JSON string escapes are decoded in-place. The buffer
     * and document must remain valid while parsed values and string_ref
     * objects are used.
     */
    parser(char* buffer, std::size_t size, document_type& document)
        : begin_(buffer), current_(buffer), end_(buffer + size), document_(document)
    {
    }

    /**
     * @brief Parse the complete input buffer.
     *
     * The document is cleared before parsing. Leading and trailing JSON
     * whitespace is accepted, but the complete non-whitespace input must form
     * exactly one JSON value.
     *
     * On success the document contains the parsed tree and the returned
     * parse_result converts to true. On failure the result identifies the
     * error and its zero-based input offset. The document may contain a
     * partial parse after failure and must not be used as a valid JSON
     * document until a subsequent parse succeeds.
     *
     * @return The parse result.
     */
    parse_result parse()
    {
        document_.clear();
        skip_whitespace();

        if (current_ == end_)
            return fail(error::unexpected_end);

        index_type root = parse_value(0);
        if (root == invalid_index)
            return last_error_;

        skip_whitespace();
        if (current_ != end_)
            return fail(error::unexpected_character);

        return parse_result{};
    }

private:
    parse_result last_error_{};

    parse_result fail(error code)
    {
        last_error_.code = code;
        last_error_.offset = static_cast<std::size_t>(current_ - begin_);
        return last_error_;
    }

    void skip_whitespace()
    {
        while (current_ != end_ &&
               (*current_ == ' ' || *current_ == '\t' ||
                *current_ == '\n' || *current_ == '\r'))
            ++current_;
    }

    index_type parse_value(std::size_t depth)
    {
        skip_whitespace();
        if (current_ == end_)
        {
            last_error_ = fail(error::unexpected_end);
            return invalid_index;
        }

        switch (*current_)
        {
        case 'n': return parse_literal("null", value_type::null);
        case 't': return parse_literal("true", value_type::boolean, true);
        case 'f': return parse_literal("false", value_type::boolean, false);
        case '"': return parse_string();
        case '[': return parse_array(depth);
        case '{': return parse_object(depth);
        default:
            if (*current_ == '-' || (*current_ >= '0' && *current_ <= '9'))
                return parse_number();
            last_error_ = fail(error::expected_value);
            return invalid_index;
        }
    }

    index_type parse_literal(const char* literal, value_type type, bool boolean = false)
    {
        const char* p = literal;
        char* start = current_;
        while (*p != '\0')
        {
            if (current_ == end_ || *current_ != *p)
            {
                current_ = start;
                last_error_ = fail(error::invalid_literal);
                return invalid_index;
            }
            ++current_;
            ++p;
        }

        index_type index = document_.add(type);
        if (index == invalid_index)
        {
            last_error_ = fail(error::capacity_exceeded);
            return invalid_index;
        }
        if (type == value_type::boolean)
            document_.values_[index].data.boolean = boolean;
        return index;
    }

    index_type parse_string()
    {
        ++current_;
        char* output = current_;
        char* start = output;

        while (current_ != end_)
        {
            const char c = *current_++;
            if (c == '"')
            {
                index_type index = document_.add(value_type::string);
                if (index == invalid_index)
                {
                    last_error_ = fail(error::capacity_exceeded);
                    return invalid_index;
                }
                document_.values_[index].data.string =
                    string_ref{start, static_cast<std::size_t>(output - start)};
                return index;
            }

            if (static_cast<unsigned char>(c) < 0x20)
            {
                last_error_ = fail(error::invalid_string);
                return invalid_index;
            }

            if (c != '\\')
            {
                *output++ = c;
                continue;
            }

            if (current_ == end_)
            {
                last_error_ = fail(error::unexpected_end);
                return invalid_index;
            }

            const char escape = *current_++;
            switch (escape)
            {
            case '"': *output++ = '"'; break;
            case '\\': *output++ = '\\'; break;
            case '/': *output++ = '/'; break;
            case 'b': *output++ = '\b'; break;
            case 'f': *output++ = '\f'; break;
            case 'n': *output++ = '\n'; break;
            case 'r': *output++ = '\r'; break;
            case 't': *output++ = '\t'; break;
            case 'u':
                last_error_ = fail(error::invalid_string);
                return invalid_index;
            default:
                last_error_ = fail(error::invalid_string);
                return invalid_index;
            }
        }

        last_error_ = fail(error::unexpected_end);
        return invalid_index;
    }

    index_type parse_number()
    {
        char* integer_start = current_;
        bool negative = false;

        if (current_ != end_ && *current_ == '-')
        {
            negative = true;
            ++current_;
            integer_start = current_;
        }

        if (current_ == end_)
        {
            last_error_ = fail(error::invalid_number);
            return invalid_index;
        }

        if (*current_ == '0')
        {
            ++current_;
            if (current_ != end_ && *current_ >= '0' && *current_ <= '9')
            {
                last_error_ = fail(error::invalid_number);
                return invalid_index;
            }
        }
        else if (*current_ >= '1' && *current_ <= '9')
        {
            while (current_ != end_ && *current_ >= '0' && *current_ <= '9')
                ++current_;
        }
        else
        {
            last_error_ = fail(error::invalid_number);
            return invalid_index;
        }

        bool floating = false;
        char* decimal_point = nullptr;
        if (current_ != end_ && *current_ == '.')
        {
            floating = true;
            decimal_point = current_;
            ++current_;
            if (current_ == end_ || *current_ < '0' || *current_ > '9')
            {
                last_error_ = fail(error::invalid_number);
                return invalid_index;
            }
            while (current_ != end_ && *current_ >= '0' && *current_ <= '9')
                ++current_;
        }

        char* mantissa_end = current_;
        int exponent = 0;
        bool exponent_negative = false;
        if (current_ != end_ && (*current_ == 'e' || *current_ == 'E'))
        {
            floating = true;
            ++current_;
            if (current_ != end_ && (*current_ == '+' || *current_ == '-'))
            {
                exponent_negative = *current_ == '-';
                ++current_;
            }
            if (current_ == end_ || *current_ < '0' || *current_ > '9')
            {
                last_error_ = fail(error::invalid_number);
                return invalid_index;
            }

            while (current_ != end_ && *current_ >= '0' && *current_ <= '9')
            {
                const int digit = *current_ - '0';
                if (exponent < 10000)
                {
                    exponent = exponent * 10 + digit;
                    if (exponent > 10000)
                        exponent = 10000;
                }
                ++current_;
            }
        }

        if (!floating)
        {
            const std::uint64_t max_integer =
                static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
            const std::uint64_t limit = negative ? max_integer + 1u : max_integer;

            std::uint64_t magnitude = 0;
            for (char* p = integer_start; p != current_; ++p)
            {
                const std::uint64_t digit = static_cast<unsigned>(*p - '0');
                if (magnitude > (limit - digit) / 10u)
                {
                    last_error_ = fail(error::invalid_number);
                    return invalid_index;
                }
                magnitude = magnitude * 10u + digit;
            }

            index_type index = document_.add(value_type::integer);
            if (index == invalid_index)
            {
                last_error_ = fail(error::capacity_exceeded);
                return invalid_index;
            }

            document_.values_[index].data.integer =
                negative
                    ? (magnitude == max_integer + 1u
                        ? std::numeric_limits<std::int64_t>::min()
                        : -static_cast<std::int64_t>(magnitude))
                    : static_cast<std::int64_t>(magnitude);

            return index;
        }

        std::uint64_t significand = 0;
        std::size_t significant_digits = 0;
        std::size_t first_nonzero_position = 0;
        std::size_t digit_position = 0;
        int rounding_digit = -1;
        bool nonzero_seen = false;

        for (char* p = integer_start; p != mantissa_end; ++p)
        {
            if (*p == '.')
                continue;

            const unsigned digit = static_cast<unsigned>(*p - '0');
            if (!nonzero_seen)
            {
                if (digit == 0)
                {
                    ++digit_position;
                    continue;
                }
                nonzero_seen = true;
                first_nonzero_position = digit_position;
            }

            if (significant_digits < 17)
            {
                significand = significand * 10u + digit;
                ++significant_digits;
            }
            else if (rounding_digit < 0)
            {
                rounding_digit = static_cast<int>(digit);
            }

            ++digit_position;
        }

        if (!nonzero_seen)
        {
            index_type index = document_.add(value_type::number);
            if (index == invalid_index)
            {
                last_error_ = fail(error::capacity_exceeded);
                return invalid_index;
            }
            document_.values_[index].data.number = negative ? -0.0 : 0.0;
            return index;
        }

        if (rounding_digit >= 5)
            ++significand;

        std::int64_t decimal_exponent = exponent_negative
            ? -static_cast<std::int64_t>(exponent)
            : static_cast<std::int64_t>(exponent);

        const std::size_t integer_digits = decimal_point == nullptr
            ? static_cast<std::size_t>(mantissa_end - integer_start)
            : static_cast<std::size_t>(decimal_point - integer_start);

        const std::size_t max_exponent =
            static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max());
        if (integer_digits > max_exponent ||
            first_nonzero_position > max_exponent ||
            significant_digits > max_exponent)
        {
            last_error_ = fail(error::invalid_number);
            return invalid_index;
        }

        const auto add_exponent = [&](std::size_t amount) -> bool
        {
            if (amount > max_exponent)
                return false;
            if (decimal_exponent > 0 &&
                amount > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max() - decimal_exponent))
                return false;
            decimal_exponent += static_cast<std::int64_t>(amount);
            return true;
        };

        const auto subtract_exponent = [&](std::size_t amount) -> bool
        {
            if (amount > max_exponent)
                return false;
            if (decimal_exponent < 0 &&
                amount > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max() + decimal_exponent))
                return false;
            decimal_exponent -= static_cast<std::int64_t>(amount);
            return true;
        };

        if (!add_exponent(integer_digits) ||
            !subtract_exponent(first_nonzero_position) ||
            !subtract_exponent(significant_digits))
        {
            last_error_ = fail(error::invalid_number);
            return invalid_index;
        }

        if (significand >= 100000000000000000ULL)
        {
            significand /= 10u;
            ++decimal_exponent;
        }

        if (decimal_exponent > 10000)
            decimal_exponent = 10000;
        else if (decimal_exponent < -10000)
            decimal_exponent = -10000;

        double value = static_cast<double>(significand);
        double power = 10.0;
        std::uint64_t magnitude_exponent = decimal_exponent < 0
            ? static_cast<std::uint64_t>(-decimal_exponent)
            : static_cast<std::uint64_t>(decimal_exponent);
        while (magnitude_exponent != 0)
        {
            if ((magnitude_exponent & 1u) != 0)
            {
                if (decimal_exponent < 0)
                    value /= power;
                else
                    value *= power;
            }
            magnitude_exponent >>= 1;
            if (magnitude_exponent != 0)
                power *= power;
        }

        if (!std::isfinite(value))
        {
            last_error_ = fail(error::invalid_number);
            return invalid_index;
        }

        if (negative)
            value = -value;

        index_type index = document_.add(value_type::number);
        if (index == invalid_index)
        {
            last_error_ = fail(error::capacity_exceeded);
            return invalid_index;
        }

        document_.values_[index].data.number = value;
        return index;
    }

    index_type parse_array(std::size_t depth)
    {
        if (depth >= MaxDepth)
        {
            last_error_ = fail(error::nesting_limit);
            return invalid_index;
        }

        ++current_;
        index_type parent = document_.add(value_type::array);
        if (parent == invalid_index)
        {
            last_error_ = fail(error::capacity_exceeded);
            return invalid_index;
        }

        skip_whitespace();
        if (current_ != end_ && *current_ == ']')
        {
            ++current_;
            return parent;
        }

        index_type last = invalid_index;
        while (true)
        {
            index_type child = parse_value(depth + 1);
            if (child == invalid_index)
                return invalid_index;

            if (last == invalid_index)
                document_.values_[parent].first_child = child;
            else
                document_.values_[last].next_sibling = child;
            last = child;

            skip_whitespace();
            if (current_ == end_)
            {
                last_error_ = fail(error::unexpected_end);
                return invalid_index;
            }
            if (*current_ == ']')
            {
                ++current_;
                return parent;
            }
            if (*current_ != ',')
            {
                last_error_ = fail(error::expected_comma);
                return invalid_index;
            }
            ++current_;
        }
    }

    index_type parse_object(std::size_t depth)
    {
        if (depth >= MaxDepth)
        {
            last_error_ = fail(error::nesting_limit);
            return invalid_index;
        }

        ++current_;
        index_type parent = document_.add(value_type::object);
        if (parent == invalid_index)
        {
            last_error_ = fail(error::capacity_exceeded);
            return invalid_index;
        }

        skip_whitespace();
        if (current_ != end_ && *current_ == '}')
        {
            ++current_;
            return parent;
        }

        index_type last = invalid_index;
        while (true)
        {
            skip_whitespace();
            if (current_ == end_ || *current_ != '"')
            {
                last_error_ = fail(error::unexpected_character);
                return invalid_index;
            }

            index_type key = parse_string();
            if (key == invalid_index)
                return invalid_index;

            string_ref key_ref = document_.values_[key].data.string;
            --document_.value_count_;

            skip_whitespace();
            if (current_ == end_ || *current_ != ':')
            {
                last_error_ = fail(error::expected_colon);
                return invalid_index;
            }
            ++current_;

            index_type child = parse_value(depth + 1);
            if (child == invalid_index)
                return invalid_index;
            document_.values_[child].key = key_ref;

            if (last == invalid_index)
                document_.values_[parent].first_child = child;
            else
                document_.values_[last].next_sibling = child;
            last = child;

            skip_whitespace();
            if (current_ == end_)
            {
                last_error_ = fail(error::unexpected_end);
                return invalid_index;
            }
            if (*current_ == '}')
            {
                ++current_;
                return parent;
            }
            if (*current_ != ',')
            {
                last_error_ = fail(error::expected_comma);
                return invalid_index;
            }
            ++current_;
        }
    }
};

template <std::size_t MaxValues, std::size_t MaxDepth>
/**
 * @brief Parse JSON from a caller-owned writable buffer.
 *
 * @tparam MaxValues Maximum number of value nodes stored in the document.
 * @tparam MaxDepth Maximum object/array nesting depth.
 * @param buffer Writable input buffer containing one complete JSON value.
 * @param size Number of bytes in @p buffer.
 * @param document Destination document whose fixed-capacity storage receives
 * the parsed values.
 * @return A parse_result that converts to true on success, or identifies the
 * error and zero-based input offset on failure.
 *
 * The input buffer is modified while strings are decoded. It must remain
 * valid while the resulting document, values and string_ref objects are used.
 * The buffer must be non-null even when @p size is zero.
 *
 * The document is cleared before parsing. On failure it may contain a partial
 * parse and must not be treated as valid until a later parse succeeds.
 */
parse_result parse(char* buffer, std::size_t size, document<MaxValues, MaxDepth>& document)
{
    parser<MaxValues, MaxDepth> parser_instance(buffer, size, document);
    return parser_instance.parse();
}

} // namespace json

#endif // JSON_HPP
