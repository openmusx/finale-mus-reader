// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/support/enigma_text.h"
#include "reader/timing.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <string_view>

#include "musx/musx.h"

namespace finale_mus_reader {
namespace text {
namespace {

using musx::dom::Cmper;
using FontDefinitionSource = musx::dom::others::FontDefinition;

// The effect names the fixed-row eras spell out, and the modern style bit each one sets.
//
// The order is the classic Mac QuickDraw style bit sequence -- plain, bold, italic, underline,
// outline, shadow, condense, extend -- which is the order Finale's own effect table uses.
// `bold` at 0x01, `italic` at 0x02 and `absolute` at 0x40 are established. The rest occupy
// their QuickDraw positions.
//
// **Unverified: `strikeout` and `hidden`.** They are musxdom's names for the 0x20 and 0x80
// bits, and QuickDraw calls 0x20 condense, so either spelling may be wrong. A wrong name here
// costs one ignored effect and a diagnostic naming the spelling the file actually used, which
// is what would correct it.
//
// musxdom supplies every bit value, so this table states only which name reaches which bit.
// The two exceptions are `outline` and `shadow`, which the modern format dropped and which
// therefore have no musxdom constant; they are listed with the position they occupy so that a
// file carrying one is neither reported unknown nor folded into a neighboring style.
constexpr std::uint16_t enigmaStyleOutline = 0x08;
constexpr std::uint16_t enigmaStyleShadow = 0x10;

struct EffectName
{
    std::string_view name;
    std::uint16_t bit;
};

constexpr EffectName effectNames[] = {
    {"bold", musx::dom::FontInfo::EnigmaStyleBold},
    {"italic", musx::dom::FontInfo::EnigmaStyleItalic},
    {"underline", musx::dom::FontInfo::EnigmaStyleUnderline},
    {"outline", enigmaStyleOutline},
    {"shadow", enigmaStyleShadow},
    {"strikeout", musx::dom::FontInfo::EnigmaStyleStrikeout},
    {"absolute", musx::dom::FontInfo::EnigmaStyleAbsolute},
    {"hidden", musx::dom::FontInfo::EnigmaStyleHidden},
};

constexpr std::uint16_t supportedEffectMask = musx::dom::FontInfo::EnigmaStyleBold | musx::dom::FontInfo::EnigmaStyleItalic
                                              | musx::dom::FontInfo::EnigmaStyleUnderline | musx::dom::FontInfo::EnigmaStyleStrikeout
                                              | musx::dom::FontInfo::EnigmaStyleAbsolute | musx::dom::FontInfo::EnigmaStyleHidden;

constexpr std::uint16_t normalizeEffectMask(std::uint16_t effects)
{
    return effects & supportedEffectMask;
}

constexpr bool hasDroppedEffectBits(std::uint16_t effects)
{
    return normalizeEffectMask(effects) != effects;
}

bool canEmbedEnigmaFontName(std::string_view name)
{
    int parenthesisDepth = 0;
    for (const char character : name) {
        if (character == ',') {
            return false;
        }
        if (character == '(') {
            ++parenthesisDepth;
        } else if (character == ')' && --parenthesisDepth < 0) {
            return false;
        }
    }
    return parenthesisDepth == 0;
}

std::string spellResolvedEnigmaFontCommand(std::string_view command, Cmper font, std::string_view name)
{
    if (!canEmbedEnigmaFontName(name)) {
        return "^fontid(" + std::to_string(font) + ')';
    }
    return '^' + std::string(command) + '(' + std::string(name) + ')';
}

// The binary command codes, and the modern command each one spells. Finale writes commands in
// this form from Finale 2006 on; earlier eras spell them out, and both forms are read.
//
// The codes fall into two groups. 0x81 to 0x88 are style commands, in no order visible here.
// From 0x8a up they are inserts, alphabetically, with `fdate` at 0x8d the one member out of
// place -- which it would not be if its internal name began with `date`. `perftime`, `cprsym`,
// `value`, `control` and `pass` follow `totpages`, and 0xa0 up holds the commands later
// releases added: appending is what a release does when it adds a command, since renumbering
// would change the meaning of every document already saved. Neither group grows in place, so
// a name absent from an earlier release never appears inside the alphabetical run.
//
// Six codes have no command: 0x80, 0x82, 0x83, 0x89, 0x93, 0x97. They are unexplained holes
// rather than reserved slots. **Unverified: 0x82 and 0x83 may be `^fontid` and `^fontNum`, or
// the two the other way round.** Nothing supports that beyond the shape of the list, which has
// misled twice; it is recorded so that a document carrying either code is recognized as
// bearing on it. None of the six is entered speculatively: an unlisted code is reported by
// number and its text dropped, which is a visible defect, where a guessed entry would write a
// wrong value into the document silently.
struct CommandCode
{
    std::uint8_t code;
    std::string_view command;
    /// @brief How many digit bytes the argument occupies. Zero means the command takes none.
    std::uint8_t digits;
    /// @brief Whether the argument is the comparator of the font that following text is
    /// written in, rather than a value of the command's own.
    bool selectsFont;
};

constexpr std::uint8_t flagArgument = 1;
constexpr std::uint8_t shortArgument = 4;
constexpr std::uint8_t longArgument = 8;

constexpr bool plainArgument = false;
constexpr bool selectsFont = true;

constexpr CommandCode commandCodes[] = {
    {0x81, "baseline", longArgument, plainArgument},
    {0x84, "nfx", shortArgument, plainArgument},
    // This column holds the command a font code becomes once its comparator is named.
    // `^fontid` appears nowhere in it: that spelling is the fallback for a comparator the
    // document does not define, not a command any code carries.
    {0x85, "font", shortArgument, selectsFont},
    {0x86, "size", shortArgument, plainArgument},
    {0x87, "superscript", longArgument, plainArgument},
    {0x88, "tracking", longArgument, plainArgument},
    {0x8a, "composer", 0, plainArgument},
    {0x8b, "copyright", 0, plainArgument},
    {0x8c, "date", shortArgument, plainArgument},
    {0x8d, "fdate", shortArgument, plainArgument},
    {0x8e, "dbflat", 0, plainArgument},
    {0x8f, "dbsharp", 0, plainArgument},
    {0x90, "description", 0, plainArgument},
    {0x91, "filename", 0, plainArgument},
    {0x92, "flat", 0, plainArgument},
    {0x94, "natural", 0, plainArgument},
    {0x95, "page", longArgument, plainArgument},
    {0x96, "sharp", 0, plainArgument},
    // **Believed.** The argument is the seconds flag, matching musxdom's own `^time`. This
    // command is carried forward even though Finale's own conversion discards it: what a
    // converter drops says what that conversion does, not what the document contains.
    {0x98, "time", flagArgument, plainArgument},
    {0x99, "title", 0, plainArgument},
    {0x9a, "totpages", 0, plainArgument},
    {0x9b, "perftime", shortArgument, plainArgument},
    {0x9c, "cprsym", 0, plainArgument},
    {0x9d, "value", 0, plainArgument},
    {0x9e, "control", 0, plainArgument},
    {0x9f, "pass", 0, plainArgument},
    // Appended by later releases. `^partname` needs linked parts, and the three File Info
    // inserts need the fuller File Info, so no document earlier than those features carries
    // them. The three are in the order of the File Info fields they read.
    {0xa0, "partname", 0, plainArgument},
    {0xa1, "lyricist", 0, plainArgument},
    {0xa2, "arranger", 0, plainArgument},
    {0xa3, "subtitle", 0, plainArgument},
    // Believed: Finale's legacy URL insert points at a text-block comparator and maps to
    // `^url(n)`. The relationship's validity is left to the text pool rather than inferred here.
    {0xa8, "url", shortArgument, plainArgument},
    // The font-category commands, appended alongside the inserts rather than grouped with the
    // other style commands, in the order Finale's marking-category dialog lists them. Each
    // needs marking categories to exist. The argument is a font comparator, as it is for 0x85;
    // each keeps its own spelling because the category it names is the one thing `^fontid`
    // cannot carry.
    {0xa4, "fontTxt", shortArgument, selectsFont},
    {0xa5, "fontMus", shortArgument, selectsFont},
    {0xa6, "fontNum", shortArgument, selectsFont},
    // Automatic rehearsal marks, and so later than everything above. It takes no argument: the
    // byte following the code is ordinary literal text, which is not a digit byte and could
    // not be an argument.
    {0xa7, "rehearsal", 0, plainArgument},
};

// Several commands are resolved by a parsing context rather than by the general Enigma parser.
// `value`, `control` and `pass` come from the context a `TextExpressionDef` supplies, all three
// being playback properties of the expression the text is attached to; `rehearsal` comes from
// the context a `MeasureExprAssign` supplies, since the mark it produces depends on where in
// the score the expression is assigned. `filename` is resolved by neither, and is for the
// client to resolve: only the client knows the name it saved the document under, or is about to
// save it under. All of them are written out regardless -- each is what the document says, and
// a command that cannot be resolved here is left for whoever can, where dropping it would
// delete content.

// A binary command's argument is a run of hexadecimal digits, one byte per digit, each digit
// stored one greater than its value. So `\x01\x01\x01\x02` is 0x0001 and `\x01\x01\x02\x09`
// is 0x0018. The digit range runs the full 0 to 15, making a nibble of 0xf the byte 0x10. No
// argument byte is ever 0x00, which is presumably the point of the offset: a zero byte would
// end a C string.
//
// The run is one value in base sixteen, not a sequence of shorter ones: an eight-digit
// `01 01 01 01 01 02 02 04` is 0x113, or 275.
//
// **An argument is a signed two's-complement value at whatever width it occupies:** 16 bits for
// four digits, 32 for eight. `10 10 10 10 10 10 10 04` is 0xfffffff3, or -13; read unsigned it
// would be 4294967283, so this is not a cosmetic distinction -- a negative baseline is ordinary
// in real documents.
//
// Four-digit arguments carry things with no negative meaning -- a font comparator, a point
// size, a style mask, a format ordinal -- and are read signed anyway, because the encoding is
// signed and the floor belongs to the dialog. Finale's page offset and tracking dialogs both
// run 0 to 32767 while writing an eight-digit argument, which has 32 bits to spend; a ceiling
// at the signed 16-bit maximum in a 32-bit slot means the value behind the dialog is a signed
// short widened on the way out. Baseline and superscript write that same width and are
// routinely negative.
//
// The one case where signed and unsigned would differ is a font comparator above 32767, which
// needs a document with 32768 font definitions. Such a comparator would be conspicuous the
// moment it appeared, so nothing is done about it in advance.
//
// **The width comes from the table, not from a scan.** Widths are one, four or eight digits,
// and which one a command uses is a property of the command and not of the value: `^nfx(0)`
// still spends four digits on a zero. Reading exactly that many bytes is what keeps a literal
// byte in the digit range from being consumed -- text set in a symbol font is glyph numbers, so
// a character 0x10 directly after a four-digit argument is perfectly possible, and scanning for
// the end of the run would take it as a fifth digit and lose both the glyph and the value.
//
// Where the bytes do not match the width, the command is reported unread rather than guessed
// at. That is a tripwire as much as a safeguard: it is what would announce a command whose
// argument is not the width recorded here.

/// @brief The byte a digit of zero is stored as. Digit 15 is `lastDigitByte`.
constexpr std::uint8_t firstDigitByte = 0x01;
constexpr std::uint8_t lastDigitByte = 0x10;

bool isDigitByte(std::uint8_t value)
{
    return value >= firstDigitByte && value <= lastDigitByte;
}

bool isCommandNameByte(std::uint8_t value)
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

/// @brief Returns the opening parenthesis after optional command whitespace.
std::optional<std::size_t> parenthesizedArgumentOpen(std::string_view text, std::size_t afterName)
{
    while (afterName < text.size() && isSpace(text[afterName])) {
        ++afterName;
    }
    return (afterName < text.size() && text[afterName] == '(') ? std::optional<std::size_t>(afterName) : std::nullopt;
}

/// @brief Returns one past a balanced parenthesized argument, or nothing if it is incomplete.
std::optional<std::size_t> parenthesizedArgumentEnd(std::string_view text, std::size_t open)
{
    if (open >= text.size() || text[open] != '(') {
        return std::nullopt;
    }
    std::size_t depth = 1;
    for (std::size_t at = open + 1; at < text.size(); ++at) {
        if (text[at] == '(') {
            ++depth;
        } else if (text[at] == ')' && --depth == 0) {
            return at + 1;
        }
    }
    return std::nullopt;
}

void rememberUnreadCode(std::vector<std::uint8_t>& list, std::uint8_t value)
{
    if (std::find(list.begin(), list.end(), value) == list.end()) {
        list.push_back(value);
    }
}

void rememberUnreadEffect(std::vector<std::string>& list, const std::string& value)
{
    if (std::find(list.begin(), list.end(), value) == list.end()) {
        list.push_back(value);
    }
}

// Converts one record. Held as a class because the conversion is a state machine over three
// pieces of state -- the font in force, an unflushed run of literal bytes, and an unflushed
// run of effect bits -- and every step needs all three.
class RecordConverter
{
public:
    RecordConverter(std::span<const std::uint8_t> body, const EnigmaTextSource& source)
        : m_body(body), m_source(source), m_font(source.initialFont ? std::optional<Cmper>(source.initialFont->fontId) : std::nullopt)
    {}

