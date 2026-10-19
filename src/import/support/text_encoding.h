// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <cctype>
#include <concepts>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>

#include "finale_mus_reader/reader.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace text {

using SymbolFontNames = std::unordered_set<std::string>;

/// @brief Parses the contents of Finale's `MacSymbolFonts.txt`.
/// @details Each nonblank line is one font name. Names are stored in musxdom's normalized
/// form so every later lookup uses the same typeface comparison.
SymbolFontNames parseMacSymbolFonts(std::span<const std::uint8_t> contents);

inline constexpr std::uint8_t legacyCharsetBankBit = 13;
inline constexpr std::uint8_t legacyCharsetValueBits = 12;
inline constexpr int windowsAnsiCharset = 0;
inline constexpr int windowsDefaultCharset = 1;

/// @brief What bytes using a font absent from the document are known to represent.
enum class UnresolvedFontFallback {
    Text,
    Symbol,
};

/// @brief Converts legacy text through a font's character-set fields.
/// @details Legacy MUS stores text in whatever encoding the saving machine used and does
/// not record that encoding globally. What it does record is per font: `charsetBank` names
/// the platform whose charset numbering applies, and `charsetVal` selects within it. That
/// is enough to decode, and it is why conversion is driven from the font rather than from
/// the document's platform — a Mac font can appear in a document saved on Windows.
///
/// Where `charsetVal` names no script — a symbol
/// font, Windows `DEFAULT` or `OEM`, or an unrecognized value — the bank's own
/// default applies: Mac Roman for the Mac bank, Windows-1252 for the Windows bank. That is
/// a starting position to revise if a file contradicts it.
///
/// Symbol fonts are not special-cased. `calcIsSymbolFont` describes how character codes in
/// text *set in* that font map to glyphs; the font's own name is ordinary platform text.
std::string toUtf8(std::string_view source, musx::dom::others::FontDefinition::CharacterSetBank bank, int charsetVal);

/// @brief Converts legacy text through a packed font character set.
/// @details The high nibble names the bank and the low twelve bits name its character set.
/// This is the representation stored by a FontDefinition and repeated as the optional second
/// argument of a compressed-era Enigma font command.
std::string toUtf8(std::string_view source, std::uint16_t packedCharset);

/// @brief Converts fontless legacy text through the source platform's encoding.
/// @details Every name goes through a converter, including a name that is entirely 7-bit.
/// Skipping ASCII would give the same answer for every encoding here, since all of them
/// agree with ASCII below 0x80, but it would also mean the conversion path went untested
/// against the overwhelming majority of real input.
///
/// A failed conversion preserves each source byte as the code point of the same value. The
/// result remains valid UTF-8 and retains enough information for a better-informed pass.
std::string toUtf8(std::string_view source, SourcePlatform platform);

/// @brief Converts null-terminated UTF-16 code units to UTF-8.
/// @details Unpaired surrogates remain visible as their stored codepoint rather than being
/// discarded. Input after the first null code unit is ignored.
std::string utf16ToUtf8(std::span<const std::int16_t> source);

/// @brief Converts null-terminated little-endian UTF-16 bytes to UTF-8.
/// @details A final unpaired byte is ignored.
std::string utf16LeToUtf8(std::span<const std::uint8_t> source);

/// @brief Converts legacy text using the named document font.
/// @details A resolved font determines whether its bytes are text or glyph numbers. If the
/// font is absent, @p unresolvedFontFallback supplies that fact. Text uses the source
/// platform's default encoding; symbol bytes retain their numeric glyph values.
std::string toUtf8(
    std::string_view source, const musx::dom::DocumentPtr& document, musx::dom::Cmper fontId, UnresolvedFontFallback unresolvedFontFallback);

/// @brief Whether a byte is ASCII whitespace, for whichever integral or character type it
/// arrives as (`char`, `unsigned char`, `std::uint8_t`, and so on).
/// @details `std::isspace` is only well-defined for a value representable as `unsigned char`
/// or `EOF`, and is locale-dependent beyond ASCII; casting to `unsigned char` and rejecting
/// anything at or above 0x80 avoids both, and keeps a UTF-8 continuation byte -- always 0x80
/// or above -- from ever being misread as whitespace while trimming multibyte text.
template <std::integral T>
bool isSpace(T ch)
{
    const auto byte = static_cast<unsigned char>(ch);
    return byte < 0x80 && std::isspace(byte) != 0;
}

/// @brief Restates legacy line breaks in the convention EnigmaXML uses.
/// @details Legacy MUS separates the lines of a text block with a carriage return, which is
/// the classic Mac convention Finale was built on and which it kept on both platforms.
/// EnigmaXML uses a line feed, and carries no `&#xD;`. A carriage return left in place would
/// not survive an XML round trip either, since XML parsers normalize a literal one to a line
/// feed regardless.
///
/// A `\r\n` pair becomes one `\n`, so a file written by a Windows Finale is not turned into
/// double-spaced text.
std::string normalizeLineBreaks(std::string source);

/// @brief Converts one legacy font character to the code point musxdom stores.
char32_t codepointFromByte(
    std::uint8_t stored, const musx::dom::DocumentPtr& document, musx::dom::Cmper fontId, UnresolvedFontFallback unresolvedFontFallback);

} // namespace text
} // namespace finale_mus_reader
