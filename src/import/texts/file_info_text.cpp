// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/texts.h"
#include "import/texts/internal.h"

#include <algorithm>
#include <memory>
#include <span>
#include <string>

#include "import/support/enigma_text.h"
#include "import/support/text_encoding.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace texts {
namespace {

using FileInfoTarget = musx::dom::texts::FileInfoText;

// File Info is document content that lives in the header rather than in any pool: four
// NUL-terminated strings at fixed offsets inside the first 0x200 bytes. musxdom models them
// as ordinary texts-pool objects keyed by @ref FileInfoText::TextType, so recovering them is
// a matter of reading four offsets and converting four strings.
//
// Only for a document that keeps them there. By Finale 2008 File Info has moved into the text
// pool as `^fileInfo(n)` chunks, which `importTextPool` reads, and the header offsets are
// empty. Which one a document uses is read from the document rather than from its version:
// this pass fills in only the types the pool did not supply, so a file that has moved on is
// left alone and a file that has not is still recovered. Where in Finale 2007 or 2008 the move
// happened is open, and does not need answering to read either kind.
//
// Each offset holds what was typed into that field of the File Info dialog. The offsets are
// not a uniform grid -- the composer slot is 0x20 long where the others are 0x40 -- so they are
// listed individually rather than derived from a stride.
//
// **Four fields is the whole of it in the eras that store File Info here, not a gap in this
// table.** musxdom's `FileInfoText::TextType` also names lyricist, arranger and subtitle, but
// Finale's File Info dialog offers only these four until a later release, by which point the
// fields have moved into the text pool. A document that keeps them in the header cannot carry
// the other three, so reading a fifth offset would be reading whatever else the header keeps
// there.
struct FileInfoField
{
    std::size_t offset;
    FileInfoTarget::TextType type;
};

constexpr FileInfoField fileInfoFields[] = {
    {0x0d8, FileInfoTarget::TextType::Title},
    {0x118, FileInfoTarget::TextType::Composer},
    {0x138, FileInfoTarget::TextType::Copyright},
    {0x178, FileInfoTarget::TextType::Description},
};

// Where the description ends when it is longer than its slot.
//
// A long description runs past 0x200 and into what is otherwise the record body, and the
// header states the body's real start at offset 0x60 as a 16-bit value in the file's own byte
// order. Reading that gives the string a bound the file itself supplies. It is a guard rather
// than a terminator: a description still ends at a NUL before it.
constexpr std::size_t bodyOffsetLocation = 0x60;
constexpr std::size_t fileInfoHeaderSize = 0x200;

std::size_t readBodyOffset(std::span<const std::uint8_t> source, ByteOrder byteOrder)
{
    if (source.size() < bodyOffsetLocation + 2) {
        return source.size();
    }
    const auto high = source[bodyOffsetLocation + (byteOrder == ByteOrder::BigEndian ? 0 : 1)];
    const auto low = source[bodyOffsetLocation + (byteOrder == ByteOrder::BigEndian ? 1 : 0)];
    const auto stated = static_cast<std::size_t>((high << 8U) | low);
    // A body offset below the customary header size, or past the end of the file, is not
    // describing this header; fall back to the file itself, which bounds the read anyway.
    return stated >= fileInfoHeaderSize && stated <= source.size() ? stated : source.size();
}

std::string readString(std::span<const std::uint8_t> source, std::size_t offset, std::size_t limit)
{
    if (offset >= limit) {
        return {};
    }
    const auto begin = source.begin() + static_cast<std::ptrdiff_t>(offset);
    const auto end = source.begin() + static_cast<std::ptrdiff_t>(limit);
    const auto terminator = std::find(begin, end, std::uint8_t{0});
    return std::string(reinterpret_cast<const char*>(source.data() + offset), static_cast<std::size_t>(terminator - begin));
}

} // namespace

void importHeaderFileInfoTexts(const ImportContext& context)
{
    // The Coda-banner epoch is excluded on purpose, and the exclusion is structural rather
    // than a version test: that era has no 0x200 header of this shape at all -- it opens with
    // a plain-text product banner and reserves the region for something else -- so there is
    // nothing at these offsets to read. Its File Info, if it keeps any, is unlocated.
    if (context.profile.epoch == FormatEpoch::CodaBanner) {
        return;
    }
    if (context.source.size() < fileInfoHeaderSize) {
        return;
    }
    const auto limit = readBodyOffset(context.source, context.profile.byteOrder);
    const auto defaultFont =
        musx::dom::options::FontOptions::getFontInfoOrNull(context.document, musx::dom::options::FontOptions::FontType::TextBlock);

    for (const auto& field : fileInfoFields) {
        if (context.document->getTexts()->get<FileInfoTarget>(static_cast<musx::dom::Cmper>(field.type))) {
            // The text pool already stated this one, and it is the more direct statement:
            // the record names its own type where the header only implies it by position.
            continue;
        }
        const auto raw = readString(context.source, field.offset, limit);
        if (raw.empty()) {
            // An empty slot means the field was never filled in, so creating an object for it
            // would invent content rather than recover it.
            continue;
        }
        // The stored string is already a raw Enigma string, and musxdom documents the inserts
        // in this class as meaningless, so nothing is translated: only the bytes change.
        auto instance = std::make_shared<FileInfoTarget>(context.document, static_cast<musx::dom::Cmper>(field.type));
        instance->text = text::normalizeLineBreaks(text::toUtf8(raw, context.profile.platform));
        bool fontWasSynthesized = false;
        bool sizeWasSynthesized = false;
        bool effectsWereSynthesized = false;
        if (defaultFont) {
            instance->text = text::initializeEnigmaTextFontState(
                std::move(instance->text), *defaultFont, &fontWasSynthesized, &sizeWasSynthesized, &effectsWereSynthesized);
        }
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto key = reporting.template instanceKey<FileInfoTarget>(musx::dom::SCORE_PARTID, static_cast<musx::dom::Cmper>(field.type));
            reporting.textField(key, "text", fontWasSynthesized, sizeWasSynthesized, effectsWereSynthesized);
            reporting.report().setField(
                key, "text", {Reporting::Origin::LegacyMus, 0, field.offset, static_cast<std::int64_t>(instance->text.size())});
        });
        context.document->getTexts()->add(FileInfoTarget::XmlNodeName, std::move(instance));
    }
}

} // namespace texts
} // namespace finale_mus_reader
