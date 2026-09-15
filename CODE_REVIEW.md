# External Code Review

This branch is a review snapshot of the current `json_parser` implementation.

The purpose of this pull request is to obtain an independent automated review of the parser, tests, and supporting documentation before any further changes are made to `main`.

Particular areas of interest:

- memory safety and bounds checking
- undefined behaviour and lifetime issues
- integer and floating-point overflow handling
- parser state and error offsets
- nesting and fixed-capacity limits
- in-place string decoding
- C++14 portability
- API edge cases
- malformed and adversarial input handling
- consistency between implementation, tests, and documentation

No production behaviour is intentionally changed by this file.