    ConvertedEnigmaText run()
    {
        while (m_at < m_body.size()) {
            if (readEffect()) {
                continue;
            }
            if (bridgesEffectRun()) {
                continue;
            }
            flushEffects();
            if (m_body[m_at] == '^') {
                FINALE_MUS_READER_TIMING_INCREMENT(timing::Counter::TextCommands, 1);
                readCommand();
            } else {
                m_literal.push_back(static_cast<char>(m_body[m_at++]));
            }
        }
        // Only one of these can have anything pending: an effect run flushes the literal
        // before it starts, and any literal byte flushes the effects before it accumulates.
        flushEffects();
        flushLiteral();
        m_result.text = normalizeLineBreaks(std::move(m_result.text));
        if (m_source.initialFont) {
            FINALE_MUS_READER_TIMED_SCOPE(timing::Phase::TextFontState);
            bool fontWasSynthesized = false;
            bool sizeWasSynthesized = false;
            bool effectsWereSynthesized = false;
            m_result.text = initializeEnigmaTextFontState(
                std::move(m_result.text), *m_source.initialFont, &fontWasSynthesized, &sizeWasSynthesized, &effectsWereSynthesized);
            m_result.fontWasSynthesized = fontWasSynthesized;
            m_result.sizeWasSynthesized = sizeWasSynthesized;
            m_result.effectsWereSynthesized = effectsWereSynthesized;
        }
        return std::move(m_result);
    }

private:
    /// @brief The bytes of the command name starting at @ref m_at, or empty if there is none.
    std::string_view commandNameAt(std::size_t start) const
    {
        std::size_t end = start;
        while (end < m_body.size() && isCommandNameByte(m_body[end])) {
            ++end;
        }
        return std::string_view(reinterpret_cast<const char*>(m_body.data() + start), end - start);
    }

