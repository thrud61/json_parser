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
    return static_cast<int>(result.code);

// Access parsed values...
```

## JSON support

The parser supports the core JSON value types and JSON number/string syntax needed by its configuration-oriented design:

- `null`
- `true` / `false`
- integers and floating-point numbers
- strings
- arrays
- objects
- JSON whitespace and the standard simple string escapes

The `\uXXXX` Unicode escape form is currently rejected rather than decoded. Raw UTF-8 bytes in strings are not interpreted by the parser and are retained as input bytes.

Duplicate object member names are permitted. When looking up a member by name, the first matching member is returned.

Non-standard extensions such as comments are not supported.

## Resource limits

Resource limits are explicit rather than being determined by the heap or operating system.

For example:

```cpp
json::document<64, 8> doc;
```

can represent at most 64 JSON values and eight levels of nesting. Exceeding either limit is a parse error.

Parsing is recursive, so `MaxDepth` also bounds parser recursion. Setting it to a very large value increases stack usage accordingly.

Floating-point values are converted using bounded decimal processing rather than locale-dependent conversion routines. Results that cannot be represented as a finite `double` are rejected as `invalid_number`.

## API behaviour

`document::value` is a lightweight, non-owning handle. Missing object members and out-of-range array elements produce an invalid value, which can be tested with:

```cpp
if (!doc.root()["missing"])
    // member was not present
```

`operator bool()` is the validity check. Other `value` accessors assume the handle is valid.

A failed parse may leave a partial document; the document must not be used unless the parse operation succeeds.

## Status

The parser is usable for its intended bounded, configuration-oriented use case. The API and internal representation may still evolve.

## License

To be decided.
