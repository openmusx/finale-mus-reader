---
name: implement-class-importer
description: Add or extend legacy Finale MUS recovery of one musxdom DOM class in this reader. Use when the user asks to implement, recover, decode, or map a musxdom class from legacy .mus files and supplies (or the code already has) the record locations to read. Covers the field manifest, mapping tables, the ClassImporter and its registry row, timing instrumentation, fixtures, provenance, tests, and documentation.
---

# Implement a class importer

Take one musxdom class from known record locations to a tested importer. Keep the physical
layout, its semantic interpretation, the implementation, and its reporting distinct so that a
later layout can revise one without destabilizing the others.

## Hard rules

- Read `AGENTS.md`, then `docs/format_overview.md`, then the class's existing importer if it
  has one (`src/import/<pool>/<name>.cpp`) and its test file (`tests/classes/<pool>/<name>_tests.cpp`).
- Do not add the target class's selectors, class ids, payload layouts, flags, version behavior,
  sharing behavior, or recovery status to `docs/format_overview.md`.
- Treat every locator or structure hint, including one from the user, as a hypothesis until a
  fixture validates it with bytes. This does not delay implementing
  conversions for any epoch. Label uncertain format claims **Believed** or **Unverified**
  in the code per the `comment-production-code` skill.
- Inspect the musxdom class and its EnigmaXML parsing behavior directly. Construct that class;
  never introduce a parallel document model or duplicate behavior musxdom already implements.
- Keep physical framing separate from logical interpretation. Choose the gating instrument by
  the hierarchy in `docs/decoder_rules.md`: structural marker first, then epoch gate, then a
  version gate framed inside its epoch. No epoch may be silently uncovered.
- ETF and Finale 27 conversions may normalize, synthesize, reorder, renumber, or discard source
  data. Tests of source decoding therefore assert the legacy representation directly.
- Report value origin per `docs/options_fallback.md`. A value the pinned baseline already
  supplies is not asserted again in code.
- **An implemented class is field-complete before it is recovery-complete.** Inventory every
  persisted musxdom field, including leaves of contained objects and every fixed collection
  element, directly from the class and its XML mapping. Give every leaf a report entry from the
  first implementation. A field is never omitted because its source is unknown or because its
  value equals a default.
- Keep every project-owned source file unity-build clean and formatted
  (`docs/code_conventions.md`; `python3 scripts/check_format.py --fix`).
- Do not commit, push, or publish unless the user separately requests it.

**Implement the complete conversion across ALL EPOCHS in the first pass, regardless of
available evidence.** Complete the implementation and tests before coverage captures. Use
evidence afterward to validate and refine the implementation; missing fixtures must not defer
an epoch or justify leaving its conversion at synthesized defaults.

## Step 1 — Define the vertical slice

Identify:

1. The fully qualified musxdom class and pool (`options`, `others`, `details`, `texts`),
   including comparator, incidence, sharing, and part semantics.
2. Every persisted field and fixed collection element. List its C++ type, default, XML name or
   contained path, enum meaning, units, bit semantics, whether it is a reference, and its
   initial mapping status.
3. The epochs in scope and the physical locators supplied for each.
4. The fallback behavior expected when the source lacks the class or a field.

## Step 2 — Establish the musxdom semantics

Search the configured musxdom source for the class declaration, `XmlNodeName`, XML field
names, enum definitions, constructors, pool accessors, and any custom parse or serialization
logic. Also inspect nearby classes for the idiomatic construction path.

Determine before decoding bytes:

- whether an object is seeded from the reference document or created from source records;
- which fields musxdom already transforms, validates, or derives;
- whether booleans are individual XML properties but packed in MUS;
- whether identifiers are document-local cmpers that require referential resolution;
- whether absence differs semantically from zero or an empty collection.

Never restate a musxdom enum or normalization rule in reader code when it can be used directly.

## Step 3 — Implement the complete conversion across all epochs

Prefer the table-driven mapping framework when a field has a stable source location and a
direct assignment or bit extraction. Add a class-specific file under the class's pool directory
(`src/import/options/`, `src/import/others/`, `src/import/details/`, `src/import/texts/`). The
shared machinery lives in `src/import/support/legacy_mapping.*`; there is no intermediate
`mappings/` directory.