    /// @brief Reads a parenthesized argument list, or nothing when the command has none.
    /// @details Parentheses may occur in an argument, notably in a font name, so the matching
    /// closing parenthesis ends it. A command whose arguments are unterminated is treated as
    /// having none, which leaves its bytes to be read as ordinary text rather than swallowing
    /// the rest of the record.
    ///
    /// Leading and trailing whitespace is trimmed from the argument before it is returned:
    /// Finale's own writer is loose about spacing around and between commands, and Finale
    /// itself still applies `^efx( bold)` despite the stray space, so a caller matching an
    /// argument against a fixed vocabulary, such as an effect name, must not fail on account
    /// of it. Interior
    /// spaces are left alone; a multi-word argument such as a font name is not affected.
    std::optional<std::string_view> argumentsAt(std::size_t start, std::size_t& end) const
    {
        end = start;
        const std::string_view body(reinterpret_cast<const char*>(m_body.data()), m_body.size());
        const auto argumentEnd = parenthesizedArgumentEnd(body, start);
        if (!argumentEnd) {
            return std::nullopt;
        }
        end = *argumentEnd;
        std::string_view argument = body.substr(start + 1, end - start - 2);
        while (!argument.empty() && isSpace(argument.front())) {
            argument.remove_prefix(1);
        }
        while (!argument.empty() && isSpace(argument.back())) {
            argument.remove_suffix(1);
        }
        return argument;
    }

