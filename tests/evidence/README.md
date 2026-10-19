# Test fixtures

Controlled Finale documents used by the test suite. Each directory holds the fixtures written by
one Finale release, named by that release: `F100` is Finale 1.0.0, `F263` is 2.6.3, `F300` is 3.0,
`F372` is 3.7.2, `F97` and `F98` are Finale 97 and 98, and `F2000` through `F2012` are the annual
releases.

Within a directory:

- `<name>.mus` is the legacy document as that release saved it.
- `<name>.etf` (or `.ETF`) is the same document exported as Enigma Transportable File by the
  release that wrote it, where the release could export one.
- `-finale27/<name>.musx` is Finale 27's conversion of the same document. It is a semantic
  reference for Finale's upgrade behavior, not a byte-level one: conversion may normalize,
  synthesize, substitute, reorder, or remove legacy values.
- `provenance.txt` records the release and platform that wrote the directory's fixtures, the
  SHA-256 of each file, and what each fixture changes relative to its baseline.

Most fixtures come in pairs: a baseline saved from a new document, and a variant that changes one
named setting.

Each directory's `provenance.txt` states the origin of its fixtures. Every fixture whose name
begins with its release prefix (`F100-`, `F2012-`, and so on) was created by the repository's
author, some originally as plug-in test cases, and is released under the repository's MIT
license. Third-party graphics embedded in some fixtures carry their own licenses, recorded
beside their entries; a license that must travel with its file is kept next to it as
`<file>.LICENSE`. Tests locate them through `FINALE_MUS_READER_TEST_SOURCE_DIR`, defined by
`tests/CMakeLists.txt`, and never through a path outside this tree.
