// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/details.h"

#include <array>
#include <memory>
#include <string>
#include <type_traits>

#include "import/support/text_encoding.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace details {
namespace {

using ShapeNoteValues = musx::dom::details::ShapeNoteBase::NoteShapes;
using ShapeNoteSymbols = musx::dom::options::MusicSymbolOptions;
constexpr std::size_t shapeNoteCount = 101;
struct ShapeNoteColumn
{
    const char* name;
    char32_t ShapeNoteValues::* member;
    char32_t ShapeNoteSymbols::* fallback;
};
constexpr std::array shapeNoteColumns{
    ShapeNoteColumn{"doubleWhole", &ShapeNoteValues::doubleWhole, &ShapeNoteSymbols::noteheadDblWhole},
    ShapeNoteColumn{"whole", &ShapeNoteValues::whole, &ShapeNoteSymbols::noteheadWhole},
    ShapeNoteColumn{"half", &ShapeNoteValues::half, &ShapeNoteSymbols::noteheadHalf},
    ShapeNoteColumn{"quarter", &ShapeNoteValues::quarter, &ShapeNoteSymbols::noteheadQuarter},
};
constexpr VersionBound shapeNoteWideVersion{17, 0, 0, 2, 25};
constexpr VersionBound shapeNoteEncodedVersion{17, 0, 0, 2, 37};

template <typename Target>
musx::dom::Cmper shapeNoteFont(const ImportContext& context, const Target& target)
{
    using Staff =
        std::conditional_t<std::is_same_v<Target, musx::dom::details::ShapeNoteStyle>, musx::dom::others::StaffStyle, musx::dom::others::Staff>;
    if (target.getCmper1() != 0) {
        const auto staff = context.document->getOthers()->get<Staff>(target.getSourcePartId(), target.getCmper1());
        if (staff && staff->useNoteFont && staff->noteFont) {
            return staff->noteFont->fontId;
        }
    }
    const auto font = musx::dom::options::FontOptions::getFontInfoOrNull(context.document, musx::dom::options::FontOptions::FontType::Noteheads);
    return font ? font->fontId : musx::dom::DEFAULT_MUSIC_FONT_ID;
}

template <typename Target>
void recoverShapeNoteFamily(const ImportContext& context, const RecordFamilySource& source)
{
    for (const auto& [partId, cmper] : recordKeys(source)) {
        const records::LegacyRow* first = nullptr;
        bool partial = false;
        for (const auto cmper2 : source.pool->secondCmpersForTag(source.identity, cmper, partId)) {
            if (cmper2 > 4) {
                continue;
            }
            const auto* row = source.pool->get(source.identity, cmper, cmper2, 0, partId);
            if (row) {
                if (!first) {
                    first = row;
                }
                partial |= recordShareMode(source, *row) == musx::dom::EnigmaBase::ShareMode::Partial;
            }
        }
        if (!first) {
            continue;
        }
        using ShareMode = musx::dom::EnigmaBase::ShareMode;
        auto target = std::make_shared<Target>(context.document, partId,
            partId == musx::dom::SCORE_PARTID ? ShareMode::All
            : partial                         ? ShareMode::Partial
                                              : ShareMode::None,
            cmper, musx::dom::Cmper(0));
        const auto symbols = context.document->getOptions()->get<ShapeNoteSymbols>();
        const auto font = shapeNoteFont(context, *target);
        for (std::size_t index = 0; index < shapeNoteCount; ++index) {
            auto values = std::make_shared<ShapeNoteValues>();
            if (symbols) {
                for (const auto& column : shapeNoteColumns) {
                    values.get()->*column.member = symbols.get()->*column.fallback;
                }
            }
            target->noteShapes.push_back(std::move(values));
        }
        const auto rowsFor = [&](musx::dom::Cmper column) {
            auto rows = source.pool->getArray(source.identity, cmper, column, partId);
            // Each duration is independently shared. A missing part array inherits the score.
            if (rows.empty() && partId != musx::dom::SCORE_PARTID) {
                rows = source.pool->getArray(source.identity, cmper, column, musx::dom::SCORE_PARTID);
            }
            return rows;
        };
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto key = reporting.template instanceKey<Target>(partId, cmper, std::nullopt, musx::dom::Cmper(0));
            reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
            reporting.report().setField(key, "arrangedByPitch", {Reporting::Origin::LegacyBehavior, 0, 0, 0});
            for (std::size_t index = 0; index < shapeNoteCount; ++index) {
                for (const auto& column : shapeNoteColumns) {
                    reporting.report().setField(key, "noteShapes[" + std::to_string(index) + "]." + column.name,
                        {symbols ? Reporting::Origin::LegacyBehavior : Reporting::Origin::Unmapped, 0, 0, 0});
                }
            }
        });
        for (musx::dom::Cmper duration = 0; duration < shapeNoteColumns.size(); ++duration) {
            const auto rows = rowsFor(duration);
            if (rows.empty()) {
                continue;
            }
            const auto words = collectRecordWords(source, rows, context.profile.byteOrder);
            // Arrays can end after any slot, so their length cannot identify character width.
            // Believed: fixed-row epochs all use the same narrow array representation.
            const bool wide = sourceAtOrAfter(context.profile, FormatEpoch::ZlibLegacy, shapeNoteWideVersion);
            const bool encoded = sourceAtOrAfter(context.profile, FormatEpoch::ZlibLegacy, shapeNoteEncodedVersion);
            const std::size_t stride = wide ? 2 : 1;
            const auto& column = shapeNoteColumns[duration];
            for (std::size_t index = 0; index < shapeNoteCount && (index + 1) * stride <= words.size(); ++index) {
                const auto at = index * stride;
                const auto raw = wide ? wideCodepoint(words[at + 1], words[at]) : static_cast<std::uint32_t>(static_cast<std::uint16_t>(words[at]));
                if (raw == 0) {
                    break;
                }
                target->noteShapes[index].get()->*column.member =
                    encoded ? static_cast<char32_t>(raw)
                            : text::codepointFromByte(static_cast<std::uint8_t>(raw), context.document, font, text::UnresolvedFontFallback::Symbol);
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    const auto key = reporting.template instanceKey<Target>(partId, cmper, std::nullopt, musx::dom::Cmper(0));
                    const auto& row = source.rowOfWord(rows, at);
                    reporting.report().setField(key, "noteShapes[" + std::to_string(index) + "]." + column.name,
                        {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + source.byteOffsetInRow(at * 2), raw, source.identity});
                });
            }
        }
        const auto flags = rowsFor(4);
        if (!flags.empty()) {
            const auto words = collectRecordWords(source, flags, context.profile.byteOrder);
            if (!words.empty()) {
                target->arrangedByPitch = words.front() != 0;
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    reporting.report().setField(reporting.template instanceKey<Target>(partId, cmper, std::nullopt, musx::dom::Cmper(0)),
                        "arrangedByPitch",
                        {Reporting::Origin::LegacyMus, flags.front().blockOffset, flags.front().decodedOffset, words.front(), source.identity});
                });
            }
        }
        context.document->getDetails()->add(Target::XmlNodeName, std::move(target));
    }
}

template <typename Target>
void importShapeNoteFamily(const ImportContext& context, records::LegacyTag tag, records::LegacyTag classId)
{
    const auto source = selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), tag, classId, true);
    if (source) {
        // Staff font references and document music symbols must be complete before decoding.
        context.pending.defer(
            DeferredStage::NormalizeStoredValues, [&context, source = *source] { recoverShapeNoteFamily<Target>(context, source); });
    }
}

} // namespace

void importShapeNotes(const ImportContext& context)
{
    importShapeNoteFamily<musx::dom::details::ShapeNote>(context, records::packTag("HI"), 0x0426);
}

void importShapeNoteStyles(const ImportContext& context)
{
    importShapeNoteFamily<musx::dom::details::ShapeNoteStyle>(context, records::packTag("HY"), 0x0427);
}

} // namespace details
} // namespace finale_mus_reader