    /// @brief Consumes the spaces between two `^efx` commands, keeping the run together.
    /// @details A run states one style, and the fixed-row eras write it either as adjacent
    /// commands or spaced apart. Both are one statement, so a space between two of them must
    /// not close the run and produce an `^nfx` for each half. The spaces are literal text and
    /// are held until the run ends, then written immediately before the single `^nfx`.
    bool bridgesEffectRun()
    {
        constexpr std::string_view effectCommand = "efx";
        if (!m_effects || m_body[m_at] != ' ') {
            return false;
        }
        std::size_t at = m_at;
        while (at < m_body.size() && m_body[at] == ' ') {
            ++at;
        }
        if (at >= m_body.size() || m_body[at] != '^' || commandNameAt(at + 1) != effectCommand) {
            return false;
        }
        m_effectGap.append(at - m_at, ' ');
        m_at = at;
        return true;
    }

    /// @brief Consumes one `^efx(name)` and folds it into the pending effect bits.
    bool readEffect()
    {
        if (m_at + 1 >= m_body.size() || m_body[m_at] != '^') {
            return false;
        }
        constexpr std::string_view effectCommand = "efx";
        if (commandNameAt(m_at + 1) != effectCommand) {
            return false;
        }
        const std::string_view body(reinterpret_cast<const char*>(m_body.data()), m_body.size());
        const auto open = parenthesizedArgumentOpen(body, m_at + 1 + effectCommand.size());
        if (!open) {
            return false;
        }
        std::size_t end = 0;
        const auto arguments = argumentsAt(*open, end);
        if (!arguments) {
            return false;
        }
        flushLiteral();
        m_at = end;
        if (!m_effects) {
            m_effects = Cmper(0);
        }
        // `plain` clears the accumulated bits rather than setting one, making the run a
        // complete statement of the style rather than a change to whatever came before.
        if (*arguments == "plain") {
            m_effects = Cmper(0);
            return true;
        }
        const auto found =
            std::find_if(std::begin(effectNames), std::end(effectNames), [&](const EffectName& entry) { return entry.name == *arguments; });
        if (found == std::end(effectNames)) {
            rememberUnreadEffect(m_result.unknownEffectNames, std::string(*arguments));
            return true;
        }
        m_effectsContainDroppedBits = m_effectsContainDroppedBits || hasDroppedEffectBits(found->bit);
        *m_effects |= found->bit;
        return true;
    }

