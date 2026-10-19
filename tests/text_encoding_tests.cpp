// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "import/support/text_encoding.h"
#include "musx/factory/DocumentFactory.h"

using finale_mus_reader::text::codepointFromByte;
using finale_mus_reader::text::parseMacSymbolFonts;
using finale_mus_reader::text::toUtf8;
using finale_mus_reader::text::UnresolvedFontFallback;

using Bank = musx::dom::others::FontDefinition::CharacterSetBank;

namespace {

// Font-name bytes as Finale 2007 and 2008 documents store them. Each is a real macOS font of
// the era, and each decodes to legible text under exactly one encoding, which is what makes
// the charset value worth trusting rather than guessing.
const std::string kHiraginoMincho = "\x83\x71\x83\x89\x83\x4d\x83\x6d\x96\xbe\x92\xa9 Pro W3";
const std::string kHuawenFangsong = "\xbb\xaa\xce\xc4\xb7\xc2\xcb\xce";
const std::string kLiSong = "\xc4\xd7\xa7\xba Pro";
// MacRoman 0x8e is e-acute; the same byte is a capital Z-caron in Windows-1252.
const std::string kMacAccented = "C\x8elino";

} // namespace

/// @brief Whether a string is well-formed UTF-8, which every recovered string must be.
bool isValidUtf8(std::string_view value)
{
    for (std::size_t at = 0; at < value.size();) {
        const auto lead = static_cast<unsigned char>(value[at]);
        std::size_t length = lead < 0x80 ? 1 : (lead & 0xe0U) == 0xc0U ? 2 : (lead & 0xf0U) == 0xe0U ? 3 : (lead & 0xf8U) == 0xf0U ? 4 : 0;
        if (length == 0 || at + length > value.size()) {
            return false;
        }
        for (std::size_t i = 1; i < length; ++i) {
            if ((static_cast<unsigned char>(value[at + i]) & 0xc0U) != 0x80U) {
                return false;
            }
        }
        at += length;
    }
    return true;
}

TEST_CASE("Mac script codes select the encoding", "[text]")
{
    CHECK(toUtf8(kMacAccented, Bank::MacOS, 0) == "Célino");
    CHECK(toUtf8(kHiraginoMincho, Bank::MacOS, 1) == "ヒラギノ明朝 Pro W3");
    CHECK(toUtf8(kLiSong, Bank::MacOS, 2) == "儷宋 Pro");
    CHECK(toUtf8("\xb0\xa1", Bank::MacOS, 3) == "가");
    CHECK(toUtf8(kHuawenFangsong, Bank::MacOS, 25) == "华文仿宋");
    CHECK(toUtf8("\x81\x82", Bank::MacOS, 6) == "¹²");
    CHECK(toUtf8("\x80\x81\x82", Bank::MacOS, 7) == "АБВ");
    CHECK(toUtf8("\x81\x82", Bank::MacOS, 29) == "Āā");
}

TEST_CASE("Windows charset values select the encoding", "[text]")
{
    // ANSI is pinned rather than resolved against the running machine's code page, so the
    // same file decodes identically everywhere.
    CHECK(toUtf8(kMacAccented, Bank::Windows, 0) == "CŽlino");
    CHECK(toUtf8(kHiraginoMincho, Bank::Windows, 128) == "ヒラギノ明朝 Pro W3");
    CHECK(toUtf8("\xb0\xa1", Bank::Windows, 129) == "가");
    CHECK(toUtf8(kLiSong, Bank::Windows, 136) == "儷宋 Pro");
    CHECK(toUtf8("\xc0", Bank::Windows, 186) == "Ą");
    CHECK(toUtf8("\xa5", Bank::Windows, 238) == "Ą");
}

TEST_CASE("A charset that names no script falls to the bank default", "[text]")
{
    // Symbol fonts are not special-cased. musxdom's calcIsSymbolFont describes how
    // character codes in text *set in* the font map to glyphs; the font's own name is
    // ordinary platform text and is converted like any other.
    CHECK(toUtf8(kMacAccented, Bank::MacOS, 0xfff) == "Célino");
    CHECK(toUtf8(kMacAccented, Bank::Windows, 2) == "CŽlino");
    // Windows DEFAULT and OEM name no encoding either.
    CHECK(toUtf8(kMacAccented, Bank::Windows, 1) == "CŽlino");
    CHECK(toUtf8(kMacAccented, Bank::Windows, 255) == "CŽlino");
    // smUninterp (32) is the Script Manager's own "not text in any script" code, and
    // Finale's separate 0xfff sentinel means the same thing. A font's name is ordinary
    // platform text whatever its content is, so both convert as Mac Roman.
    CHECK(toUtf8(kMacAccented, Bank::MacOS, 32) == "Célino");
    // Nor does an unknown charset value. This is the starting position, to be revised when a
    // file demands it, rather than a claim about what Finale wrote.
    CHECK(toUtf8(kMacAccented, Bank::MacOS, 99) == "Célino");
    CHECK(toUtf8(kMacAccented, Bank::Windows, 77) == "CŽlino");
}

TEST_CASE("Document font character sets apply regardless of comparator", "[text]")
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    for (const auto cmper : {musx::dom::Cmper(0), musx::dom::Cmper(23)}) {
        auto font =
            std::make_shared<musx::dom::others::FontDefinition>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, cmper);
        font->name = cmper == 0 ? "Pmusic" : "P music";
        font->charsetBank = Bank::MacOS;
        font->charsetVal = 0;
        document->getOthers()->add(musx::dom::others::FontDefinition::XmlNodeName, font);
    }

    CHECK(toUtf8("\xb0", document, 0, UnresolvedFontFallback::Text) == "∞");
    CHECK(toUtf8("\xb0", document, 23, UnresolvedFontFallback::Text) == "∞");
}

