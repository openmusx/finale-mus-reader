// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <iterator>
#include <memory>

#include "import/support/text_encoding.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using FontDefinitionTarget = musx::dom::others::FontDefinition;

// Legacy MUS stores a font name in whatever encoding the machine that saved it used, and
// names the encoding nowhere except in this same record: `charsetBank` selects the
// platform's charset numbering and `charsetVal` selects within it. musxdom expects UTF-8,
// because EnigmaXML is always UTF-8, so the name is converted here once both fields are in
// hand.
//
// Every name goes through a converter, including one that is entirely 7-bit. Every encoding
// here agrees with ASCII below 0x80, so the answer is the same either way, but skipping
// ASCII would leave the conversion path untested against the bulk of real input. A name
// whose bytes contradict the charset it claims is left exactly as it was found rather than
// replaced or dropped: preserved mojibake can be re-decoded later, discarded bytes cannot.
void convertNameToUtf8(void* instance, const SourceProfile&, const musx::dom::DocumentPtr&)
{
    auto* font = static_cast<FontDefinitionTarget*>(instance);
    font->name = text::toUtf8(font->name, font->charsetBank, font->charsetVal);
}

int symbolCharsetForBank(FontDefinitionTarget::CharacterSetBank bank)
{
    return bank == FontDefinitionTarget::CharacterSetBank::Windows ? FontDefinitionTarget::SYMBOL_CHARSET_WIN
                                                                   : FontDefinitionTarget::SYMBOL_CHARSET_MAC;
}

// MacSymbolFonts names faces whose stored charset may not express their glyph-number
// semantics. Normalizing each matching definition once makes the constructed document
// self-describing, so every later decoder and document consumer can rely on the font itself.
void applyConfiguredSymbolFonts(const ImportContext& context)
{
    if (!context.profile.symbolFontNames) {
        return;
    }
    for (const auto& font : context.document->getOthers()->getArray<FontDefinitionTarget>(musx::dom::SCORE_PARTID)) {
        if (!context.profile.symbolFontNames->contains(musx::dom::normalizeFontName(font->name))) {
            continue;
        }
        const auto adjusted = symbolCharsetForBank(font->charsetBank);
        if (font->charsetVal == adjusted) {
            continue;
        }
        const auto mutableFont = std::const_pointer_cast<FontDefinitionTarget>(font);
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto key = reporting.template instanceKey<FontDefinitionTarget>(musx::dom::SCORE_PARTID, font->getCmper());
            if (auto* info = reporting.report().findField(key, "charsetVal")) {
                info->origin = Reporting::Origin::LegacyMusAdjusted;
            } else {
                reporting.report().setField(key, "charsetVal", {Reporting::Origin::LegacyMusAdjusted, 0, 0, font->charsetVal});
            }
        });
        mutableFont->charsetVal = adjusted;
    }
}

// Before Finale 3.2 the font record carries no charset at all, so `charsetBank` and
// `charsetVal` would otherwise sit at their constructed defaults and say nothing about this
// document. Rather than pick an encoding here, synthesize the bank from the file's own
// operating system and then convert exactly as every other era does: `charsetVal` stays 0,
// which already means Mac Roman under the Mac bank and ANSI under the Windows bank.
//
// **Unverified: the synthesized bank.** Every pre-3.2 font name seen is 7-bit, so no name is
// affected either way and nothing yet distinguishes this choice from another. It stops being
// arbitrary the moment an 8-bit pre-3.2 name appears.
//
// An unclassified platform is treated as Mac. Coda-banner files carry no platform tuple at all,
// and nothing earlier than Finale 3.0 is Windows-origin, so Mac is the only origin those
// documents can have.
void convertEarlyNameToUtf8(void* instance, const SourceProfile& profile, const musx::dom::DocumentPtr& document)
{
    auto* font = static_cast<FontDefinitionTarget*>(instance);
    font->charsetBank =
        profile.platform == SourcePlatform::Windows ? FontDefinitionTarget::CharacterSetBank::Windows : FontDefinitionTarget::CharacterSetBank::MacOS;
    convertNameToUtf8(instance, profile, document);
}

