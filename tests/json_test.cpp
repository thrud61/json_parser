#include "json.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{

int tests_run = 0;

void report_pass(const char* name)
{
    ++tests_run;
    std::printf("PASS: %s\n", name);
}

bool equal(json::string_ref value, const char* expected)
{
    const std::size_t length = std::strlen(expected);

    return value.size == length &&
           std::memcmp(value.data, expected, length) == 0;
}

void test_cmake_settings()
{
    char input[] = R"json({
  "configurations": [
    {
      "name": "KSA-MSVC",
      "generator": "Ninja",
      "configurationType": "Debug",
      "inheritEnvironments": [ "msvc_x64_x64" ],
      "buildRoot": "${projectDir}\\out\\build\\${name}",
      "installRoot": "${projectDir}\\out\\install\\${name}",
      "variables": [
        {
          "name": "USE_LOG_TYPE",
          "value": "DualLogging",
          "type": "STRING"
        },
        {
          "name": "USE_TCP",
          "value": "true",
          "type": "STRING"
        },
        {
          "name": "CONTAINER_ID",
          "value": "TestApp",
          "type": "STRING"
        }
      ]
    },
    {
      "name": "KSA-VxWorks",
      "generator": "Ninja",
      "configurationType": "Debug",
      "cmakeToolchain": "C:/WindRiver/vxworks/24.03/toolchain.cmake",
      "variables": [
        {
          "name": "WIND_HOME",
          "value": "C:/WindRiver",
          "type": "STRING"
        },
        {
          "name": "VXWORKS",
          "value": "True",
          "type": "BOOL"
        }
      ]
    }
  ]
})json";

    json::document<64, 8> document;
    const auto result = json::parse(input, std::strlen(input), document);

    assert(result);
    assert(result.code == json::error::none);

    const auto root = document.root();
    assert(root.is_object());
    assert(root.size() == 1);

    const auto configurations = root["configurations"];
    assert(configurations.is_array());
    assert(configurations.size() == 2);

    const auto msvc = configurations[std::size_t{ 0 }];
    assert(msvc.is_object());
    assert(equal(msvc["name"].as_string(), "KSA-MSVC"));
    assert(equal(msvc["generator"].as_string(), "Ninja"));
    assert(equal(msvc["configurationType"].as_string(), "Debug"));

    const auto environments = msvc["inheritEnvironments"];
    assert(environments.is_array());
    assert(environments.size() == 1);
    assert(equal(environments[0].as_string(), "msvc_x64_x64"));

    assert(equal(
        msvc["buildRoot"].as_string(),
        "${projectDir}\\out\\build\\${name}"
    ));

    const auto variables = msvc["variables"];
    assert(variables.is_array());
    assert(variables.size() == 3);
    assert(equal(variables[0]["name"].as_string(), "USE_LOG_TYPE"));
    assert(equal(variables[0]["value"].as_string(), "DualLogging"));
    assert(equal(variables[2]["name"].as_string(), "CONTAINER_ID"));
    assert(equal(variables[2]["value"].as_string(), "TestApp"));

    const auto vxworks = configurations[1];
    assert(equal(vxworks["name"].as_string(), "KSA-VxWorks"));
    assert(equal(
        vxworks["cmakeToolchain"].as_string(),
        "C:/WindRiver/vxworks/24.03/toolchain.cmake"
    ));

    const auto vx_variables = vxworks["variables"];
    assert(vx_variables.is_array());
    assert(vx_variables.size() == 2);
    assert(equal(vx_variables[0]["name"].as_string(), "WIND_HOME"));
    assert(equal(vx_variables[1]["name"].as_string(), "VXWORKS"));
    assert(equal(vx_variables[1]["value"].as_string(), "True"));

    report_pass("CMakeSettings parsing, nested lookup and in-place strings");
}

void test_numeric_values()
{
    char input[] = R"json({
        "zero": 0,
        "positive": 123456789,
        "negative": -987654321,
        "max": 9223372036854775807,
        "min": -9223372036854775808,
        "fraction": 123.456,
        "negative_fraction": -0.25,
        "exponent": 1.5e3,
        "negative_exponent": 2.5E-2
    })json";

    json::document<32, 4> document;
    const auto result = json::parse(input, std::strlen(input), document);
    assert(result);

    const auto root = document.root();
    assert(root["zero"].is_integer());
    assert(root["zero"].as_integer() == 0);
    assert(root["positive"].is_integer());
    assert(root["positive"].as_integer() == 123456789);
    assert(root["negative"].is_integer());
    assert(root["negative"].as_integer() == -987654321);
    assert(root["max"].is_integer());
    assert(root["max"].as_integer() == 9223372036854775807LL);
    assert(root["min"].is_integer());
    assert(root["min"].as_integer() == (-9223372036854775807LL - 1));
    assert(root["fraction"].is_number());
    assert(root["fraction"].as_number() == 123.456);
    assert(root["negative_fraction"].as_number() == -0.25);
    assert(root["exponent"].as_number() == 1500.0);
    assert(root["negative_exponent"].as_number() == 0.025);

    report_pass("valid integers, int64 boundaries, fractions and exponents");
}