TEST_CASE("An unresolved font uses the caller's content fallback", "[text]")
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    document->getHeader() = std::make_shared<musx::dom::header::Header>();
    document->getHeader()->textEncoding = musx::dom::header::TextEncoding::Mac;

    CHECK(toUtf8("\xb0", document, 23, UnresolvedFontFallback::Text) == "∞");
    CHECK(toUtf8("\xb0", document, 23, UnresolvedFontFallback::Symbol) == "°");
    CHECK(codepointFromByte(0xb0, document, 23, UnresolvedFontFallback::Symbol) == 0xb0);
}

TEST_CASE("MacSymbolFonts contents normalize one font name per line", "[text]")
{
    const std::string input = "  Pmusic\r\n\r\nGrace Notes  \nP music\n";
    const std::vector<std::uint8_t> bytes(input.begin(), input.end());
    const auto names = parseMacSymbolFonts(bytes);

    CHECK(names.size() == 2);
    CHECK(names.contains("pmusic"));
    CHECK(names.contains("gracenotes"));
}

TEST_CASE("Seven-bit names survive conversion unchanged", "[text]")
{
    // These go through a converter like everything else rather than short-circuiting on
    // ASCII, so this also asserts the conversion path is right for the bulk of real input:
    // every encoding here agrees with ASCII below 0x80.
    const std::string ascii = "Petrucci";
    CHECK(toUtf8(ascii, finale_mus_reader::SourcePlatform::MacOS) == ascii);
    CHECK(toUtf8(ascii, finale_mus_reader::SourcePlatform::Windows) == ascii);
    CHECK(toUtf8(ascii, Bank::MacOS, 1) == ascii);
    CHECK(toUtf8(ascii, Bank::Windows, 136) == ascii);
    CHECK(toUtf8(ascii, std::uint16_t{1}) == ascii);
    CHECK(toUtf8("", Bank::Windows, 136).empty());
}

TEST_CASE("Legacy font names convert to UTF-8", "[text]")
{
    CHECK(toUtf8(kHiraginoMincho, Bank::MacOS, 1) == "ヒラギノ明朝 Pro W3");
    CHECK(toUtf8(kHuawenFangsong, Bank::MacOS, 25) == "华文仿宋");
    CHECK(toUtf8(kLiSong, Bank::MacOS, 2) == "儷宋 Pro");
    CHECK(toUtf8(kMacAccented, Bank::MacOS, 0) == "Célino");
}

TEST_CASE("Mac Roman is not Windows-1252", "[text]")
{
    // The two agree below 0x80 and disagree above it. Treating a Mac Roman name as ANSI is
    // the specific mistake this mapping exists to prevent, and it is why a pre-3.2 file
    // synthesizes its bank from the document's own operating system.
    CHECK(toUtf8(kMacAccented, finale_mus_reader::SourcePlatform::MacOS) == "Célino");
    CHECK(toUtf8(kMacAccented, finale_mus_reader::SourcePlatform::Windows) == "CŽlino");
}

TEST_CASE("The single-byte Mac encodings agree on every platform", "[text]")
{
    // Each platform reaches these through a different route: CoreFoundation on macOS,
    // MultiByteToWideChar on Windows, and the embedded tables everywhere else, since musl's
    // iconv — and therefore Emscripten's — rejects all four outright. Asserting concrete
    // results is what keeps those three routes from drifting apart.
    CHECK(toUtf8("C\x8elino", Bank::MacOS, 0) == "Célino");
    CHECK(toUtf8("\xa5\xc7", Bank::MacOS, 0) == "•«");
    CHECK(toUtf8("\x81\x82", Bank::MacOS, 6) == "¹²");
    CHECK(toUtf8("\x80\x81\x82", Bank::MacOS, 7) == "АБВ");
    CHECK(toUtf8("\x81\x82", Bank::MacOS, 29) == "Āā");
    // Below 0x80 every one of them is ASCII.
    CHECK(toUtf8("Petrucci", Bank::MacOS, 6) == "Petrucci");
    CHECK(toUtf8("Petrucci", Bank::MacOS, 7) == "Petrucci");
    CHECK(toUtf8("Petrucci", Bank::MacOS, 29) == "Petrucci");
}

TEST_CASE("A name that contradicts its charset keeps its bytes", "[text]")
{
    // Preserved mojibake can be re-decoded once the encoding is understood; discarded bytes
    // cannot, and a thrown exception mid-import loses the whole document. What is preserved is
    // each byte as the code point of the same value, which is reversible and, unlike the bytes
    // themselves, is valid UTF-8. A string carrying a raw byte above 0x7f is not a
    // representation musxdom can hold or EnigmaXML can spell.
    const std::string notShiftJis = "\xff\xfe\xff\xfe";
    const auto converted = toUtf8(notShiftJis, Bank::Windows, 128);
    CHECK(converted == "\u00ff\u00fe\u00ff\u00fe");
    CHECK(isValidUtf8(converted));

    // An unassigned byte inside an otherwise convertible string takes the whole string down
    // the same path. 0x81 has no Windows-1252 meaning, and a real document was found writing
    // one in a font whose record claims that code page.
    const auto unassigned = toUtf8("\x81", Bank::Windows, 0);
    CHECK(unassigned == "\u0081");
    CHECK(isValidUtf8(unassigned));
}
