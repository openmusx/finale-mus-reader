# AGENTS.md

Guidance for coding agents and contributors working in this repository.

## Project purpose and boundaries

Legacy Finale MUS Reader is an MIT-licensed C++ library for reading legacy proprietary Finale
`.mus` files and constructing a `musxdom::Document`.

- This repository is a client of `musxdom` and must remain separate from it.
- Construct musxdom objects directly. Do not add a second public document model.
- Do not add legacy MUS decoding code to musxdom.
- Do not implement MUSX package extraction here. musxdom already accepts raw EnigmaXML
  through `DocumentFactory`.
- Do not bundle Finale fonts, Finale libraries, or proprietary source material.
- Keep the importer removable as an independent component if provenance or licensing concerns
  change.

The reader recovers what it has verified and reports everything else as a synthesized default.
Implement narrow, verified vertical slices and do not present partial format coverage as a
universal reader.

**An assignment scoped to one importer never authorizes changing another, settled importer.**
This remains true when the new class shares records, exposes a dependency, or appears to require
different behavior from the settled importer. Stop before editing it, show the user the concrete
dependency and proposed change, and obtain explicit permission. Test and comparison differences
are evidence to report, not implicit authorization to expand the implementation scope.

## Skills

Task-specific procedures live in `.agents/skills/<name>/SKILL.md`, kept tool-neutral so any
agent can use them. Read the relevant one before starting that task:

- `.agents/skills/implement-class-importer/`: add or extend recovery of one musxdom class:
  field manifest, mapping tables, registry row, timing phase, fixtures, and tests.
- `.agents/skills/comment-production-code/`: what a comment may state, how to label a belief,
  and what never belongs in one.
- `.agents/skills/prepare-pull-request/`: final local validation and delivery of a feature
  branch when the user asks for a pull request.

## Documentation

`docs/` holds the binding project rules and cross-cutting format knowledge the code relies on.
Read one when the task calls for it:

| Read | Before |
|---|---|
| [`docs/format_overview.md`](docs/format_overview.md) | Any work on the reader: the pipeline, four epochs, framing, generic record rows, and terminology. Never add class-specific format notes. |
| [`docs/decoder_rules.md`](docs/decoder_rules.md) | Writing or changing a layout gate, or any binary read. Structural markers outrank version gates; epoch gates outrank version gates. |
| [`docs/text_encoding.md`](docs/text_encoding.md) | Any text or symbol conversion. Never re-encode pre-Finale-2012 text without that text's own font. |
| [`docs/options_fallback.md`](docs/options_fallback.md) | Adding an option overlay or reporting a `ValueOrigin`. |
| [`docs/embedded_defaults.md`](docs/embedded_defaults.md) | Touching `src/defaults/`, `resources/defaults/`, or the resource generator. |
| [`docs/build_invariants.md`](docs/build_invariants.md) | Changing CMake or adding a dependency. |
| [`docs/code_conventions.md`](docs/code_conventions.md) | Writing or reviewing any project-owned C++ source file. |
| [`tests/evidence/README.md`](tests/evidence/README.md) | Adding or using a fixture. |

Do not add a class's selectors, class ids, payload layout, flags, version behavior, sharing
behavior, or recovery status to `docs/format_overview.md`. The importer, its tests, and fixture
provenance are the record of a class's format; do not add class reference prose alongside them.

Comments in `src/` and `include/` state how the code works, and how the format is believed to
work where belief is all there is, with the confidence labeled. They do not carry how a behavior
was derived, which fixture established it, or what was believed before. A fixture's own
`provenance.txt` records only what that fixture is. Doxygen documents the contract, not its
history.

## Repository map

- `include/finale_mus_reader/`: public C++ API.
- `src/container/`: headers, format classification, byte order, compression, framing,
  checksums, and bounds validation.
- `src/defaults/`: embedded Finale-default resources and options seeding.
- `src/import/`: direct construction and overlay of musxdom objects, one file per class under
  `options/`, `others/`, `details/`, and `texts/`, with the shared mapping framework under
  `support/`.
- `src/reader/`: reader orchestration, document construction, and instrumentation.
- `src/records/`: version-aware wire-record decoding and the record index.
- `resources/defaults/`: authoritative Finale 27 New Document Without Libraries fallback
  resources for macOS and Windows.
- `tests/`: the Catch2 suite; `tests/classes/<pool>/` mirrors `src/import/`, and
  `tests/classes/common/` holds the shared helpers.
- `tests/evidence/`: controlled MUS/ETF/MUSX fixtures with their provenance.
- `scripts/`: maintenance checks: formatting, embedded-defaults generation, the reporting
  boundary, and documentation links.
- `third_party/`: pinned, license-compatible codec code (`blast`). Not ours to edit.
- `docs/`: the rules and format knowledge listed above.

## Nothing is implemented more than once