    void readCommand()
    {
        // An escaped caret is content, not a command, and stays escaped: musxdom's parser
        // reads `^^` back as one caret.
        if (m_at + 1 < m_body.size() && m_body[m_at + 1] == '^') {
            m_literal.append("^^");
            m_at += 2;
            return;
        }
        const auto name = commandNameAt(m_at + 1);
        if (!name.empty()) {
            const std::string_view body(reinterpret_cast<const char*>(m_body.data()), m_body.size());
            const auto afterName = m_at + 1 + name.size();
            const auto open = parenthesizedArgumentOpen(body, afterName);
            std::size_t end = 0;
            const auto arguments = open ? argumentsAt(*open, end) : std::nullopt;
            flushLiteral();
            m_at = arguments ? end : afterName;
            emitTextCommand(name, arguments.value_or(std::string_view{}), arguments.has_value());
            return;
        }
        if (readBinaryCommand()) {
            return;
        }
        // A lone caret that starts nothing recognizable is left as content. musxdom does the
        // same with an unparseable caret, so the two agree about what the document says.
        m_literal.push_back('^');
        ++m_at;
    }

    /// @brief Reads the command code that follows the caret, in whichever spelling applies.
    /// @details A pre-Unicode record stores the code as one byte. From Finale 2012 the record
    /// is UTF-8, so the same value arrives as its two-byte encoding.
    ///
    /// Which spelling to expect is decided by the record, not by the bytes, and deliberately.
    /// A structural test would be ambiguous in exactly the case that matters: in a pre-Unicode
    /// record a command code of 0xc2 followed by literal text beginning with a glyph number in
    /// 0x80-0xbf reads as a valid two-byte sequence and would swallow the character. The
    /// record already knows its own encoding, so it is asked.
    std::optional<std::uint8_t> commandCodeAt(std::size_t start, std::size_t& end) const
    {
        if (start >= m_body.size()) {
            return std::nullopt;
        }
        const auto first = m_body[start];
        if (m_source.utf8) {
            // Only U+0080 to U+00FF can be a command code, so only these two lead bytes can
            // introduce one.
            if ((first != 0xc2 && first != 0xc3) || start + 1 >= m_body.size() || (m_body[start + 1] & 0xc0U) != 0x80U) {
                return std::nullopt;
            }
            end = start + 2;
            return static_cast<std::uint8_t>(((first & 0x1fU) << 6U) | (m_body[start + 1] & 0x3fU));
        }
        if (first < 0x80) {
            return std::nullopt;
        }
        end = start + 1;
        return first;
    }

    /// @brief Reads exactly @p digits argument bytes, or nothing when they are not all digits.
    std::optional<std::int64_t> argumentAt(std::size_t start, std::uint8_t digits) const
    {
        if (start + digits > m_body.size()) {
            return std::nullopt;
        }
        std::uint32_t value = 0;
        for (std::uint8_t i = 0; i < digits; ++i) {
            const auto byte = m_body[start + i];
            if (!isDigitByte(byte)) {
                return std::nullopt;
            }
            value = value * 16U + (byte - firstDigitByte);
        }
        // Signed at whatever width the argument occupies, which is four bits per digit.
        const auto bits = static_cast<unsigned>(digits) * 4U;
        const auto signBit = std::uint32_t{1} << (bits - 1U);
        const auto magnitude = static_cast<std::int64_t>(value & (signBit - 1U));
        return (value & signBit) ? magnitude - static_cast<std::int64_t>(signBit) : magnitude;
    }

