/*
 * json_parser - A small, bounded JSON parser for C++14
 *
 * Copyright (c) 2026 Jim Wilson
 *
 * This software is provided as-is, without warranty of any kind.
 */

#ifndef JSON_HPP
#define JSON_HPP

#include <cstddef>
#include <cstdint>
#include <limits>

namespace json {

/**
 * @brief Non-owning reference to a string in the input buffer.
 *
 * String contents are decoded in-place by the parser. The referenced
 * storage is owned by the caller and must remain valid while the
 * reference is used.
 */
struct string_ref
{
    const char* data;
    std::size_t size;

    const char* begin() const { return data; }
    const char* end() const { return data + size; }
};

/** @brief JSON value types supported by the parser. */
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

/** @brief Errors that can be reported by the parser. */
enum class error : std::uint8_t
{
    none,
    unexpected_end,
    unexpected_character,
    invalid_string,
    invalid_number,
    invalid_literal,
    expected_colon,
    expected_comma,
    expected_value,
    nesting_limit,
    capacity_exceeded
};

/**
 * @brief Result returned by a parse operation.
 *
 * On failure, @c code identifies the error and @c offset identifies
 * its position in the input buffer.
 */
struct parse_result
{
    error code = error::none;
    std::size_t offset = 0;

    explicit operator bool() const
    {
        return code == error::none;
    }
};

/**
 * @brief Fixed-capacity parsed JSON document.
 *
 * @tparam MaxValues Maximum number of JSON values stored in the document.
 * @tparam MaxDepth Maximum object/array nesting depth.
 *
 * The document owns no dynamic memory. String values refer directly to
 * the caller-provided input buffer.
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
     * A value does not own the underlying JSON data. It remains valid only
     * while the associated document and its input buffer remain valid.
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
        value() : document_(nullptr), index_(invalid_index) {}

        /** @brief Return whether this handle refers to a parsed value. */
        explicit operator bool() const
        {
            return document_ != nullptr && index_ != invalid_index;
        }

        value_type type() const { return node_ref().type; }

        bool is_null() const { return type() == value_type::null; }
        bool is_boolean() const { return type() == value_type::boolean; }
        bool is_integer() const { return type() == value_type::integer; }
        bool is_number() const
        {
            return type() == value_type::integer || type() == value_type::number;
        }
        bool is_string() const { return type() == value_type::string; }
        bool is_array() const { return type() == value_type::array; }
        bool is_object() const { return type() == value_type::object; }

        bool as_boolean() const { return node_ref().data.boolean; }
        std::int64_t as_integer() const { return node_ref().data.integer; }
        double as_number() const
        {
            return type() == value_type::integer
                ? static_cast<double>(node_ref().data.integer)
                : node_ref().data.number;
        }
        string_ref as_string() const { return node_ref().data.string; }

        std::size_t size() const
        {
            std::size_t result = 0;
            for (index_type i = node_ref().first_child; i != invalid_index;
                 i = document_->values_[i].next_sibling)
                ++result;
            return result;
        }

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

        template <std::size_t N>
        value operator[](const char(&key)[N]) const
        {
            return (*this)[string_ref{ key, N - 1 }];
        }

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

    document() : values_{}, value_count_(0) {}

    /** @brief Remove all values from the document. */
    void clear()
    {
        value_count_ = 0;
    }

    /** @brief Return the number of values currently stored. */
    std::size_t value_count() const { return value_count_; }

    /** @brief Return a handle to the root value, or an invalid value if empty. */
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
     * The buffer must be non-null, even when size is zero, and must remain
     * valid while the parsed document or any string_ref is used.
     */
    parser(char* buffer, std::size_t size, document_type& document)
        : begin_(buffer), current_(buffer), end_(buffer + size), document_(document)
    {
    }

    /**
     * @brief Parse the complete input buffer.
     *
     * The document is cleared before parsing. On failure it may contain a
     * partial parse and must not be used as a valid JSON document.
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
        if (current_ != end_ && *current_ == '.')
        {
            floating = true;
            ++current_;
            if (current_ == end_ || *current_ < '0' || *current_ > '9')
            {
                last_error_ = fail(error::invalid_number);
                return invalid_index;
            }
            while (current_ != end_ && *current_ >= '0' && *current_ <= '9')
                ++current_;
        }

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

        double value = 0.0;
        std::size_t significant_digits = 0;
        std::size_t fractional_digits = 0;
        int decimal_exponent = exponent_negative ? -exponent : exponent;

        char* p = integer_start;
        while (p != current_ && *p != '.' && *p != 'e' && *p != 'E')
        {
            if (significant_digits < 17)
            {
                value = value * 10.0 + static_cast<double>(*p - '0');
                ++significant_digits;
            }
            else
            {
                ++decimal_exponent;
            }
            ++p;
        }

        if (p != current_ && *p == '.')
        {
            ++p;
            while (p != current_ && *p >= '0' && *p <= '9')
            {
                ++fractional_digits;
                if (significant_digits < 17)
                {
                    value = value * 10.0 + static_cast<double>(*p - '0');
                    ++significant_digits;
                }
                ++p;
            }
        }

        if (fractional_digits > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            last_error_ = fail(error::invalid_number);
            return invalid_index;
        }

        decimal_exponent -= static_cast<int>(fractional_digits);

        if (value == 0.0)
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

        double power = 10.0;
        int magnitude_exponent = decimal_exponent < 0
            ? -decimal_exponent
            : decimal_exponent;
        while (magnitude_exponent != 0)
        {
            if ((magnitude_exponent & 1) != 0)
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
parse_result parse(char* buffer, std::size_t size, document<MaxValues, MaxDepth>& document)
{
    parser<MaxValues, MaxDepth> parser_instance(buffer, size, document);
    return parser_instance.parse();
}

} // namespace json

#endif // JSON_HPP