**Expose one `ClassImporter` per musxdom class from that pool's `<pool>.h`, and register that
single entry point in the registry in `legacy_mapping.cpp`.** The registry states which classes
are imported and in what order, and nothing else. Everything era-specific belongs inside the
class's own translation unit: how many physical layouts it has, which epoch or version gate
selects each, whether a capture pass must build a collection before the scalar tables overlay
it, and what must be checked once they have run. Registry order is not a contract: apart from the
bootstrap pair the registry calls out, an importer builds the same document wherever it appears,
and entries are alphabetical within each pool. An importer that needs another class's objects
registers a deferred step with `PendingReferences::defer`, which runs after every importer. The
`DeferredStage` it names is its dependency: choose the earliest stage whose comment guarantees what
the step reads, and never rely on another step of the same stage. Steps that each allocate a
comparator belong in one `defer(stage, sequence)` call, so the importer's own object order decides
the numbers. Test that reversing deferred steps within a stage leaves the result unchanged.

**Give every new importer a timing phase in the same change.** Add one aggregate
`timing::Phase` to `src/reader/timing.h`, name it in `src/reader/timing.cpp`, and use it on the
importer's `FINALE_MUS_READER_IMPORTER` registry row. Use the timing macros for any nested phase
so a build without `FINALE_MUS_READER_ENABLE_INSTRUMENTATION` excludes them completely.

**`<pool>.h` declares importers and nothing else.** A stage a test drives on its own goes in
`<pool>/test_access.h`, which no library code may include; a stage no test drives is
file-local. Do not export a stage for symmetry with a neighboring class.

Choose target construction deliberately:

- `OptionsSingleton`: overlay a complete seeded options object.
- `OthersByCmper`: overlay reference-seeded objects only when their identities are genuinely
  stable and unmatched source records should not create objects.
- `OthersFromRecords`: construct source-owned pooled objects by source comparator.
- A bespoke capture pass: for variable-length collections, physical-to-semantic reordering,
  cross-record assembly, reference repair, or transformations requiring several fields at once.

If another pool needs a new construction policy, extend the shared framework once; do not hide
a second mapping engine in the new class file.

Before adding source mappings, create the class's complete field manifest in the report path.
Initialize every unresolved source-owned leaf to its musxdom default with `Unmapped` provenance;
for options, retain the seeded value while marking the unresolved leaf `Unmapped`. Mark a field
`MusxOnly` only when it postdates every supported legacy layout. A later mapping changes only the
fields it establishes. Blanket initialization never overwrites `LegacyMus` or `LegacyBehavior`.

For cmper fields, never compare or copy ids across documents as identities. Resolve the referent
on each side. If fallback requires cloning a reference object, reuse an equivalent target
object when safe; otherwise allocate the next sequential target cmper. Define cmper-zero
semantics explicitly.

Route warnings through the report or musxdom's logging channel.

## Step 4 — Prove the draft on controlled fixtures

After implementing all epochs, validate mappings with controlled fixtures where available: a baseline saved from a new document in the
relevant Finale release, and a variant that changes the one setting under test, so that the
record words that move between the two locate the field. Place fixtures under
`tests/evidence/<era>/` following `tests/evidence/README.md`, and add each file's SHA-256 and a
one-paragraph description of what it changes to that directory's `provenance.txt`. Prefer a
baseline plus one changed field over a realistic score whose unrelated changes create noise.

Do not wait for those fixtures to implement or test a conversion. Use constructed records for
the supported layouts and boundaries, and distinguish these tests from independent evidence.
Add focused tests in `tests/classes/<pool>/<name>_tests.cpp` for:

- an exact comparison between the class's persisted field manifest and the report's field set,
  so adding or overlooking a field cannot silently reduce coverage;
- every unresolved or MUSX-only field appearing in the report with its default value and
  `Unmapped` or `MusxOnly` origin, rather than being absent;
- one exact fixture from every claimed physical layout and byte order;
- each implemented version boundary;
- exact target values plus `ImportReport` origin and raw values;
- packed flags, signed values, longs, enum translations, and reference resolution as
  applicable;
- absent records retaining defaults or producing no object, according to policy;
- malformed, truncated, incomplete-incidence, or trailing-byte behavior where the new decoder
  introduces a boundary;
- no score content leaking from the reference document.

Test the source representation independently of any companion: a passing comparison against a
Finale 27 conversion cannot prove that the original bytes were decoded correctly.

Run the focused tests, the full suite, `python3 scripts/check_format.py`,
`python3 scripts/check_reporting_boundary.py`, `git diff --check`, and `git status --short`.

## Completion standard

Finish with a concise account of the implemented class and fields, supported epochs and version
gates, fixtures and tests, synthesized fallback, every still-unmapped field, every MUSX-only
field, and remaining open layouts. Call the result partial whenever any epoch or field remains
unsupported.

**State every completely uncovered epoch explicitly, every time.** Name the epoch, say whether
the exclusion is intended or merely unevidenced, and give the reason. Saying "supported for X,
Y and Z" leaves the reader to notice that W is missing. Say that W is uncovered.