    /// @brief Skips the argument of a command whose width is not known.
    /// @details Only reached for a code with no entry in the table, whose text is dropped
    /// anyway, so this is choosing between two ways of being wrong. Consuming every digit byte
    /// is the better one: leaving them would put control characters into the document's text.
    std::size_t skipUnknownArgument(std::size_t start) const
    {
        std::size_t at = start;
        while (at < m_body.size() && at - start < longArgument && isDigitByte(m_body[at])) {
            ++at;
        }
        return at;
    }

    bool readBinaryCommand()
    {
        std::size_t afterCode = 0;
        const auto code = commandCodeAt(m_at + 1, afterCode);
        if (!code) {
            return false;
        }
        const auto found =
            std::find_if(std::begin(commandCodes), std::end(commandCodes), [&](const CommandCode& entry) { return entry.code == *code; });
        const auto argument = found != std::end(commandCodes) ? argumentAt(afterCode, found->digits) : std::nullopt;
        if (found == std::end(commandCodes) || (found->digits > 0 && !argument)) {
            // Either no spelling is known for this code, or its argument is not the width
            // recorded for it. Either way the command cannot be stated, and
            // inventing one would put a wrong value into the document.
            rememberUnreadCode(m_result.unreadCommandCodes, *code);
            m_at = skipUnknownArgument(afterCode);
            return true;
        }
        m_at = afterCode + found->digits;

        flushLiteral();
        if (found->selectsFont) {
            emitResolvedFont(found->command, static_cast<Cmper>(argument.value_or(0)));
            return true;
        }
        if (found->command == "nfx") {
            emitNfx(static_cast<std::uint16_t>(argument.value_or(0)));
            return true;
        }
        m_result.text.push_back('^');
        m_result.text.append(found->command);
        m_result.text.push_back('(');
        if (found->digits > 0) {
            m_result.text.append(std::to_string(*argument));
        }
        m_result.text.push_back(')');
        return true;
    }

    /// @brief Passes a spelled-out command through, resolving a font reference on the way.
    void emitTextCommand(std::string_view name, std::string_view arguments, bool hasArguments)
    {
        if (name == "font" || name == "Font" || name == "fontid" || name == "fontMus" || name == "fontTxt" || name == "fontNum") {
            emitFontCommand(name, arguments);
            return;
        }
        if (name == "nfx" && hasArguments) {
            if (const auto effects = readDecimal(arguments)) {
                emitNfx(static_cast<std::uint16_t>(*effects));
                return;
            }
        }
        m_result.text.push_back('^');
        m_result.text.append(name);
        if (hasArguments) {
            m_result.text.push_back('(');
            m_result.text.append(convertCommandText(arguments));
            m_result.text.push_back(')');
        }
    }

    void emitNfx(std::uint16_t effects) { m_result.text.append("^nfx(" + std::to_string(normalizeEffectMask(effects)) + ")"); }

    void emitFontCommand(std::string_view name, std::string_view arguments)
    {
        // The fixed-row eras name the font alone; the compressed eras add its character set
        // as a second argument, packed exactly as the `FN` record's own header word. It
        // governs the stored bytes of the name itself, then is omitted from the rewritten
        // command because the resolved `FontDefinition` states the same thing.
        const auto comma = arguments.find(',');
        const auto spelled = arguments.substr(0, comma);
        // An empty ordinary font command names no font and has no effect on the active state.
        if ((name == "font" || name == "Font") && spelled.empty()) {
            return;
        }
        std::optional<std::uint16_t> packedCharset;
        if (comma != std::string_view::npos) {
            if (const auto packed = readDecimal(arguments.substr(comma + 1))) {
                packedCharset = static_cast<std::uint16_t>(*packed);
            }
        }
        // Whatever the source spells, the command names one font definition. `^fontid` states
        // its comparator outright, `Font` followed by digits is the same thing under Finale's
        // own convention for a font it knows only by id, and anything else is a name matched
        // to a definition after normalization.
        const auto resolved = name == "fontid" ? readDecimal(spelled) : resolveFont(spelled, packedCharset);
        if (!resolved) {
            // Nothing in the document answers to this normalized name, so the spelling is
            // all there is to keep. There is no comparator to fall back to either.
            m_font.reset();
            m_result.text.push_back('^');
            m_result.text.append(name);
            m_result.text.push_back('(');
            m_result.text.append(convertCommandText(spelled, packedCharset));
            m_result.text.push_back(')');
            return;
        }
        // `^font`, `^Font` and `^fontid` all say the same thing, so they converge on one
        // spelling; the three categorized commands say something more and keep theirs.
        const auto isCategorized = name == "fontMus" || name == "fontTxt" || name == "fontNum";
        emitResolvedFont(isCategorized ? name : std::string_view("font"), *resolved);
    }

