#include "json.hpp"

#include <cassert>
#include <cmath>
#include <cstring>

namespace
{

void expect_error(const char* text, json::error expected)
{
    char input[2048];
    const std::size_t length = std::strlen(text);
    assert(length < sizeof(input));
    std::memcpy(input, text, length + 1);

    json::document<128, 16> document;
    const auto result = json::parse(input, length, document);

    assert(!result);
    assert(result.code == expected);
}

void test_truncated_structures()
{
    const char* inputs[] = {
        "[",
        "[1",
        "[1,",
        "{",
        "{\"a\"",
        "{\"a\":",
        "{\"a\":1",
        "{\"a\":1,"
    };

    for (const char* input : inputs)
        expect_error(input, json::error::unexpected_end);
}

void test_malformed_separators()
{
    struct test_case
    {
        const char* input;
        json::error error;
    };

    const test_case cases[] = {
        { "[,]", json::error::expected_value },
        { "[1,]", json::error::expected_value },
        { "[1 2]", json::error::expected_comma },
        { "{,}", json::error::unexpected_character },
        { "{\"a\",1}", json::error::expected_colon },
        { "{\"a\":}", json::error::expected_value },
        { "{\"a\":1,}", json::error::unexpected_character },
        { "{\"a\":1 \"b\":2}", json::error::expected_comma }
    };

    for (const test_case& test : cases)
        expect_error(test.input, test.error);
}

void test_malformed_literals()
{
    const char* inputs[] = {
        "n",
        "nu",
        "nul",
        "nulx",
        "t",
        "tr",
        "tru",
        "trux",
        "f",
        "fa",
        "fal",
        "falsx"
    };

    for (const char* input : inputs)
        expect_error(input, json::error::invalid_literal);

    expect_error("nullx", json::error::unexpected_character);
    expect_error("truefalse", json::error::unexpected_character);
}

void test_malformed_numbers()
{
    const char* inputs[] = {
        "-",
        "+1",
        ".1",
        "01",
        "-01",
        "1.",
        "1.e1",
        "1e",
        "1e+",
        "1e-",
        "--1",
        "-+1"
    };

    for (const char* input : inputs)
        expect_error(input, json::error::invalid_number);

    expect_error("1x", json::error::unexpected_character);
    expect_error("1.0x", json::error::unexpected_character);
    expect_error("1e2x", json::error::unexpected_character);
}

void test_string_boundaries()
{
    const char* inputs[] = {
        "\"",
        "\"abc",
        "\"abc\\",
        "\"abc\\\"",
        "\"abc\\q\"",
        "\"abc\\u\"",
        "\"abc\n\""
    };

    for (const char* input : inputs)
    {
        char buffer[64];
        std::strcpy(buffer, input);

        json::document<8, 4> document;
        const auto result = json::parse(buffer, std::strlen(buffer), document);

        assert(!result);
        assert(result.code == json::error::unexpected_end ||
               result.code == json::error::invalid_string);
    }
}

void test_long_tokens()
{
    // Long valid strings must not require storage proportional to string size.
    char string_input[1024];
    string_input[0] = '"';
    for (std::size_t i = 1; i < sizeof(string_input) - 2; ++i)
        string_input[i] = 'x';
    string_input[sizeof(string_input) - 2] = '"';
    string_input[sizeof(string_input) - 1] = '\0';

    json::document<2, 1> string_document;
    const auto string_result =
        json::parse(string_input, sizeof(string_input) - 1, string_document);
    assert(string_result);
    assert(string_document.root().is_string());
    assert(string_document.root().as_string().size == sizeof(string_input) - 3);

    // A very long integer must fail without overflowing the accumulator.
    char integer_input[1024];
    std::memset(integer_input, '9', sizeof(integer_input) - 1);
    integer_input[sizeof(integer_input) - 1] = '\0';
    expect_error(integer_input, json::error::invalid_number);

    // A long fractional token should remain bounded and parse successfully.
    char fraction_input[1024];
    fraction_input[0] = '0';
    fraction_input[1] = '.';
    for (std::size_t i = 2; i < sizeof(fraction_input) - 1; ++i)
        fraction_input[i] = '1';
    fraction_input[sizeof(fraction_input) - 1] = '\0';

    json::document<2, 1> fraction_document;
    const auto fraction_result =
        json::parse(fraction_input, sizeof(fraction_input) - 1, fraction_document);
    assert(fraction_result);
    assert(std::isfinite(fraction_document.root().as_number()));
}

void test_whitespace_and_trailing_data()
{
    char whitespace_input[1024];
    std::memset(whitespace_input, ' ', sizeof(whitespace_input) - 5);
    std::memcpy(whitespace_input + sizeof(whitespace_input) - 5, "null ", 5);

    json::document<2, 1> document;
    const auto result =
        json::parse(whitespace_input, sizeof(whitespace_input), document);
    assert(result);
    assert(document.root().is_null());

    expect_error("null null", json::error::unexpected_character);
    expect_error("[]{}", json::error::unexpected_character);
    expect_error("\"a\"\"b\"", json::error::unexpected_character);
}

void test_reparse_after_failure()
{
    json::document<8, 4> document;

    char invalid[] = "{\"a\":[1,2,}";
    const auto invalid_result =
        json::parse(invalid, std::strlen(invalid), document);
    assert(!invalid_result);

    char valid[] = "{\"answer\":42}";
    const auto valid_result =
        json::parse(valid, std::strlen(valid), document);
    assert(valid_result);
    assert(document.root().is_object());
    assert(document.root().size() == 1);
    assert(document.root()["answer"].as_integer() == 42);
}

} // namespace

int main()
{
    test_truncated_structures();
    test_malformed_separators();
    test_malformed_literals();
    test_malformed_numbers();
    test_string_boundaries();
    test_long_tokens();
    test_whitespace_and_trailing_data();
    test_reparse_after_failure();

    return 0;
}
