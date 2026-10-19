// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "import/support/legacy_percussion.h"
#include "import/support/percussion_records.h"
#include "import/support/text_encoding.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using PercussionNoteInfoTarget = musx::dom::others::PercussionNoteInfo;
using PercussionFontType = musx::dom::options::FontOptions::FontType;

constexpr std::size_t percussionNoteInfoNarrowStride = 12;
constexpr std::size_t percussionNoteInfoWideDataSize = 20;
constexpr std::size_t percussionNoteInfoWideStride = 24;

template <typename Reporting>
void reportPercussionNoteInfo(Reporting& reporting, const PercussionNoteInfoTarget& target, const RecordFamilySource& source,
    const records::LegacyRow& row, std::size_t elementOffset, bool wide)
{
    const auto key = reporting.template instanceKey<PercussionNoteInfoTarget>(target.getSourcePartId(), target.getCmper(), target.getInci());
    reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
    const auto report = [&](const char* member, std::size_t fieldOffset, auto value) {
        reporting.report().setField(
            key, member, {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + elementOffset + fieldOffset, value, source.identity});
    };
    report("percNoteType", 0, target.percNoteType);
    report("staffPosition", 2, target.staffPosition);
    report("closedNotehead", 4, static_cast<std::uint32_t>(target.closedNotehead));
    report("halfNotehead", wide ? 8 : 6, static_cast<std::uint32_t>(target.halfNotehead));
    report("wholeNotehead", wide ? 12 : 8, static_cast<std::uint32_t>(target.wholeNotehead));
    report("dwholeNotehead", wide ? 16 : 10, static_cast<std::uint32_t>(target.dwholeNotehead));
}

template <typename Reporting>
void reportLegacyPercussionNoteInfo(Reporting& reporting, const PercussionNoteInfoTarget& target, const percussion::LegacyLayoutNote& note)
{
    using Origin = Reporting::Origin;
    const auto key = reporting.template instanceKey<PercussionNoteInfoTarget>(target.getSourcePartId(), target.getCmper(), target.getInci());
    reporting.report().setInstanceOrigin(key, Origin::LegacyBehavior);
    const auto report = [&](const char* member, Origin origin, std::size_t fieldOffset, auto value) {
        if (note.row) {
            reporting.report().setField(key, member, {origin, note.row->blockOffset, note.row->decodedOffset + fieldOffset, value, note.row->tag});
        } else {
            reportFallbackField(reporting, key, member, Origin::LegacyBehavior, value);
        }
    };
    report("percNoteType", Origin::LegacyBehavior, 0, note.playbackKey);
    report("staffPosition", Origin::LegacyMus, 2, target.staffPosition);
    report("closedNotehead", Origin::LegacyMus, 4, static_cast<std::uint32_t>(target.closedNotehead));
    report("halfNotehead", Origin::LegacyBehavior, 6, static_cast<std::uint32_t>(target.halfNotehead));
    report("wholeNotehead", Origin::LegacyBehavior, 6, static_cast<std::uint32_t>(target.wholeNotehead));
    report("dwholeNotehead", Origin::LegacyBehavior, 6, static_cast<std::uint32_t>(target.dwholeNotehead));
}

char32_t percussionNotehead(std::uint16_t stored, const musx::dom::DocumentPtr& document, musx::dom::Cmper fontId)
{
    return text::codepointFromByte(static_cast<std::uint8_t>(stored), document, fontId, text::UnresolvedFontFallback::Symbol);
}

void importLegacyPercussionNoteInfo(const ImportContext& context)
{
    context.pending.defer(DeferredStage::NormalizeStoredValues, [&context] {
        const auto rows = selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(),
            percussion_records::layoutRowTag, percussion_records::layoutRowClass, true);
        const auto percussionFont = musx::dom::options::FontOptions::getFontInfoOrNull(context.document, PercussionFontType::Percussion);
        const auto fontId = percussionFont ? percussionFont->fontId : musx::dom::DEFAULT_MUSIC_FONT_ID;
        for (const auto& [layoutId, notes] : percussion::legacyConversion(context).layouts) {
            for (std::size_t index = 0; index < notes.size(); ++index) {
                const auto& note = notes[index];
                const auto inci = static_cast<musx::dom::Inci>(index);
                auto target = note.row && rows
                                  ? createOthersRecordTarget<PercussionNoteInfoTarget>(context.document, *rows, *note.row, layoutId, inci)
                                  : std::make_shared<PercussionNoteInfoTarget>(
                                        context.document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, layoutId, inci);
                if (!target) {
                    continue;
                }
                target->percNoteType = note.type;
                target->staffPosition = note.staffPosition;
                target->closedNotehead = percussionNotehead(note.closedNotehead, context.document, fontId);
                // The legacy row has one open notehead, used for every unfilled value.
                target->halfNotehead = target->wholeNotehead = target->dwholeNotehead =
                    percussionNotehead(note.openNotehead, context.document, fontId);
                withReporting(
                    context.report, [&]<typename Reporting>(Reporting& reporting) { reportLegacyPercussionNoteInfo(reporting, *target, note); });
                context.document->getOthers()->add(PercussionNoteInfoTarget::XmlNodeName, std::move(target));
            }
        }
    });
}

} // namespace