// A font definition is one `FN` family. Its first incidence is the header and every
// incidence after it carries the name.
//
// The header's first word packs the character set: the high nibble names the bank, 1 for a
// Mac font and 2 for a Windows font, and the remaining twelve bits are the character set
// value. This says where the *font* came from, not where the document was written: a Mac
// font can appear in a document saved on Windows.
//
// The values agree with musxdom's own documentation of the field, which records 0xfff as the
// macOS symbol character set and 2 as the Windows one. A Mac symbol font stores 0x1fff and a
// Mac text font 0x1000.
//
// The header's second word carries the pitch in its low nibble and the family in its high
// nibble. It is non-zero only for Windows-bank fonts, Mac-bank fonts always storing zero, which
// is consistent with a Windows font-selection attribute that has no Mac equivalent.
//
// The remaining four words are unused, and are zero everywhere.
const FieldMapping fontFields[] = {
    // Bit 13 distinguishes the two banks: the nibble holds 1 for Mac and 2 for Windows, so
    // only a Windows font sets it. An explicit conversion keeps the enum honest rather than
    // relying on the legacy encoding happening to match musxdom's enumerators.
    MUS_BITS_AS(FontDefinitionTarget, "FN", CMPER_FROM_TARGET, /*incidence*/ 0, /*slot*/ 0, text::legacyCharsetBankBit, 1, charsetBank,
        value != 0 ? FontDefinitionTarget::CharacterSetBank::Windows : FontDefinitionTarget::CharacterSetBank::MacOS),
    MUS_BITS(FontDefinitionTarget, "FN", CMPER_FROM_TARGET, /*incidence*/ 0, /*slot*/ 0,
        /*firstBit*/ 0, text::legacyCharsetValueBits, charsetVal),
    MUS_BITS_AS(FontDefinitionTarget, "FN", CMPER_FROM_TARGET, /*incidence*/ 0, /*slot*/ 1,
        /*firstBit*/ 0, /*bitCount*/ 8, pitch, value & 0x0F),
    MUS_BITS_AS(FontDefinitionTarget, "FN", CMPER_FROM_TARGET, /*incidence*/ 0, /*slot*/ 1,
        /*firstBit*/ 0, /*bitCount*/ 8, family, value & 0xF0),
    // The name runs from the second incidence to the end of the family, so a long name
    // simply occupies more rows. It is read as bytes because character payloads are not
    // byte-order sensitive, and it ends at the first NUL.
    MUS_TEXT(FontDefinitionTarget, "FN", CMPER_FROM_TARGET, /*firstIncidence*/ 1, name),
};

// Before Finale 3.2 there is no header incidence: the family opens with the name. The character
// set is therefore unavailable for these documents rather than merely unmapped. This table
// carries only the name; the bank is synthesized from the file's operating system by the
// finalizer above.
//
// The split is by version, not by platform: both platforms behave alike on either side of the
// boundary. A file carrying the header would truncate at the first NUL of its packed charset
// word if read this way, so names that come out clean are positive evidence that no header is
// present.
//
// Unverified: the exact release between 3.0 and 3.2 that introduced the header.

const FieldMapping earlyFontFields[] = {
    MUS_TEXT(FontDefinitionTarget, "FN", CMPER_FROM_TARGET, /*firstIncidence*/ 0, name),
};

// From Finale 2007 the record is class-identified and length-governed rather than a fixed
// row, so the same fields are addressed by byte offset inside one record's payload. The
// character set encoding survived the change unchanged, which is why the bit positions below
// match the earlier table exactly; only the addressing differs.
constexpr records::LegacyTag fontDefinitionClass = 0x0090;
constexpr std::uint8_t charsetOffset = 0;
constexpr std::uint8_t pitchFamilyOffset = 2;
constexpr std::uint8_t nameOffset = 12;

const FieldMapping classFontFields[] = {
    MUS_CLASS_BITS_AS(FontDefinitionTarget, fontDefinitionClass, charsetOffset, text::legacyCharsetBankBit, 1, charsetBank,
        value != 0 ? FontDefinitionTarget::CharacterSetBank::Windows : FontDefinitionTarget::CharacterSetBank::MacOS),
    MUS_CLASS_BITS(FontDefinitionTarget, fontDefinitionClass, charsetOffset, 0, text::legacyCharsetValueBits, charsetVal),
    MUS_CLASS_BITS_AS(FontDefinitionTarget, fontDefinitionClass, pitchFamilyOffset, 0, 8, pitch, value & 0x0F),
    MUS_CLASS_BITS_AS(FontDefinitionTarget, fontDefinitionClass, pitchFamilyOffset, 0, 8, family, value & 0xF0),
    // The payload is length-governed, so the name simply runs to its end. A longer name grows
    // the record rather than spilling into another incidence.
    MUS_CLASS_TEXT(FontDefinitionTarget, fontDefinitionClass, nameOffset, name),
};

