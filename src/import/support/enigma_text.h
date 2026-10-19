// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "import/support/text_encoding.h"
#include "musx/dom/CommonClasses.h"
#include "musx/dom/Document.h"

namespace finale_mus_reader {
namespace text {

/// @brief One legacy Enigma text record, restated in the spelling musxdom parses.
/// @details Legacy MUS keeps a text block as Enigma text, the same `^command(...)` language
/// EnigmaXML still uses, so most of a record survives verbatim. Three things do not, and
/// they are what this conversion exists for:
///
/// - the bytes are in a code page named by whichever font is in force at that point, while
///   EnigmaXML is always UTF-8;
/// - the fixed-row eras spell text styles as a run of `^efx(name)` commands where modern
///   Enigma packs them into one `^nfx(bits)`;
/// - the compressed eras write commands in a binary form -- a caret, a one-byte code, and
///   an argument spelled as hexadecimal digits offset by one -- that no modern parser reads.
///
/// What comes out is Enigma text, not plain text: inserts, sizes, and font changes are kept
/// so that musxdom's own parser can resolve them against the imported document.
struct ConvertedEnigmaText
{
    /// @brief The record in modern Enigma spelling, encoded UTF-8.
    std::string text;
    /// @brief Binary command codes that could not be read, deduplicated.
    /// @details Either no modern spelling is known for the code, or its argument was not the
    /// width recorded for it -- which would mean the recorded width is wrong. Both cases drop
    /// the command from @ref text rather than guess at it, and name it here so a caller can
    /// report it. The table grows by adding a code, not by changing any caller, and a command
    /// nobody reports is the one thing that silently loses content.
    std::vector<std::uint8_t> unreadCommandCodes;
    /// @brief `^efx` effect names with no known bit, deduplicated.
    std::vector<std::string> unknownEffectNames;
    bool fontWasSynthesized{};
    bool sizeWasSynthesized{};
    bool effectsWereSynthesized{};
};

/// @brief Document-local results of resolving Enigma font names to font comparators.
struct EnigmaFontResolutionCache
{
    std::unordered_map<std::string, std::optional<musx::dom::Cmper>> fontIdsByName;
};

/// @brief What a record needs in order to be read: a document to resolve fonts against, and
/// how its bytes are encoded.
struct EnigmaTextSource
{
    /// @brief The document being built. Font commands resolve against its `FontDefinition`
    /// pool, which is why the text pool must be imported after the font definitions.
    const musx::dom::DocumentPtr& document;
    /// @brief Whether the record's bytes are already UTF-8.
    /// @details True from Finale 2012, which converted stored text to Unicode. Such a record
    /// needs no code page at all, and its binary command codes appear as the two-byte UTF-8
    /// spelling of the same value.
    bool utf8{};
    /// @brief The source platform for text that no font or packed character set identifies.
    SourcePlatform platform = SourcePlatform::Unknown;
    /// @brief The text class's document default, used until an explicit font command changes it.
    /// @details Its face also supplies the initial font command when the record omits one, and
    /// its size and effects complete an otherwise partial initial formatting state.
    std::shared_ptr<const musx::dom::FontInfo> initialFont;
    /// @brief Optional cache shared by records whose font definitions cannot change.
    EnigmaFontResolutionCache* fontResolutionCache{};
};

/// @brief Converts one legacy Enigma text record body to the modern, UTF-8 spelling.
/// @param body The record's bytes, between its `^keyword(n)` header and its `^end`.
[[nodiscard]] ConvertedEnigmaText toModernEnigmaText(std::span<const std::uint8_t> body, const EnigmaTextSource& source);

/// @brief Completes the initial face, size, and effects commands from @p defaultFont.
/// @param value A modern Enigma string whose explicit commands must be preserved.
/// @param defaultFont The document default for the text class containing @p value.
[[nodiscard]] std::string initializeEnigmaTextFontState(std::string value, const musx::dom::FontInfo& defaultFont, bool* fontWasSynthesized = nullptr,
    bool* sizeWasSynthesized = nullptr, bool* effectsWereSynthesized = nullptr);

} // namespace text
} // namespace finale_mus_reader
