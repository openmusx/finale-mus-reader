# Legacy text encoding

The rules for converting legacy MUS text to the UTF-8 that musxdom expects. Binding on every
text, symbol, or character conversion.

Legacy MUS stores text in whatever encoding the machine that saved it used; EnigmaXML and
musxdom are always UTF-8. Converting between the two is this project's job and not musxdom's,
which is why `src/import/support/text_encoding.*` exists here.

The encoding is named per font rather than per document: the font in force at that point in
the string names the code page through its own `charsetBank` (the platform's charset
numbering) and `charsetVal` (the selection within it), so a Mac font in a document saved on
Windows still decodes correctly. Before Finale 3.2 the font record carries no charset at all,
and the bank is synthesized from the document's own platform instead.

A font whose character set says symbol, which musxdom's `calcIsSymbolFont` decides, has no code
page and preserves the byte as the code point of the same value. `MacSymbolFonts.txt`, supplied
by the caller through `ReaderOptions::macSymbolFonts`, is an external override for character
interpretation: Finale treats a listed face's characters as symbol values without necessarily
changing the persisted `FontDefinition` charset to match. The reader applies that list while
importing font definitions: a matching definition retains its bank and receives musxdom's
symbol charset for that bank, the report keeps the stored charset as the raw value, and the
result is labeled `LegacyMusAdjusted`. The rule applies to both banks and never reinterprets a
Windows font as a Mac font. Comparator zero and definitions sharing its normalized name do not
independently confer symbol semantics.

**Never re-encode pre-Finale-2012 text to Unicode without using that text's own font.** This
applies to a single stored character exactly as it applies to a run of text: a clef character,
a stem-connection symbol, a custom line style's character, and a text symbol insert are each a
byte in the encoding of the font their own record names, not a code point. Decoding one through
the wrong encoding does not merely garble it — it names a different glyph, which is why a
symbol font's byte survives untouched rather than being read as Mac Roman.
`text::codePageForDocumentFont` answers the question once, and `text::codepointFromByte` applies
the answer to one character.

Where no font names an encoding, the fallback is the platform default: Mac Roman on Mac and
Windows-1252 on Windows. That fallback is `text::platformCodePage` and belongs only to text that
genuinely has no font — the File Info header strings, the name inside a font command, literal
text before any font command in a block, and the lyric punctuation string. It is not restated
anywhere else.

Legacy line breaks are carriage returns and become line feeds. **A trailing line break is
content and is kept**, although Finale's own upgrade discards it: a break the source stores is
content, and dropping it needs a reason better than the upgrade doing so.

**Aim for the best result obtainable on the machine that is running.** Conversion need not be
bit-identical across platforms, and insisting on that would mean giving up real accuracy:
Windows can name encodings iconv cannot, so it gets the more faithful code page rather than
being held to a common subset. Where a platform must fall back, say so next to the fallback.

Choices that no document settles — an unknown charset value, for instance — are starting
positions, not findings. They are labeled as such in the code and revised when a file demands
it.