bool sourceHasFontHeader(const SourceProfile& profile)
{
    return sourceAtOrAfter(profile, FormatEpoch::UncompressedLegacy, versions::finale3_2);
}

bool sourcePredatesFontHeader(const SourceProfile& profile)
{
    return profile.version && !sourceHasFontHeader(profile);
}

// The Coda-banner epoch, which needs no version test at all.
//
// That epoch lies entirely below the header boundary, so the early layout always applies to it
// and asking for a version can only lose files. It does lose them: this era's Windows documents
// state a platform where its Mac documents state a version, so `PC 1.0+` yields no version and
// a version-gated table skips the document silently, leaving every font name empty.
//
// It shares @ref earlyFontFields with the table above rather than restating the layout, because
// the layout is the same fact; only the question of which files it applies to differs.
const MappingTable& codaFontDefinitionsTable()
{
    static const MappingTable table{.reportPrefix = "others.fontName",
        .epochs = EpochMask::CodaBanner,
        .targetKind = TargetKind::OthersFromRecords,
        .recordIdentity = records::packTag("FN"),
        .createTarget = &createOthersTarget<FontDefinitionTarget>,
        .fields = earlyFontFields,
        .fieldCount = std::size(earlyFontFields),
        .finalizeTarget = &convertNameToUtf8};
    return table;
}

const MappingTable& classFontDefinitionsTable()
{
    static const MappingTable table{.reportPrefix = "others.fontName",
        .epochs = EpochMask::Zlib,
        .encoding = RecordEncoding::ClassRecord,
        .targetKind = TargetKind::OthersFromRecords,
        .recordIdentity = fontDefinitionClass,
        .createTarget = &createOthersTarget<FontDefinitionTarget>,
        .fields = classFontFields,
        .fieldCount = std::size(classFontFields),
        .finalizeTarget = &convertNameToUtf8};
    return table;
}

const MappingTable& fontDefinitionsTable()
{
    static const MappingTable table{.reportPrefix = "others.fontName",
        // Fixed-row epochs only. A Coda-banner document can never be Finale 3.2 or later, so
        // listing that epoch here could only ever be satisfied by a misread version.
        .epochs = EpochMask::FixedRow,
        .sourceApplies = &sourceHasFontHeader,
        .targetKind = TargetKind::OthersFromRecords,
        .recordIdentity = records::packTag("FN"),
        .createTarget = &createOthersTarget<FontDefinitionTarget>,
        .fields = fontFields,
        .fieldCount = std::size(fontFields),
        .finalizeTarget = &convertNameToUtf8};
    return table;
}

const MappingTable& earlyFontDefinitionsTable()
{
    static const MappingTable table{.reportPrefix = "others.fontName",
        // The uncompressed epoch straddles the boundary, so here the version decides.
        .epochs = EpochMask::FixedRow,
        .sourceApplies = &sourcePredatesFontHeader,
        .targetKind = TargetKind::OthersFromRecords,
        .recordIdentity = records::packTag("FN"),
        .createTarget = &createOthersTarget<FontDefinitionTarget>,
        .fields = earlyFontFields,
        .fieldCount = std::size(earlyFontFields),
        .finalizeTarget = &convertEarlyNameToUtf8};
    return table;
}

} // namespace

void importFontDefinitions(const ImportContext& context)
{
    // Four layouts, one destination. The uncompressed epoch straddles the Finale 3.2 header
    // boundary and so needs both of the first two; the Coda epoch is decided by its epoch
    // alone, and the zlib epoch reads the same definitions as class records. Listing them
    // together is what lets one file match exactly one of them.
    applyMappingTables({&fontDefinitionsTable(), &earlyFontDefinitionsTable(), &codaFontDefinitionsTable(), &classFontDefinitionsTable()},
        context.index, context.profile, context.document, context.report);
    applyConfiguredSymbolFonts(context);
}

} // namespace others
} // namespace finale_mus_reader
