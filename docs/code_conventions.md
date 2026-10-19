# C++ code conventions

Formatting, naming, file headers, namespaces, preprocessor comments, Windows macro safety, MSVC
directory-wide flags, unity-build cleanliness, and the import-reporting boundary. Project rules
for every project-owned C++ source file.

- Follow the surrounding musxdom C++ conventions where this repository has not established a
  local style.
- Match musxdom's naming: `camelCase` for methods, properties, and variables, `PascalCase` for
  classes and enums. Source files are `snake_case`.
- Begin every project-owned C++ header and source file with
  `Copyright (c) 2026 Robert G. Patterson` and the SPDX identifier `MIT`. Preserve original
  copyright and license notices in third-party sources.
- Use explicit nested namespace blocks rather than concatenated namespace declarations.
- End every preprocessor conditional with a comment naming the condition, such as
  `#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)`. For `#ifdef` and `#ifndef`,
  spell the comment as the corresponding positive or negated condition.
- Do not require `NOMINMAX`. Protect standard-library `min` and `max` tokens from the Windows
  macros with parentheses: `(std::min)(a, b)`, `(std::numeric_limits<T>::max)()`.
- The test target compiles with `/W4 /WX` under MSVC and `-Werror` elsewhere, so an implicit
  narrowing conversion is a build break, and MSVC reports narrowing that GCC and Clang accept.
  Never pass a bare literal where the parameter is a `std::optional<T>` or a `T` narrower than
  the literal's own type; write `T(value)`, for example `Evpu(24)` or `std::uint8_t(0x7f)`. Test
  files are the usual offender because the parameter type is not visible at the call site.
- Every C and C++ object is compiled with `/bigobj` under MSVC, directory-wide, so
  template-heavy musxdom factory instantiations cannot exceed the default COFF section limit.
- Every object is compiled with `/utf-8` under MSVC, also directory-wide. Sources carry UTF-8
  string literals and no byte-order mark; without the flag MSVC reads them in the machine's
  active code page and silently produces different bytes.
- Keep every project-owned translation unit unity-build clean. Unity compilation is the normal
  build for the library and tests. CMake may combine unrelated source files into one translation
  unit, so an anonymous namespace does not make file-local names unique after amalgamation; use
  distinctive names for aliases, helpers, and constants. Do not let unity-only fixes change
  runtime behavior.
- Project-owned targets enable unity compilation themselves; external dependencies retain their
  own build policy ([build_invariants.md](build_invariants.md)).

## Comments

A comment states how the code works, and how the format is believed to work where belief is all
there is, with the confidence labeled. It does not carry how a behavior was derived, which
fixture established it, or what was believed before. The `comment-production-code` skill under
`.agents/skills/` states the rule in full.

## Formatting

Formatting is defined by [`.clang-format`](../.clang-format) at the repository root and is not a
matter of taste or of matching the surrounding file. `scripts/check_format.py` runs clang-format
over every project-owned C++ file (`include/`, `src/`, `tests/`; vendored and generated files
are listed in `.clang-format-ignore`), CI runs the same check, and `scripts/check_format.py
--fix` rewrites the tree. Run `--fix` before handing off; do not hand-format. The script pins the
clang-format major version because releases differ in output.

The rules are MuseScore's, translated from its uncrustify profile: four-space indentation, a
150-column limit, braces on their own line for function and type definitions (`class`, `struct`,
`union`) and attached everywhere else (`namespace`, `enum`, control statements, lambdas), braces
required around every control-statement body, `Type* p` and `Type& r`, and a wrapped `&&`,
`||`, or `?:` leading its line. Three deliberate deviations from MuseScore, each named in the
config file: `struct` braces break like `class` braces; braced initializer lists have no inner
spaces (`{1, 2}`); and a wrapped argument or parameter list continues at one indent level rather
than aligned under the open paren. Function arguments and braced lists are governed separately:
arguments are filled to the column limit, while a braced list with a trailing comma always stays
one item per line and one without is left on one line when it fits. That comma is how a table
keeps its row shape, so use it instead of `// clang-format off`.

## Import reporting

Class importers use the private lazy boundary in
[`reporting.h`](../src/import/support/reporting.h). Put substantial reporting passes in named
`report…` helpers with ordinary decoder or DOM types in their signatures. Small isolated reports
may use `withReporting` inline. Instrumentation build directives and token-erasing report macros
belong outside class importers; `scripts/check_reporting_boundary.py` checks this boundary and
CI runs it.

`withReporting` is the unit that disappears in a non-instrumented build. Keep the reporting work
for an object inside one callback, including field-name construction, row and offset lookups, and
calls to shared helpers. A helper that calls `withReporting` once per field still evaluates its
arguments in a non-instrumented build. Use `reportLegacyField` and `reportFallbackField` from
`reporting.h` when their field metadata and overwrite behavior match the importer; keep local
reporting code for different layouts or provenance rules. Adopt these helpers when they make an
importer clearer, without changing settled importers solely to use them.

A reporting callback is generic: use its writer for instance keys, origins, field metadata, and
report access. Those names must depend on the callback parameter, because a non-instrumented
build does not declare the public instrumentation types. Use `defaultField` and `behaviorField`
for singleton options, `unmappedField` to preserve existing provenance, and `report()` for the
remaining public report operations. All report-only formatting, allocation, lookups, and loops
belong inside the callback, including preparation of arguments to subordinate reporting helpers.
Capture existing objects by reference; capture initializers execute even when the callback does
not. Callbacks run synchronously and never perform document mutations, required validation, or
user diagnostics.

`ReportClass` and `ReportInstance` retain identities across callbacks; `DeferredFieldReport`
retains a field awaiting reference resolution. Class-specific accumulated metadata uses
`ReportState<State>`, where `State` is a template with writer-dependent instrumentation types.
These wrappers have no payload with instrumentation disabled. Do not create parallel origin
enums or recompute decoding decisions to construct reports; pass the decoder's existing outcomes.

Validate reporting changes with the instrumented suite. Do not run a complete non-instrumented
library build during ordinary implementation or validation. That build is required only by the
`prepare-pull-request` skill when opening a pull request, or when the user explicitly requests it.
`FINALE_MUS_READER_BUILD_REPORTING_TESTING=ON` enables the standalone `reporting` test in either
configuration without requiring an XML backend.
