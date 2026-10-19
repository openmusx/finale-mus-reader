# Decoder rules

How to choose between a structural marker, an epoch gate, and a version gate, and the
binary-safety rules every decoder honors. These are project rules.

Legacy MUS is a family of formats ([format_overview.md](format_overview.md)). Classification
uses observed framing, byte order, lengths, and checksum or codec validation; a decoder is never
selected from a marketing version alone. Physical framing stays separate from logical record
interpretation.

- Make all binary reads bounds-checked and overflow-safe.
- Preserve original offsets, raw values, selected byte order, format epoch, and provenance in
  diagnostics where useful.
- Validate declared sizes, decompressed sizes, CRCs, and complete input/output consumption as
  appropriate to the epoch.
- **Where the data or the record structure states which layout a file uses, read that instead
  of dating the file.** This outranks both gates below. Version coverage is incomplete and
  always will be; the Coda-banner era's Windows documents state no version at all; and a version
  read without the container's byte order gives a plausible wrong answer rather than no answer. A
  structural marker is also one step closer to the evidence, because a version boundary is
  usually inferred from the same observation the structure makes directly. Two mappings work this
  way: the clef tuple width comes from the payload size, and the whole stem family's units come
  from the size of its connection collection; a field that is quiescent in one era and packed in
  the next serves the same purpose. Say at the site why the marker is trustworthy, what it costs
  when the file is ambiguous, and whether a version gate would also have worked.
  A test over unbounded content is not a marker but a heuristic: font names are whatever a user
  or system could install, so no rule distinguishes a header incidence from the first bytes of
  every possible name, and that boundary rightly stays a version range. A marker is a fact about
  the record's shape, not a guess about its contents.
- Keep record layouts and option mappings explicitly version-aware, but **prefer an epoch gate
  to a version gate wherever the boundary is really the epoch.** A version gate is the more
  fragile instrument: it fails closed on any file whose version cannot be recovered, and it fails
  silently, leaving the class populated from reference defaults and every field reported as
  synthesized. That looks exactly like a document with nothing to recover. The Coda-banner era's
  Windows documents have no version at all, so a version-gated table skips them without a word.
- **Where a version gate is genuinely required, frame it inside the epoch it is gating.** A
  boundary that falls within one epoch — the font-definition header arriving in Finale 3.2,
  inside the uncompressed era — belongs to that epoch and is expressed as a version range on a
  table already restricted to it. Listing extra epochs alongside it can only ever be satisfied by
  a misread version, and bounding the range at both ends keeps a wild version from landing in the
  wrong case. Never invent a version to satisfy a gate: a file that does not state one is telling
  you something.
- **Never leave an epoch entirely uncovered by accident.** A gate that excludes a whole epoch
  says in a comment at the gate that the exclusion is intended and why: that the era does not
  store the class, that it stores it somewhere still unlocated, or that the evidence to place it
  is missing. An epoch silently absent from a gate is a defect, not a scope decision. Cover each
  epoch with at least one test, so that removing an epoch from a gate has to break something.
- Treat sharing, tag-specific fields, and early directory spans as open where the code labels
  them open.
- When a file contradicts an assumption, preserve the file's evidence and revise the assumption
  rather than forcing the sample through an expected layout.
