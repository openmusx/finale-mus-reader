---
name: implement-note-detail
description: Implement a legacy MUS importer for a musxdom NoteDetailsBase class, including note ID remapping after Entry import. Use for note-attached detail classes; use implement-class-importer for ordinary classes.
---

# Implement a note detail

Follow this repository's `implement-class-importer` skill for the class importer, complete field manifest, reporting, fixtures, tests, and epoch accounting. Inspect the target musxdom class and its XML mapping; identify its entry number, incidence, note ID, part sharing, and every persisted field. Keep the format layout in that class's translation unit.

## Note identity

Entry import records original and final Note IDs by Note index in `PendingReferences::noteIdsByEntryIndex` when it changes an ID. For Note-ID-based records, a detail's stored `noteId` refers to the original ID. Register each new `NoteDetailsBase` target with `registerNoteIdRemap(context, target, &Target::noteId)` after decoding its raw ID. The generic remapper runs in `ResolveStoredRefs`, after all importers, and changes a Note ID reference only when the original ID identifies one changed Note slot. It does not depend on registry order or the position of `noteId` in the class. Do not decode Entry a second time or add a musxdom setter solely for import.

Enable the shared Coda TCD-to-Note-ID process only for these classes:

- `NoteAlterations` recovered from old combined notehead records.
- `AccidentalAlterations` recovered from those same combined records.
- `TieAlterStart`.
- `TieAlterEnd`.

Omit all other classes, including `CrossStaff` and `DotAlterations`, from TCD conversion. They still require ordinary deferred Note ID remapping through `registerNoteIdRemap` with `NoteReferenceMode::NoteIdOnly`. Later split notehead and accidental records also use `NoteIdOnly`.

Verify whether each note detail class stores a TCD or a Note ID in each supported layout.
Report contradictions or uncertainty rather than silently changing the
class list. Use controlled fixtures to validate the representation and narrow uncertain gates.

For classes requiring the shared Coda conversion, select `combinedNoteReferenceMode(profile)` when registering. A Coda-banner document states the form its combined details use in its format flags (`SourceProfile::codaFormatFlags`): the selector resolves by TCD only when the Note ID flag is clear and by Note ID only otherwise, and every later epoch uses Note IDs. No version participates. Preserve an ambiguous reference with a diagnostic rather than inferring the target from detail incidence.

Keep the source ID in the report's raw value; label an adjusted target `LegacyMusAdjusted` where the class reports the final value. A duplicate source ID may name more than one Note. Look for source and conversion evidence that identifies its owner; otherwise preserve the stored ID and emit a diagnostic. Never pair by detail incidence or row order without evidence that establishes that relationship.

## Incidences

When a source family can omit or reject individual records, use the shared `EmittedIncidence` helper from `import/support/legacy_mapping.h` for each part and entry. Assign `current()` to the target, then add it to the pool through `EmittedIncidence::add`, which throws if the target does not carry the next incidence. This keeps emitted incidences consecutive even when source row positions have gaps. Preserve a stored incidence only when the class requires that exact identity, and verify such a requirement separately from Note ID matching.

## Checks

Test an unchanged ID, a unique changed ID, a rest ID changed by Entry, and an ambiguous duplicate ID. Assert the target's final ID, source raw value, origin, and musxdom `getForNote` lookup where a frame exposes the Note. Exercise score and linked-part details when the class supports them. Run the class test, production suite, formatting and reporting checks.
