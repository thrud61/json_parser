#include "json.hpp"

#include <cassert>
#include <cstring>

namespace
{

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

    const auto result =
        json::parse(input, std::strlen(input), document);

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

    // JSON escapes are decoded in-place in the caller-owned buffer.
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
    const auto result =
        json::parse(input, std::strlen(input), document);

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
}

void test_invalid_numbers()
{
    const char* inputs[] = {
        "01",
        "-",
        "1.",
        "1e",
        "1e+",
        "9223372036854775808",
        "-9223372036854775809"
    };

    for (const char* text : inputs)
    {
        char input[64];
        std::strcpy(input, text);

        json::document<8, 2> document;
        const auto result =
            json::parse(input, std::strlen(input), document);

        assert(!result);
        assert(result.code == json::error::invalid_number);
    }
}

void test_invalid_json()
{
    char input[] = R"json({
        "name": "broken"
        "value": "missing comma"
    })json";

    json::document<16, 4> document;

    const auto result =
        json::parse(input, std::strlen(input), document);

    assert(!result);
    assert(result.code == json::error::expected_comma);
}

} // namespace

int main()
{
    test_cmake_settings();
    test_numeric_values();
    test_invalid_numbers();
    test_invalid_json();

    return 0;
}
