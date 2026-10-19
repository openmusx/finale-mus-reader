# Legacy Finale MUS Reader

Legacy Finale MUS Reader is a C++ library for reading legacy Finale `.mus` files — the
proprietary binary format Finale used from version 1.0 through Finale 2012 — and constructing
[`musxdom`](https://github.com/openmusx/musxdom) documents from them.

The project is intentionally separate from musxdom. Legacy Finale MUS Reader is a client of
musxdom; musxdom does not depend on this library and contains no legacy `.mus` decoding code.

## Status

The reader is a narrow, verified importer rather than a complete one. For every supported
document it creates a musxdom document with a complete Finale 27 options pool, recovers the
header, validates the container, and overlays every legacy value it has verified. It is not yet
a complete score importer: entries (notes and rests) and their details are not imported.

What it does:

1. Classify a file's Finale format epoch and byte order.
2. Decode the applicable container or compression layer, validating sizes and checksums.
3. Recover the header and the legacy "other", "detail", and text records.
4. Seed a complete musxdom options pool from a pinned Finale 27
   new-document-without-libraries baseline.
5. Overlay every legacy value that has been confidently decoded: all 28 options classes, and
   the others, details, and texts classes listed in `src/import/`.
6. Report, when instrumented, which values were recovered and which remain synthesized
   defaults.

| MUS epoch | Container support |
| --- | --- |
| Finale 1.x–2.6 (Coda banner) | Product and version from the `Finale(TM)` banner, chained record pools, text chunks. |
| Finale 3.x–2000 (uncompressed) | Validated typed pools in either byte order, banner header, record index, text pool. |
| Finale 2001–2006 (DCL) | Validated typed blocks, PKWARE DCL inflation, CRC-32, record index, text pool. |
| Finale 2007–2012 (zlib) | Validated typed blocks, zlib inflation, CRC-32, either byte order, class records. |

Unknown banner-era framing still produces an empty options-complete document and a recovered
header rather than being mislabeled as a known epoch.

## Repository layout

| Path | Purpose |
| --- | --- |
| `include/finale_mus_reader/` | Public C++ API. |
| `src/container/` | File headers, format classification, byte order, and compression. |
| `src/defaults/` | Pinned fallback data used to create structurally complete documents. |
| `src/import/` | Direct construction and overlay of musxdom objects, one file per class. |
| `src/records/` | Wire-level decoding of others, details, and text records. |
| `src/reader/` | Reader orchestration, document construction, and instrumentation. |
| `resources/defaults/` | The Finale 27 baseline EnigmaXML resources. |
| `tests/` | Catch2 test suite. |
| `tests/evidence/` | Controlled MUS, ETF, and MUSX fixtures with their provenance. |
| `docs/` | Format overview and binding project rules. |
| `scripts/` | Maintenance checks. |
| `third_party/` | The pinned `blast` PKWARE DCL decoder. |

`AGENTS.md` orients contributors and coding agents.

## Building

The default build fetches the pinned musxdom revision:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

For development with a sibling musxdom checkout:

```bash
cmake -S . -B build -DFINALE_MUS_READER_MUSXDOM_SOURCE_DIR=../musxdom
```

The project requires C++20 and zlib. By default it fetches the pinned zlib 1.3.1 source and
builds its static target. Set `FINALE_MUS_READER_USE_SYSTEM_ZLIB=ON` to use an installed zlib
instead. When a parent build already provides `ZLIB::ZLIB`, `zlibstatic`, or `zlib`, the reader
reuses that target before considering either option; its FetchContent dependency is also named
`zlib` and uses the same release and hash as denigma, so a parent that fetches both projects
builds zlib only once.

On Linux the reader links `iconv` for legacy text conversion; macOS uses CoreFoundation and
Windows its own APIs.

The library does not select, fetch, link, or enable an XML implementation. `Reader::read` is
templated on a concrete implementation of `musx::xml::IXmlDocument`, in parallel with musxdom's
`DocumentFactory`. Only the tests fetch pugixml and instantiate the reader with its musxdom
adapter.

Set `FINALE_MUS_READER_BUILD_TESTING=OFF` when consuming the library without its tests.

Reader diagnostics and phase timing are controlled together by
`FINALE_MUS_READER_INSTRUMENTATION`, whose values are `AUTO`, `ON`, and `OFF`. `AUTO`
instruments the reader only when its tests are built and compiles instrumentation out for
ordinary clients. Clients that want `ImportReport` select `ON`; `OFF` disables it everywhere.

## API

```cpp
#include <finale_mus_reader/reader.h>

// XmlDocument must derive from musx::xml::IXmlDocument.
auto document = finale_mus_reader::Reader::read<XmlDocument>("legacy_score.mus");

// Parse optional resources once when importing multiple files.
finale_mus_reader::ReaderOptions options;
options.macSymbolFonts = macSymbolFontsBytes;            // Finale's MacSymbolFonts.txt, if available
options.percussionMappingXml = {{"SmartMusic SoftSynth.xml", percussionMappingXmlBytes}};
options.percussionMapConversionTable = percussionMapConversionTableBytes; // Finale's PercMapConversionTable.txt, if available
auto reader = finale_mus_reader::Reader::create<XmlDocument>(options);
auto anotherDocument = reader.read("another_legacy_score.mus");

#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
auto result = finale_mus_reader::Reader::readWithReport<XmlDocument>("legacy_score.mus");
const auto& report = result.report;
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
```

`ImportReport` identifies the selected epoch, byte order, source platform, recovered Finale
version, validated blocks, warnings, and the origin of every field the reader models
(`ValueOrigin`: read from the source, implied by the source era's behavior, left at the Finale
27 default, unmapped, or unavailable before MUSX). See
[docs/options_fallback.md](docs/options_fallback.md).

## How the reader works

The library owns its own document factory, built on musxdom's client construction interface:

1. `DocumentFactory::begin` starts a construction session for an empty document.
2. The pinned baseline matching the recovered source platform is inflated and parsed once with
   the caller's XML backend; an unknown platform falls back to macOS, and
   `ImportReport::defaultsPlatform` reports which baseline was used. Its complete `<options>`
   element seeds the options pool, and an explicit allowlist of option-like `<others>` nodes
   seeds the others pool. Nothing else in the baseline is ever created.
3. The header recovered from the MUS banner replaces the empty session header.
4. Every confidently decoded legacy value is overlaid onto the seeded objects.
5. `finish` hands the document to musxdom, which validates the pools and runs its resolvers
   once, after the overlays are in place.

Because the imported document is the only document the reader ever builds, fallback measures,
staves, entries, text, parts, layouts, or derived instrument state cannot leak into the result.

Legacy values are not read by hand-written per-field code. Through Finale 2006 the option and
other pools resolve to fixed 16-byte rows, so the reader presents each `(tag, cmper)` record
family as a word stream and describes every mapped field in a table:

```cpp
const FieldMapping spacingFields[] = {
    MUS_WORD(Target, "94", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 1, minWidth),
    MUS_WORD(Target, "94", GLOBALS_CMPER, /*incidence*/ 0, /*slot*/ 2, maxWidth),
};
```

One table corresponds to one musxdom class. A generic engine applies them all, records where
each recovered value came from, and reports every unmapped field at its synthesized default.
Word indices are absolute across incidences, so a four-byte value whose two words straddle a
row boundary resolves without special handling. Tables declare which epochs and Finale versions
they apply to; a later table supersedes an earlier row for the same field, so a field that
moves in a later version costs a one-row override rather than a restated table.
[docs/format_overview.md](docs/format_overview.md) describes the format as the reader relies on
it.

The embedded Finale 27 baseline is generated only when its authoritative gzip resource changes:

```bash
python3 scripts/generate_embedded_defaults.py          # regenerate
python3 scripts/generate_embedded_defaults.py --check  # verify, as CI does
```

## Test fixtures

`tests/evidence/` holds controlled documents saved by each supported Finale release, most as a
baseline plus a variant that changes one setting, together with ETF exports and Finale 27
conversions of the same documents. Every fixture was authored for this project and is released
under its license. [tests/evidence/README.md](tests/evidence/README.md) describes the layout.

## License

Legacy Finale MUS Reader is available under the [MIT License](LICENSE).