void importPercussionNoteInfo(const ImportContext& context)
{
    // Percussion maps begin in Finale 3.5. Before Finale 2010 they are stored as selected rows.
    if (percussion::storesLegacyLayouts(context)) {
        importLegacyPercussionNoteInfo(context);
        return;
    }
    if (context.profile.epoch != FormatEpoch::ZlibLegacy) {
        return;
    }
    const RecordFamilySource source{&context.index.getClassOthers(), percussion_records::noteCollectionClass, true};

    const bool wide = versions::storesUnicodeCodepoints(context.profile.version);
    const auto stride = wide ? percussionNoteInfoWideStride : percussionNoteInfoNarrowStride;
    const auto percussionFont = musx::dom::options::FontOptions::getFontInfoOrNull(context.document, PercussionFontType::Percussion);
    const auto fontId = percussionFont ? percussionFont->fontId : musx::dom::DEFAULT_MUSIC_FONT_ID;

    for (const auto& [partId, cmper] : recordKeys(source)) {
        const auto rows = source.pool->getArray(source.identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto payload = collectRecordPayload(source, rows);
        if (payload.size() % stride != 0) {
            context.report.diagnostics.push_back(
                {musx::util::Logger::LogLevel::Info, "Percussion note map " + std::to_string(cmper) + " has an incomplete trailing element."});
        }
        for (std::size_t at = 0; at + stride <= payload.size(); at += stride) {
            const auto inci = static_cast<musx::dom::Inci>(at / stride);
            const auto& row = rows.front();
            auto target = createOthersRecordTarget<PercussionNoteInfoTarget>(context.document, source, row, cmper, inci);
            if (!target) {
                continue;
            }

            target->percNoteType = payloadWord(payload, at, context.profile.byteOrder);
            target->staffPosition = static_cast<std::int16_t>(payloadWord(payload, at + 2, context.profile.byteOrder));
            if (wide) {
                target->closedNotehead = static_cast<char32_t>(payloadLong(payload, at + 4, context.profile.byteOrder, LongWordOrder::LowFirst));
                target->halfNotehead = static_cast<char32_t>(payloadLong(payload, at + 8, context.profile.byteOrder, LongWordOrder::LowFirst));
                target->wholeNotehead = static_cast<char32_t>(payloadLong(payload, at + 12, context.profile.byteOrder, LongWordOrder::LowFirst));
                target->dwholeNotehead = static_cast<char32_t>(payloadLong(payload, at + 16, context.profile.byteOrder, LongWordOrder::LowFirst));
                if (!std::all_of(payload.begin() + static_cast<std::ptrdiff_t>(at + percussionNoteInfoWideDataSize),
                        payload.begin() + static_cast<std::ptrdiff_t>(at + stride), [](std::uint8_t value) { return value == 0; })) {
                    context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                        "Percussion note map " + std::to_string(cmper) + " has nonzero trailing words in a Finale 2012 element."});
                }
            } else {
                target->closedNotehead = percussionNotehead(payloadWord(payload, at + 4, context.profile.byteOrder), context.document, fontId);
                target->halfNotehead = percussionNotehead(payloadWord(payload, at + 6, context.profile.byteOrder), context.document, fontId);
                target->wholeNotehead = percussionNotehead(payloadWord(payload, at + 8, context.profile.byteOrder), context.document, fontId);
                target->dwholeNotehead = percussionNotehead(payloadWord(payload, at + 10, context.profile.byteOrder), context.document, fontId);
            }
            withReporting(context.report,
                [&]<typename Reporting>(Reporting& reporting) { reportPercussionNoteInfo(reporting, *target, source, row, at, wide); });
            context.document->getOthers()->add(PercussionNoteInfoTarget::XmlNodeName, std::move(target));
        }
    }
}

} // namespace others
} // namespace finale_mus_reader
