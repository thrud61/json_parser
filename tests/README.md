# json_parser Test Suite

This directory contains the test suite for `json_parser`.

The tests are intended to verify both normal JSON parsing and the public access API, and the behaviour of the parser when presented with malformed, truncated, or deliberately difficult input.

Both test executables are built as C++14 programs and are compiled with **AddressSanitizer (ASan)** enabled where supported. They are also registered with CTest.

## Test programs

### `json_test.cpp`

The main functional test suite.

#### CMakeSettings parsing

Parses a representative configuration document containing:

- Nested objects and arrays
- Multiple configuration entries
- Object member lookup
- Array indexing
- Nested member access
- Strings containing escaped backslashes
- In-place string decoding

The test data is a representative, anonymised subset of a real-world CMake configuration.

#### Numeric values

Tests valid JSON numbers including:

- Zero
- Positive and negative integers
- 64-bit signed integer limits
- Decimal fractions
- Negative fractions
- Positive exponents
- Negative exponents

The tests also verify the distinction between integer and non-integer numeric values and the `as_integer()` / `as_number()` accessors.

#### Invalid numbers

Tests rejection of invalid JSON number syntax, including:

- Leading zeroes
- Missing digits after a sign
- Missing fractional digits
- Missing exponent digits
- Missing exponent sign digits
- Positive integer overflow
- Negative integer overflow

#### Numeric edge cases

Tests floating-point boundary handling:

- Positive overflow such as `1e309`
- Negative overflow such as `-1e309`
- Very small exponents such as `1e-10000`

Non-finite floating-point results must be rejected.

#### Capacity limits

Verifies the compile-time `MaxValues` limit:

- Parsing exactly up to the available capacity
- Detecting capacity exhaustion
- Correct value-node counting
- Storage requirements of empty objects and arrays

#### Nesting depth

Verifies the compile-time `MaxDepth` limit:

- Parsing at the maximum permitted depth
- Rejecting input beyond the permitted depth

#### String handling

Tests valid string handling including:

- `\"`
- `\\`
- `\/`
- `\b`
- `\f`
- `\n`
- `\r`
- `\t`
- Empty strings
- Punctuation
- In-place decoding

#### Invalid strings

Tests rejection of:

- Invalid escape sequences
- Unsupported `\uXXXX` Unicode escapes
- Unterminated strings
- Literal control characters

#### Error reporting

Verifies both the reported error code and the zero-based source offset for representative parsing failures.

The tests cover errors including:

- Unexpected values
- Missing object colons
- Missing array/object separators
- Unexpected end of input
- Invalid numbers
- Invalid strings
- Invalid literals
- Capacity exhaustion
- Nesting-limit exhaustion

#### Value API

Tests the public value-access API, including:

- JSON value types
- `type()`
- `is_null()`
- `is_boolean()`
- `is_integer()`
- `is_number()`
- `is_string()`
- `is_array()`
- `is_object()`
- `as_boolean()`
- `as_integer()`
- `as_number()`
- `as_string()`
- `size()`
- Array indexing
- Object member lookup
- `string_ref`
- Invalid-value detection using `operator bool()`

The tests also verify missing object members and out-of-range array accesses.

#### Invalid JSON

Additional tests verify rejection of malformed JSON structures and invalid syntax.

---

### `adversarial_test.cpp`

The adversarial suite concentrates on malformed and boundary input.

It is intended to exercise parser failure paths that are less likely to be encountered during normal configuration parsing.

#### Truncated structures

Tests incomplete arrays and objects, including:

- `[`
- `[1`
- `[1,`
- `{`
- `{"a"`
- `{"a":`
- `{"a":1`
- `{"a":1,`

#### Malformed separators

Tests incorrect use or omission of commas and colons in arrays and objects.

Examples include:

- `[,]`
- `[1,]`
- `[1 2]`
- `{,}`
- `{"a",1}`
- `{"a":}`
- `{"a":1,}`
- `{"a":1 "b":2}`

#### Malformed literals

Tests truncated and corrupted forms of:

- `null`
- `true`
- `false`

It also verifies that valid literals followed by additional characters are rejected.

#### Malformed numbers

Tests malformed number forms and invalid number starts, including:

- `-`
- `01`
- `-01`
- `1.`
- `1.e1`
- `1e`
- `1e+`
- `1e-`
- `--1`
- `-+1`
- `+1`
- `.1`

Trailing characters after otherwise valid numbers are also tested.

#### String boundaries

Tests strings ending at difficult parser boundaries, including:

- Empty/unterminated strings
- A trailing escape character
- An escaped quote at the end of input
- Invalid escapes
- Unsupported Unicode escapes
- Literal control characters

#### Long tokens

Exercises the parser with approximately 1 KB tokens:

- Long strings
- Long integer input
- Long fractional input

This checks that token length does not cause uncontrolled temporary storage or unsafe behaviour.

#### Whitespace and trailing data

Tests:

- Large runs of JSON whitespace
- Whitespace surrounding a valid value
- Multiple JSON values in one input
- Adjacent objects
- Adjacent strings

The parser must accept surrounding whitespace but reject trailing non-whitespace data.

#### Reusing a document after failure

Verifies that a document can be reused after a failed parse.

A failed parse is followed by a successful parse using the same `document`, and the resulting document is checked to ensure it contains only the new valid input.

## Sanitizer testing

Both test executables are built with AddressSanitizer.

On MSVC:

```text
/fsanitize=address
```

On GCC/Clang-compatible toolchains:

```text
-fsanitize=address
-fno-omit-frame-pointer
```

This is intended to detect memory safety problems such as:

- Buffer overflows
- Out-of-bounds accesses
- Invalid memory accesses
- Other AddressSanitizer-detectable failures

## Running the tests

Configure the project with CMake and build the test targets.

The tests can then be run directly:

```text
json_test
json_adversarial_test
```

or through CTest:

```text
ctest
```

The test programs report each completed test group and finish with a summary of the number of tests passed.

A successful run therefore verifies both the functional test suite and the adversarial test suite without relying solely on the process exit code.

## Test philosophy

The parser is designed around bounded resources and predictable behaviour. The tests therefore place particular emphasis on:

- Exact resource boundaries
- Nesting limits
- Integer overflow
- Floating-point overflow
- Malformed input
- Truncated input
- Error locations
- In-place string handling
- Invalid value handling
- Reuse after parse failure
- Long tokens
- Memory safety under ASan

The objective is not to test every possible JSON document individually, but to exercise the parser's grammar, public API, resource limits, numeric conversion, error handling, and failure paths systematically.