Every line of code is a liability that must be checked and maintained in perpetuity. Prefer
solutions that centralize reuse and avoid boilerplate without introducing overly complex
abstractions. A change with more red deletions than green insertions is a win.

In library code, every fact and every behavior has exactly one implementation. A second copy is a
defect even when both copies are currently correct, because the two will diverge and the
divergence will be silent. Treat an exception as needing extraordinary justification, stated in
a comment at the site. Duplication is not paid for by being convenient, by being small, or by
the copies being far apart — distance makes it worse.

In practice:

- If a constant, a spelling, a table, or a rule is needed in two places, give it one home and
  include it. Do not restate it, not even in a `case` label.
- If musxdom already implements something, call it. Do not reimplement it here, and do not
  restate the values it is built on.
- When a fact must be recorded twice for humans, record it once as code and once as prose that
  points at the code.
- Before adding a literal, grep for it. Three separately hardcoded spellings of the product
  banner are the kind of drift this rule exists to prevent.

The rule binds `src/`, `include/`, and the fixtures. Test code may repeat itself where a test
that spells out its own expectations is clearer than one that shares a helper with the code
under test.

## Architecture in brief

Legacy values are not read by hand-written per-field code. Through Finale 2006 the option and
other pools resolve to fixed 16-byte rows, so the reader presents each `(tag, cmper)` record
family as a word stream and describes every mapped field in a table (`FieldMapping`, declared
with the `MUS_WORD` family of macros in `src/import/support/legacy_mapping.h`). One table
corresponds to one musxdom class; a generic engine applies them all, records where each value
came from, and reports every unmapped field at its synthesized default. Tables declare which
epochs and versions they apply to, and a later table supersedes an earlier row for the same
field. The zlib era's class records feed the same tables through the record index.

Each class exposes one `ClassImporter` from its pool header (`src/import/<pool>.h`) and is
registered once, in `legacy_mapping.cpp`; everything era-specific stays inside the class's own
translation unit. A stage that a test drives on its own lives in `<pool>/test_access.h`, which
no library code includes. See the `implement-class-importer` skill.

## Build, code, and tests

The build uses CMake. Keep these properties intact when extending it:

- Provide a normal CMake library target and deliberate musxdom dependency integration suitable
  for downstream clients.
- Require C++20. `std::format` is permitted where it improves clarity.
- Require zlib for gzip and zlib-era decoding. Reuse a zlib target supplied by a parent project,
  otherwise fetch the pinned source by default; retain the option to use an installed system
  zlib.
- Keep the library independent of XML implementations. Do not select, fetch, link, or enable
  pugixml, RapidXML, TinyXML2, QtXML, or another backend for the library target. Accept a
  concrete `musx::xml::IXmlDocument` implementation through the public reader template. Tests
  may fetch and instantiate pugixml without propagating its backend definition to the library.
- Keep committed generated resource sources synchronized with their gzip inputs and verify them
  with `scripts/generate_embedded_defaults.py --check`.
- Formatting is fixed by `.clang-format` and checked by `scripts/check_format.py`; run it with
  `--fix` before handing off. The rest of the conventions are in
  [`docs/code_conventions.md`](docs/code_conventions.md).
- Keep public APIs small and keep wire-format details out of public interfaces unless callers
  need them for diagnostics or capability reporting.
- Route project-owned runtime warnings and errors through musxdom's logging channel; do not
  write them directly with `fprintf` or to stderr.
- Unity compilation is a project-owned-target invariant and is never imposed on code this
  repository does not own; a dependency is opted out by calling
  `finale_mus_reader_keep_out_of_unity()` at its site
  ([`docs/build_invariants.md`](docs/build_invariants.md)).

Add focused tests with every implemented format fact. Prefer controlled fixtures and exact
byte/offset assertions. Include malformed and truncated inputs for container and decompression
work. Run the relevant build and tests before finishing.

```bash
cmake -S . -B build                      # fetches the pinned musxdom
cmake --build build
ctest --test-dir build --output-on-failure
```

For a local sibling musxdom checkout, add `-DFINALE_MUS_READER_MUSXDOM_SOURCE_DIR=../musxdom`
to the configure step.

Do not modify fixtures or default resources unless the task requires it. If exact-source files
change intentionally, record their hashes in the directory's `provenance.txt`.

## Git hygiene

- Never commit or push directly to `main`. Work on a feature branch and deliver changes through
  a pull request.
- Do not push a feature branch, create a pull request, or commit unless the user requests that
  action.
- Preserve unrelated user changes in a dirty worktree.
- Keep implementation changes focused; avoid unrelated cleanup.
- Before handing off, inspect `git status --short` and `git diff --check`, and run
  `python3 scripts/check_format.py`. The authoritative CRLF EnigmaXML resources are the known
  exception to `git diff --check` whitespace reports.