    /// @brief Writes a font command whose comparator is known, naming the font where it can.
    /// @details A name is what musxdom's parser prefers, and it survives a document whose font
    /// definitions are renumbered where a bare comparator does not. `^fontid` is the fallback
    /// rather than the normal form: it is the one spelling that needs no definition to exist or
    /// the name to fit Enigma's parenthesized, comma-delimited argument syntax. That fallback
    /// also drops the marking category a categorized command names, which is the one thing
    /// `^fontid` cannot carry; losing it is the lesser harm against emitting an invalid command.
    ///
    /// Font definitions must therefore be imported before any text.
    void emitResolvedFont(std::string_view command, Cmper font)
    {
        m_font = font;
        const auto named = fontNameFor(font);
        if (!named) {
            m_result.text.append("^fontid(" + std::to_string(font) + ")");
            return;
        }
        m_result.text.append(spellResolvedEnigmaFontCommand(command, font, *named));
    }

    /// @brief The name the document gives a comparator, or nothing when it names none.
    std::optional<std::string> fontNameFor(Cmper font) const
    {
        const auto definition = m_source.document->getOthers()->get<FontDefinitionSource>(musx::dom::SCORE_PARTID, font);
        if (!definition || definition->name.empty()) {
            return std::nullopt;
        }
        return definition->name;
    }

    /// @brief The comparator a run of decimal digits states, or nothing when it is not one.
    /// @details A comparator is sixteen bits, so anything longer than five digits is not one
    /// however it is spelled. Rejecting it here rather than converting is what keeps a
    /// malformed record from throwing out of the middle of an import.
    static std::optional<Cmper> readDecimal(std::string_view digits)
    {
        constexpr std::size_t maximumComparatorDigits = 5;
        if (digits.empty() || digits.size() > maximumComparatorDigits
            || !std::all_of(digits.begin(), digits.end(), [](char value) { return value >= '0' && value <= '9'; })) {
            return std::nullopt;
        }
        const auto value = std::stoul(std::string(digits));
        if (value > (std::numeric_limits<Cmper>::max)()) {
            return std::nullopt;
        }
        return static_cast<Cmper>(value);
    }

    /// @brief The id a `FontN` spelling states, or nothing when the text is a real name.
    /// @details Finale writes a font it knows only by id as `Font` followed by that id.
    /// musxdom reads the same spelling, so this is not an invention of either side.
    static std::optional<Cmper> fontIdFromSpelling(std::string_view spelled)
    {
        constexpr std::string_view idPrefix = "Font";
        if (spelled.size() <= idPrefix.size() || spelled.substr(0, idPrefix.size()) != idPrefix) {
            return std::nullopt;
        }
        return readDecimal(spelled.substr(idPrefix.size()));
    }

    std::optional<Cmper> resolveFont(std::string_view spelled, std::optional<std::uint16_t> packedCharset = std::nullopt) const
    {
        if (const auto byId = fontIdFromSpelling(spelled)) {
            return byId;
        }
        const auto name = musx::dom::normalizeFontName(convertCommandText(spelled, packedCharset));
        if (m_source.fontResolutionCache) {
            auto& resolved = m_source.fontResolutionCache->fontIdsByName;
            if (const auto cached = resolved.find(name); cached != resolved.end()) {
                FINALE_MUS_READER_TIMING_INCREMENT(timing::Counter::TextFontResolutionCacheHits, 1);
                return cached->second;
            }
            FINALE_MUS_READER_TIMING_INCREMENT(timing::Counter::TextFontResolutionCacheMisses, 1);
            const auto font = resolveFontName(name);
            resolved.emplace(name, font);
            return font;
        }
        return resolveFontName(name);
    }

    std::optional<Cmper> resolveFontName(const std::string& normalizedName) const
    {
        // A named font command and its definition may differ in case or spacing. Compare
        // their normalized names while retaining the definition's stored comparator.
        for (const auto& font : m_source.document->getOthers()->getArray<musx::dom::others::FontDefinition>(musx::dom::SCORE_PARTID)) {
            if (musx::dom::normalizeFontName(font->name) == normalizedName) {
                return font->getCmper();
            }
        }
        return std::nullopt;
    }

    /// @brief Converts text that belongs to a command, such as a font name.
    std::string convertCommandText(std::string_view raw, std::optional<std::uint16_t> packedCharset = std::nullopt) const
    {
        if (m_source.utf8) {
            return std::string(raw);
        }
        return packedCharset ? toUtf8(raw, *packedCharset) : toUtf8(raw, m_source.platform);
    }

