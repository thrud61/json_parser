# json_parser

A small, header-only JSON parser for C++14 with deterministic memory usage.

The primary application is **configuration data**, where JSON is typically read once, parsed, and then used to configure an application or component. The design favours predictable resource usage and a small footprint over the broad feature set of general-purpose JSON libraries.

## Goals

- Header-only
- C++14
- No dynamic allocation
- No exceptions
- Bounded memory usage
- Bounded JSON nesting depth
- Small and simple API
- Suitable for configuration data
- Suitable for systems and embedded-style applications

The parser is intended for applications where a general-purpose JSON library provides considerably more functionality than is required, and where predictable resource usage is important.

## Design

A parsed document has a fixed capacity defined at compile time:

```cpp
json::document<256, 32> doc;
```

The template parameters define:

- `256` — maximum number of JSON values/nodes, including the root value.
- `32` — maximum JSON nesting depth.

The document does not allocate memory dynamically. Its storage requirements are therefore known from its template parameters and the implementation's fixed-size supporting storage.

JSON strings are intended to be decoded in-place in the caller-provided input buffer. This avoids allocating separate storage for string contents.

## Intended usage

A typical use will look something like:

```cpp
char buffer[] = R"({"server":{"port":8080}})";

json::document<256, 32> doc;

auto result = json::parse(buffer, sizeof(buffer) - 1, doc);

if (!result)
    return result.error();

// Access parsed values...
```

The exact API is still under development.

## JSON support

The parser will initially target standard JSON:

- `null`
- `true` / `false`
- numbers
- strings
- arrays
- objects
- JSON whitespace and escaping

Non-standard extensions such as comments will not be supported unless there is a compelling reason to add them.

## Resource limits

Resource limits are explicit rather than being determined by the heap or operating system.

For example:

```cpp
json::document<64, 8> doc;
```

can represent at most 64 JSON values and eight levels of nesting. Exceeding either limit is a parse error.

## Status

Early design stage. The API and internal representation are not yet finalised.

## License

To be decided.
