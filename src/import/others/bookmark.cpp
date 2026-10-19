// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

// Bookmarks. Only the name is recovered: musxdom has no class for the bookmark object, so the
// view state the record carries alongside the name has nowhere to go. It is decoded in comments
// here rather than in code so that adding the object later is a matter of reading words this
// file already describes.
//
// The record is 36 words in every era that carries its name, whether as the `BK` tag of the
// fixed-row pools or as class 0x007b of the zlib pool:
//
//   words 0..23   the name, 48 bytes ending at the first NUL
//   word 24       1 scroll view, 0 page view
//   word 25       view percent, stored as written even when word 26 is clear
//   word 26       whether the view percent applies -- EnigmaXML `changePercent`
//   words 27..29  unused
//   word 30       the measure number in scroll view, the page number in page view
//   word 31       the staff comparator in scroll view, the horizontal page position in page view
//   word 32       the staff-set comparator in scroll view, the vertical page position in page view
//   word 33       flags: 0x0001 word 31 applies, 0x0002 word 32 applies
//   words 34..35  unused
//
// A page-view bookmark with only a horizontal position stores flags 1, and one with only a
// vertical position stores flags 2. In scroll view the same first bit is EnigmaXML `changeInst`.
//
// From Finale 2012 the record keeps the same words from 24 on but drops the name, so it is 24
// bytes rather than 72, and word 27 -- the first that was unused -- becomes the text-pool
// comparator of the name. **Which form a record uses is read from its own length**, never from a
// version: a document that states its name here has it here, and one that does not states it in
// the text pool, where `importTextPool` reads it.

#include "import/others.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "import/support/legacy_mapping.h"
#include "import/support/text_encoding.h"
#include "records/legacy_record_index.h"

#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using BookmarkTarget = musx::dom::texts::BookmarkText;

constexpr records::LegacyTag bookmarkRecord = records::packTag("BK");
constexpr records::LegacyTag bookmarkClass = 0x007b;

// The name occupies the first four incidences of the tagged form, so 48 bytes, however short it
// is. The two incidences that follow carry the view state and must not be read as characters.
constexpr std::uint32_t bookmarkNameIncidences = 4;
constexpr std::size_t bookmarkNameBytes = 48;

// A class record that still carries its name is the whole 36 words; one that does not is the 12
// words from the view type on. Anything between the two is neither form, and reading a name out
// of it would be reading whatever else the record holds.
constexpr std::size_t bookmarkNamedRecordBytes = 72;

/// @brief One bookmark's name as the record states it, before any of it is converted.
struct StoredName
{
    std::string raw;
    std::size_t blockOffset{};
    std::size_t decodedOffset{};
    records::LegacyTag identity{};
};

/// @brief The names of every bookmark the document states one for, in record order.
/// @details The two stores are mutually exclusive in every document: a file with a tagged others
/// pool has no class records and the reverse, so this reads whichever one the file has rather
/// than choosing between them.
std::vector<StoredName> readStoredNames(const ImportContext& context)
{
    std::vector<StoredName> result;

    const auto& tagged = context.index.getOthers();
    for (const auto cmper : tagged.cmpersForTag(bookmarkRecord)) {
        const auto family = tagged.getArray(bookmarkRecord, cmper);
        if (family.empty()) {
            continue;
        }
        result.push_back(
            {readRowText(tagged, family, 0, bookmarkNameIncidences), family.front().blockOffset, family.front().decodedOffset, bookmarkRecord});
    }

    const auto& classed = context.index.getClassOthers();
    for (const auto cmper : classed.cmpersForTag(bookmarkClass)) {
        const auto* row = classed.get(bookmarkClass, cmper, 0, 0);
        if (!row) {
            continue;
        }
        const auto payload = classed.effectivePayloadOf(*row);
        if (payload.size() < bookmarkNamedRecordBytes) {
            // The short form, whose name is a text-pool record the pool importer has read.
            continue;
        }
        const auto name = payload.first(bookmarkNameBytes);
        const auto terminator = std::find(name.begin(), name.end(), std::uint8_t{0});
        result.push_back({std::string(reinterpret_cast<const char*>(name.data()), static_cast<std::size_t>(terminator - name.begin())),
            row->blockOffset, row->decodedOffset, bookmarkClass});
    }

    return result;
}

void synthesizeBookmarkTexts(const ImportContext& context)
{
    for (const auto& stored : readStoredNames(context)) {
        if (stored.raw.empty()) {
            // A bookmark with no name has no text to state, and an empty record would claim the
            // source said something it did not. The comparators the remaining bookmarks take are
            // unaffected, since each is allocated as its record is read.
            continue;
        }
        const auto number = context.document->getTexts()->nextFreeCmper<BookmarkTarget>();
        if (!number) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Warning, "Bookmark text synthesis exhausted the text identifier space."});
            return;
        }
        // The name carries no font, so the record is stored as its re-encoded characters and
        // nothing else. The bytes go through the source platform's encoding because there is no
        // font to name a code page, and no formatting state is prepended: Finale 27 writes a
        // bookmark's text the same way, and so does the text pool of the era that stores one
        // there, so a `^font`/`^size`/`^nfx` prefix would be this reader's invention.
        auto instance = std::make_shared<BookmarkTarget>(context.document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, *number);
        instance->text = text::normalizeLineBreaks(text::toUtf8(stored.raw, context.profile.platform));
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto key = reporting.template instanceKey<BookmarkTarget>(musx::dom::SCORE_PARTID, *number);
            reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
            reporting.textField(key, "text", false, false, false);
            reporting.report().setField(key, "text",
                {Reporting::Origin::LegacyMus, stored.blockOffset, stored.decodedOffset, static_cast<std::int64_t>(instance->text.size()),
                    stored.identity});
        });
        context.document->getTexts()->add(BookmarkTarget::XmlNodeName, std::move(instance));
    }
}

} // namespace

void importBookmarks(const ImportContext& context)
{
    // The comparator a synthesized name takes is the next free one in the text pool, so the work
    // waits until every pool is filled rather than racing the text-pool importer for the low
    // numbers. A document whose bookmarks name text-pool records has no name in its own records
    // to synthesize from, so the two never collide -- but the ordering is a property of this
    // class rather than of the registry, which states no order.
    context.pending.defer(DeferredStage::Synthesize, [&context] { synthesizeBookmarkTexts(context); });
}

} // namespace others
} // namespace finale_mus_reader
