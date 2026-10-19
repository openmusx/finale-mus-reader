# Legacy MUS format overview

What the reader relies on about the `.mus` format, stated as the reader understands it. Where a
statement is a belief rather than an established fact, it is labeled.

## The pipeline

1. **Recognize and classify** the container: confirm the file is a score, read the byte order it
   states, then validate its framing in that order.
2. **Decompress** per epoch: none, PKWARE DCL (`blast`), or zlib.
3. **Frame** typed blocks and pools; recover the banner header.
4. **Decode** records as word streams addressed by `(tag/selector, cmper, incidence, word slot)`.
5. **Overlay** decoded values onto a document seeded from the pinned Finale 27 baseline, then
   `finish` hands it to musxdom.

The reader builds exactly one document. There is no fallback document that could leak content.

## Epochs

Legacy MUS is a family of formats, not one stable binary layout. `FormatEpoch` names the four
container families:

| Epoch | Versions | Container |
|---|---|---|
| `CodaBanner` | 1.x–2.6 | Plain-text `Finale(TM) ...` banner at offset 0, chained pools, no `ENIGMA BINARY FILE` signature. Mac documents state a version; Windows documents state a platform (`PC`) and no version. |
| `UncompressedLegacy` | 3.x–2000 | Four uncompressed typed pools in platform byte order; no checksum. |
| `DclLegacy` | 2001–2006 | Typed blocks in platform byte order, PKWARE DCL compressed, with CRC-32 and stored block length. |
| `ZlibLegacy` | 2007–2012 | Typed zlib blocks with CRC-32 and stored block length; a variable-length class-record frame. Finale 2007 documents are of either byte order; 2008 onward is little-endian with rare exceptions. |

**Recognition comes first.** `recognize` decides from the file alone whether it is a legacy
score, and the reader refuses anything it does not accept. A signature-era file is a chain of
blocks, each a 16-bit type and a 32-bit length that includes its six-byte header, ending at a
type `0xffff` block or where fewer than six bytes remain. A score holds all four principal pools:

| Pool | Uncompressed | DCL | Zlib |
|---|---|---|---|
| others | `0x0001` | `0x000f` | `0x001a`, or `0x0014` / `0x0018` |
| details | `0x0002` | `0x0010` | `0x001b`, or `0x0015` / `0x0019` |
| entries | `0x0003` | `0x0011` | `0x0016` |
| text | `0x0004` | `0x0012` | `0x0017` |

A Finale binary missing any of them is a library or another non-score file; one whose blocks
overrun the file is damaged. The others pool's block number names the epoch, so only that
epoch's framing is then validated. A Coda-banner file is recognized by its banner and first pool
prologue. ETF, MUSX, lesson, and SmartMusic assessment files are named but not read.

Classification uses observed framing, byte order, lengths, and checksum or codec validation,
never a marketing version alone. An unrecognized banner-era framing still produces an
options-complete empty document with its recovered header rather than being mislabeled as a
known epoch.

## Record rows through Finale 2006

Every epoch through Finale 2006 stores its records in four pools in the same order — others,
details, entries, text — and only the container framing differs.

| Shape | Layout | Payload |
|---|---|---|
| other | comparator (2), tag (2), payload (12) | six 16-bit words |
| detail | comparator 1 (2), comparator 2 (2), tag (2), payload (10) | five 16-bit words |

- Rows are **16 bytes**, not 16 words. Entry rows are 38 bytes.
- The tag is a 16-bit value subject to the file's byte order like every other field; a
  little-endian file stores `FN` as the bytes `NF`.
- A logical record may span several rows (**incidences**) of the same comparator and tag.
  Word slots are numbered absolutely across incidences, so a 32-bit value whose halves straddle
  a row boundary resolves without special handling.
- **Unverified:** the word order within a 32-bit field. Each mapping that reads one states
  the order it assumes.
- The text pool is a byte stream of Enigma text chunks, not rows.

From Finale 2007 the pools become typed zlib blocks and records carry a class id and a payload
length. For numeric global options the class id is the numeric selector plus `0x000e`, with the
incidences coalesced into one payload; that relationship is not assumed for named tags or other
pools.

A zlib part record may carry a continuation the same size as its physical payload. Its initial
32-bit value repeats that payload length; the remaining bytes form an editable mask. The score
and part payload lengths may differ. The effective payload starts with the score bytes, extends
with zero bytes when the part is longer, then selects part bits under the mask. An omitted part
suffix inherits the score; an omitted score suffix remains zero except for selected part bits.
The two terminal words mask the part payload's final four bytes, which the continuation's length
prefix displaces.

## Terminology

- **Epoch** — one of the four container families above.
- **Selector / tag** — the numeric or two-character record family key; ETF spells it `^NN`.
- **Cmper** — the record's comparator key. `65534` (`GLOBALS_CMPER`) holds the document-wide
  option records.
- **Incidence** — one 16-byte row of a multi-row logical record.
- **Word slot** — a 16-bit word index, absolute across incidences.
- **`ValueOrigin`** — where a reported value came from; see [options_fallback.md](options_fallback.md).

## Invariants that shape the code

- When a class defines one incidence, read incidence 0 even if later rows share its key. Extra
  rows do not invalidate the first incidence.
- Where the data states its own layout, the reader reads that instead of dating the file. A
  structural marker outranks a version gate; an epoch gate outranks a version gate.
  ([decoder_rules.md](decoder_rules.md))
- Pre-Finale-2012 text is never re-encoded without that text's own font, down to a single
  stored character. ([text_encoding.md](text_encoding.md))
- Every fact and behavior has exactly one implementation. (`AGENTS.md`)
