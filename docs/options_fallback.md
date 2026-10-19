# Options fallback and `ValueOrigin`

The sequence that seeds a structurally complete options pool from the pinned Finale 27
baseline, and the `ValueOrigin` values that report where each value came from. Project rule;
the baseline resources are fixed and hashed ([embedded_defaults.md](embedded_defaults.md)).

musxdom expects a structurally complete options pool. The reader builds it in this order:

1. Select the pinned Finale 27 macOS or Windows baseline explicitly, preferring a
   source-platform match when reliable.
2. Inflate the embedded gzip bytes with zlib and parse the raw EnigmaXML with the caller's
   musxdom XML backend.
3. Seed only the complete options pool (and the allowlisted option-like `<others>` nodes) into
   the imported document.
4. Overlay every confidently recovered legacy option value.
5. Leave absent, unknown, or unsupported values at the Finale 27 default. Prefer this even where
   the source era's behavior is known, whenever the baseline already carries the value that
   behavior implies: the baseline is as fixed as a constant, and a value asserted in code beside
   a baseline that already agrees is a second copy of the same fact.
6. Report recovered values separately from synthesized defaults, and separately again from
   values determined by how the source version behaved when it had no option to store them.

## `ValueOrigin`

| Origin | Meaning |
|---|---|
| `LegacyMus` | Read from the source document. |
| `LegacyMusAdjusted` | Read from the source and then adjusted by a documented rule (for example a symbol-font charset override); the raw value is retained in the report. |
| `LegacyBehavior` | Not stored by the source era; the value is what that era always did. Reserved for a value the baseline does **not** already supply, or supplies wrongly — an era that always did something later versions let you turn off, where reading the later location would assert the opposite. Where a capture pass establishes such a field before the mapping tables run, the tables leave its entry alone. |
| `Finale27Default` | A known member of the recovery model whose applicable source layout does not supply it and for which no legacy behavior overrides the baseline. |
| `Unmapped` | The field could have a legacy source, but none has been located in any layout or epoch. A class is not partial merely because a field reports this. |
| `MusxOnly` | The field postdates every supported legacy layout and cannot be recovered. |
| `OutOfScope` | The source carries the field, but its recovery is deliberately excluded. |

`Unmapped`, `MusxOnly`, and `OutOfScope` never collapse into `Finale27Default`. In each case the value
remains default-initialized for a source-owned object and stays at the seeded Finale 27 value
for an options object. The distinction keeps an importer that maps a field in one epoch and
deliberately falls back in another from looking like an importer that never investigated the
field at all. Blanket initialization never overwrites `LegacyMus` or `LegacyBehavior`, and an
untouched field is never promoted from `Unmapped` merely because its default happens to be
right.

## What is never seeded

Fallback measures, staves, entries, text, document identity, header values, and other score
objects are never imported into an imported document. A class-specific importer may copy an
individually reviewed scalar from one of those objects when the applicable source structure
provably has no location for it; such an exception is an explicit field allowlist, never an
object clone, and document-local identities and references remain excluded. The imported
document is the only document the reader builds, so nothing else can own what it carries.
