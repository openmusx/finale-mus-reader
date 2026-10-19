# Build invariants

Why unity compilation is a project-owned-target invariant, how a dependency opts out, the MSVC
flags that are directory-wide rather than per-target, and the build-interface targets the tree
exposes beyond its public library.

## Unity compilation

**A unity build is this project's invariant and is never imposed on code this repository does
not own.** `CMakeLists.txt` states that rule once, as `finale_mus_reader_keep_out_of_unity()`,
and calls it at each dependency: the zlib targets, musxdom, pugixml, Catch2's companion target,
and the pinned `blast` source. zlib is the standing example of why — its `inftrees.h` is
included by several of its own sources and is not idempotent — but the rule is about ownership,
and a dependency added later is opted out by calling that function. Catch2 itself is the one
deliberate exception: `tests/CMakeLists.txt` amalgamates it for build speed, which is a choice
about one dependency rather than a consequence of the policy.

In a normal build, no dependency target has a `Unity/` source: only `finale_mus_reader`,
`finale_mus_reader_tests`, `finale_mus_reader_reporting_tests`, and the deliberate Catch2
appear.

## Directory-wide MSVC flags

`/bigobj` and `/utf-8` are applied with `add_compile_options` at the top of `CMakeLists.txt`
so that no object in the tree is built without them. See
[code_conventions.md](code_conventions.md) for why each is needed.

## Dependencies

- **musxdom** is fetched at a pinned revision, or supplied through
  `FINALE_MUS_READER_MUSXDOM_SOURCE_DIR`, or reused when a parent build already defines the
  `musx` target.
- **zlib** is reused from a parent (`ZLIB::ZLIB`, `zlibstatic`, or `zlib`), found on the
  system with `FINALE_MUS_READER_USE_SYSTEM_ZLIB=ON`, or fetched at the pinned 1.3.1 release.
- **pugixml** and **Catch2** are fetched only when tests are built; the library never selects,
  links, or enables an XML backend.
- **blast** (PKWARE DCL) is vendored under `third_party/blast/` and built as an object library
  with its own warning settings.

## Build-interface targets

Besides the public `finale_mus_reader::finale_mus_reader`, the tree defines two INTERFACE
targets when `FINALE_MUS_READER_BUILD_TESTING` is on. Neither is API and neither is installed;
they centralize the test-only include paths and dependencies:

- `finale_mus_reader::internal` — the private `src/` headers, the library, the selected pugixml
  target, and zlib.
- `finale_mus_reader::test_support` — `tests/classes/common/` (the class-test helpers), the
  `FINALE_MUS_READER_TEST_SOURCE_DIR` definition naming the fixture root, `Catch2::Catch2WithMain`,
  and `finale_mus_reader::internal`. The Catch2 CMake module directory is cached as
  `FINALE_MUS_READER_CATCH2_EXTRAS_DIR` for the same reason.

Changing a private header therefore changes what such a parent compiles against; that is
accepted, because the internal headers are not a contract.

## Instrumentation

`FINALE_MUS_READER_INSTRUMENTATION` is `AUTO`, `ON`, or `OFF`. `AUTO` compiles the reader's
diagnostics and phase timing in only when tests are built and out for every ordinary client.
A client that wants `ImportReport` selects `ON`. The `reporting` test
(`FINALE_MUS_READER_BUILD_REPORTING_TESTING`) builds in either mode without the test suite or an
XML backend, so CI can prove the non-instrumented library still compiles.