void test_invalid_numbers()
{
    const char* inputs[] = {
        "01", "-", "1.", "1e", "1e+",
        "9223372036854775808", "-9223372036854775809"
    };

    for (const char* text : inputs)
    {
        char input[64];
        std::strcpy(input, text);
        json::document<8, 2> document;
        const auto result = json::parse(input, std::strlen(input), document);
        assert(!result);
        assert(result.code == json::error::invalid_number);
    }

    report_pass("invalid number syntax and int64 overflow (7 cases)");
}

void test_numeric_edges()
{
    {
        char input[] = "1e309";
        json::document<4, 4> document;
        const auto result = json::parse(input, sizeof(input) - 1, document);
        assert(!result);
        assert(result.code == json::error::invalid_number);
    }

    {
        char input[] = "-1e309";
        json::document<4, 4> document;
        const auto result = json::parse(input, sizeof(input) - 1, document);
        assert(!result);
        assert(result.code == json::error::invalid_number);
    }

    {
        char input[] = "1e-10000";
        json::document<4, 4> document;
        const auto result = json::parse(input, sizeof(input) - 1, document);
        assert(result);
        assert(std::isfinite(document.root().as_number()));
    }

    {
        char input[] = "0.00000000000000001";
        json::document<4, 4> document;
        const auto result = json::parse(input, sizeof(input) - 1, document);
        assert(result);
        assert(document.root().as_number() == 1e-17);
    }

    {
        char input[] = "1.0000000000000000001";
        json::document<4, 4> document;
        const auto result = json::parse(input, sizeof(input) - 1, document);
        assert(result);
        assert(document.root().as_number() == 1.0);
    }

    {
        char input[] = "0.12345678901234567";
        json::document<4, 4> document;
        const auto result = json::parse(input, sizeof(input) - 1, document);
        assert(result);
        assert(std::abs(document.root().as_number() - 0.12345678901234567) < 1e-15);
    }

    {
        char input[] = "12345678901234567.89";
        json::document<4, 4> document;
        const auto result = json::parse(input, sizeof(input) - 1, document);
        assert(result);
        assert(document.root().as_number() == 12345678901234568.0);
    }

    report_pass("floating-point overflow, extreme exponents and precision boundaries (7 cases)");
}

void test_capacity_limits()
{
    char exact_input[] = "[1, 2, 3]";
    json::document<4, 1> exact_document;
    const auto exact_result = json::parse(exact_input, std::strlen(exact_input), exact_document);
    assert(exact_result);
    assert(exact_document.value_count() == 4);
    assert(exact_document.root().size() == 3);

    char over_input[] = "[1, 2, 3, 4]";
    json::document<4, 1> over_document;
    const auto over_result = json::parse(over_input, std::strlen(over_input), over_document);
    assert(!over_result);
    assert(over_result.code == json::error::capacity_exceeded);

    char empty_input[] = "{}";
    json::document<1, 1> empty_document;
    const auto empty_result = json::parse(empty_input, std::strlen(empty_input), empty_document);
    assert(empty_result);
    assert(empty_document.value_count() == 1);
    assert(empty_document.root().is_object());
    assert(empty_document.root().size() == 0);

    report_pass("value capacity boundaries and empty-container storage");
}

void test_depth_limits()
{
    char exact_input[] = "[0]";
    json::document<2, 1> exact_document;
    const auto exact_result = json::parse(exact_input, std::strlen(exact_input), exact_document);
    assert(exact_result);
    assert(exact_document.root().is_array());
    assert(exact_document.root().size() == 1);

    char over_input[] = "[[0]]";
    json::document<3, 1> over_document;
    const auto over_result = json::parse(over_input, std::strlen(over_input), over_document);
    assert(!over_result);
    assert(over_result.code == json::error::nesting_limit);

    report_pass("maximum nesting boundary");
}

void test_string_escapes()
{
    char input[] = R"json({
        "quote": "say \"hello\"",
        "backslash": "C:\\temp\\file.txt",
        "slash": "a\/b",
        "controls": "\b\f\n\r\t",
        "empty": "",
        "punctuation": " !@#$%^&*()[]{}:;,?"
    })json";

    json::document<16, 4> document;
    const auto result = json::parse(input, std::strlen(input), document);
    assert(result);

    const auto root = document.root();
    assert(equal(root["quote"].as_string(), "say \"hello\""));
    assert(equal(root["backslash"].as_string(), "C:\\temp\\file.txt"));
    assert(equal(root["slash"].as_string(), "a/b"));
    assert(equal(root["controls"].as_string(), "\b\f\n\r\t"));
    assert(equal(root["empty"].as_string(), ""));
    assert(equal(root["punctuation"].as_string(), " !@#$%^&*()[]{}:;,?"));

    report_pass("JSON string escapes, empty strings and punctuation");
}