    void flushLiteral()
    {
        if (m_literal.empty()) {
            return;
        }
        FINALE_MUS_READER_TIMING_INCREMENT(timing::Counter::TextLiteralRuns, 1);
        FINALE_MUS_READER_TIMING_INCREMENT(timing::Counter::TextLiteralBytes, m_literal.size());
        FINALE_MUS_READER_TIMED_SCOPE(timing::Phase::TextLiteralEncoding);
        if (m_source.utf8) {
            m_result.text.append(m_literal);
        } else if (m_font) {
            m_result.text.append(toUtf8(m_literal, m_source.document, *m_font, UnresolvedFontFallback::Text));
        } else {
            m_result.text.append(toUtf8(m_literal, m_source.platform));
        }
        m_literal.clear();
    }

    void flushEffects()
    {
        if (!m_effects) {
            return;
        }
        const auto effects = normalizeEffectMask(*m_effects);
        if (m_effectsContainDroppedBits && effects == 0) {
            m_effectGap.clear();
            m_effects.reset();
            m_effectsContainDroppedBits = false;
            return;
        }
        m_result.text.append(m_effectGap);
        m_effectGap.clear();
        m_result.text.append("^nfx(" + std::to_string(effects) + ")");
        m_effects.reset();
        m_effectsContainDroppedBits = false;
    }

    std::span<const std::uint8_t> m_body;
    const EnigmaTextSource& m_source;
    ConvertedEnigmaText m_result;
    std::string m_literal;
    std::optional<Cmper> m_font;
    std::optional<std::uint16_t> m_effects;
    bool m_effectsContainDroppedBits{};
    /// @brief Spaces consumed between two `^efx` commands of one run.
    std::string m_effectGap;
    std::size_t m_at{};
};

} // namespace

std::string initializeEnigmaTextFontState(
    std::string value, const musx::dom::FontInfo& defaultFont, bool* fontWasSynthesized, bool* sizeWasSynthesized, bool* effectsWereSynthesized)
{
    // A literal byte, including whitespace, ends the initial command run. Missing settings
    // are inserted before it so the first content is interpreted under one complete state;
    // explicit settings remain in their original order.
    bool hasFont = false;
    bool hasSize = false;
    bool hasEffects = false;
    std::size_t at = 0;
    while (at < value.size() && value[at] == '^') {
        const auto nameStart = at + 1;
        auto nameEnd = nameStart;
        while (nameEnd < value.size() && isCommandNameByte(static_cast<std::uint8_t>(value[nameEnd]))) {
            ++nameEnd;
        }
        if (nameEnd == nameStart || nameEnd >= value.size()) {
            break;
        }
        const auto open = parenthesizedArgumentOpen(value, nameEnd);
        const auto commandEnd = open ? parenthesizedArgumentEnd(value, *open) : std::nullopt;
        if (!commandEnd) {
            break;
        }
        const std::string_view name(value.data() + nameStart, nameEnd - nameStart);
        hasFont = hasFont || name == "font" || name == "Font" || name == "fontid" || name == "fontMus" || name == "fontTxt" || name == "fontNum";
        hasSize = hasSize || name == "size";
        hasEffects = hasEffects || name == "nfx";
        at = *commandEnd;
    }

    if (hasFont && hasSize && hasEffects) {
        return value;
    }

    std::string completedInitial;
    if (!hasFont) {
        if (fontWasSynthesized) {
            *fontWasSynthesized = true;
        }
        // A name survives document-local comparator renumbering. A name that cannot fit the
        // command argument syntax retains its already-resolved comparator instead.
        completedInitial = spellResolvedEnigmaFontCommand("font", defaultFont.fontId, defaultFont.getName());
    }
    completedInitial.append(value, 0, at);
    if (!hasSize) {
        if (sizeWasSynthesized) {
            *sizeWasSynthesized = true;
        }
        completedInitial += "^size(" + std::to_string(defaultFont.fontSize) + ')';
    }
    if (!hasEffects) {
        if (effectsWereSynthesized) {
            *effectsWereSynthesized = true;
        }
        completedInitial += "^nfx(" + std::to_string(defaultFont.getEnigmaStyles()) + ')';
    }
    value.replace(0, at, completedInitial);
    return value;
}

ConvertedEnigmaText toModernEnigmaText(std::span<const std::uint8_t> body, const EnigmaTextSource& source)
{
    return RecordConverter(body, source).run();
}

} // namespace text
} // namespace finale_mus_reader