void test_invalid_strings()
{
    const char* inputs[] = {
        R"json("bad\q")json",
        R"json("bad\u1234")json",
        R"json("unterminated)json",
        "\"line\nfeed\""
    };

    for (const char* text : inputs)
    {
        char input[64];
        std::strcpy(input, text);
        json::document<8, 2> document;
        const auto result = json::parse(input, std::strlen(input), document);
        assert(!result);
        assert(result.code == json::error::invalid_string ||
               result.code == json::error::unexpected_end);
    }

    report_pass("invalid escapes, unsupported Unicode escapes and unterminated strings (4 cases)");
}

void test_error_offsets()
{
    struct test_case
    {
        const char* input;
        json::error expected_error;
        std::size_t expected_offset;
    };

    const test_case cases[] = {
        { "@", json::error::expected_value, 0 },
        { R"json({"a" 1})json", json::error::expected_colon, 5 },
        { R"json({"a":1 "b":2})json", json::error::expected_comma, 7 },
        { "[", json::error::unexpected_end, 1 },
        { "1.", json::error::invalid_number, 2 },
        { R"json("bad\q")json", json::error::invalid_string, 6 },
        { "nulx", json::error::invalid_literal, 0 },
        { "trueX", json::error::unexpected_character, 4 }
    };

    for (const test_case& test : cases)
    {
        char input[64];
        std::strcpy(input, test.input);
        json::document<16, 4> document;
        const auto result = json::parse(input, std::strlen(input), document);
        assert(!result);
        assert(result.code == test.expected_error);
        assert(result.offset == test.expected_offset);
    }

    char capacity_input[] = "[1, 2, 3, 4]";
    json::document<4, 1> capacity_document;
    const auto capacity_result = json::parse(capacity_input, std::strlen(capacity_input), capacity_document);
    assert(!capacity_result);
    assert(capacity_result.code == json::error::capacity_exceeded);
    assert(capacity_result.offset == 11);

    char depth_input[] = "[[0]]";
    json::document<3, 1> depth_document;
    const auto depth_result = json::parse(depth_input, std::strlen(depth_input), depth_document);
    assert(!depth_result);
    assert(depth_result.code == json::error::nesting_limit);
    assert(depth_result.offset == 1);

    report_pass("error codes and source offsets (10 cases)");
}

void test_value_api()
{
    char input[] = R"json({
        "null": null,
        "boolean": true,
        "integer": 42,
        "number": 3.5,
        "string": "hello",
        "empty_string": "",
        "array": [1, 2],
        "empty_array": [],
        "object": { "member": false },
        "empty_object": {}
    })json";

    json::document<32, 4> document;
    const auto result = json::parse(input, std::strlen(input), document);
    assert(result);

    const auto root = document.root();
    assert(root);
    assert(root.type() == json::value_type::object);
    assert(root.is_object());
    assert(root.size() == 10);
    assert(root["null"]);
    assert(root["null"].is_null());
    assert(root["boolean"].is_boolean());
    assert(root["boolean"].as_boolean());
    assert(root["integer"].is_integer());
    assert(root["integer"].is_number());
    assert(root["integer"].as_integer() == 42);
    assert(root["integer"].as_number() == 42.0);
    assert(root["number"].type() == json::value_type::number);
    assert(root["number"].is_number());
    assert(root["number"].as_number() == 3.5);
    assert(root["string"].is_string());
    assert(equal(root["string"].as_string(), "hello"));
    assert(root["empty_string"].is_string());
    assert(root["empty_string"].as_string().size == 0);
    assert(root["array"].is_array());
    assert(root["array"].size() == 2);
    assert(root["array"][0].as_integer() == 1);
    assert(root["array"][1].as_integer() == 2);
    assert(root["empty_array"].is_array());
    assert(root["empty_array"].size() == 0);
    assert(root["object"].is_object());
    assert(root["object"].size() == 1);
    assert(root["object"]["member"].is_boolean());
    assert(!root["object"]["member"].as_boolean());
    assert(root["empty_object"].is_object());
    assert(root["empty_object"].size() == 0);
    assert(!root["missing"]);
    assert(!root["array"][2]);

    json::document<4, 2> empty_document;
    assert(!empty_document.root());

    report_pass("value API types, accessors, indexing and invalid-value checks");
}

void test_invalid_json()
{
    char input[] = R"json({
        "name": "broken"
        "value": "missing comma"
    })json";

    json::document<16, 4> document;
    const auto result = json::parse(input, std::strlen(input), document);
    assert(!result);
    assert(result.code == json::error::expected_comma);

    report_pass("malformed object missing comma");
}

} // namespace

int main()
{
    std::printf("JSON parser test suite\n");
    std::printf("======================\n");

    test_cmake_settings();
    test_numeric_values();
    test_invalid_numbers();
    test_numeric_edges();
    test_capacity_limits();
    test_depth_limits();
    test_string_escapes();
    test_invalid_strings();
    test_error_offsets();
    test_value_api();
    test_invalid_json();

    std::printf("----------------------\n");
    std::printf("%d tests passed\n", tests_run);
    return 0;
}