// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/details.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "musx/musx.h"
#include "records/legacy_record_index.h"

namespace finale_mus_reader {
namespace details {
namespace {

constexpr std::size_t shortBeamWordCount = records::detailWordCount;
constexpr std::size_t fullBeamWordCount = 2U * records::detailWordCount;

constexpr records::LegacyTag downPrimaryBeamClass = 0x0401;
constexpr records::LegacyTag upPrimaryBeamClass = 0x0402;

constexpr records::LegacyTag downSecondaryBeamClass = 0x0403;
constexpr records::LegacyTag upSecondaryBeamClass = 0x0404;

constexpr records::LegacyTag downBeamExtensionClass = 0x03fd;
constexpr records::LegacyTag upBeamExtensionClass = 0x03fe;
constexpr std::uint16_t extensionBeyondEighth = 0x0800;

constexpr records::LegacyTag beamStubDirectionClass = 0x0400;
constexpr records::LegacyTag entrySizeClass = 0x03f0;

constexpr records::LegacyTag secondaryBeamBreakClass = 0x0425;
constexpr std::size_t secondaryBreakBeamCount = 9;

constexpr records::LegacyTag plainStemClass = 0x042a;
constexpr records::LegacyTag beamedStemClass = 0x03ff;
constexpr std::size_t stemAlterationWordCount = records::detailWordCount;
constexpr records::LegacyTag customDownStemClass = 0x042b;
constexpr records::LegacyTag customUpStemClass = 0x042c;

template <typename Target>
void importCustomStemFamily(const ImportContext& context, const char* tag, records::LegacyTag classId)
{
    const auto selected =
        selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), records::packTag(tag), classId, true);
    if (!selected) {
        return;
    }
    const auto& source = *selected;
    for (const auto& [partId, entryHigh] : recordKeys(source)) {
        for (const auto entryLow : source.pool->secondCmpersForTag(source.identity, entryHigh, partId)) {
            if (entryHigh == 0 && entryLow == 0) {
                continue;
            }
            const auto* row = source.pool->get(source.identity, entryHigh, entryLow, 0, partId);
            if (!row) {
                continue;
            }
            const auto words = collectRecordWords(source, std::span(row, 1), context.profile.byteOrder);
            if (words.size() != records::detailWordCount) {
                continue;
            }
            const auto entry = (static_cast<musx::dom::EntryNumber>(entryHigh) << 16U) | entryLow;
            auto target = std::make_shared<Target>(context.document, row->partId, recordShareMode(source, *row), entry);
            target->shapeDef = static_cast<std::uint16_t>(words[0]);
            target->xOffset = words[2];
            target->yOffset = words[3];
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<Target>(row->partId, entryHigh, std::nullopt, entryLow);
                reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                reportLegacyField(reporting, key, source, *row, "shapeDef", source.byteOffsetInRow(0), target->shapeDef);
                reportLegacyField(reporting, key, source, *row, "xOffset", source.byteOffsetInRow(4), target->xOffset);
                reportLegacyField(reporting, key, source, *row, "yOffset", source.byteOffsetInRow(6), target->yOffset);
            });
            context.document->getDetails()->add(Target::XmlNodeName, std::move(target));
        }
    }
}

unsigned secondaryBeamBreakMask(std::span<const std::uint8_t> payload, bool flipWordBytes)
{
    unsigned mask = 0;
    for (std::size_t beam = 0; beam < secondaryBreakBeamCount; ++beam) {
        if (payload[flipWordBytes ? beam ^ 1U : beam] != 0) {
            mask |= unsigned(musx::dom::NoteType::Note16th) >> beam;
        }
    }
    return mask;
}

void importBeamStubDirectionRecord(const ImportContext& context)
{
    const auto selected = selectRecordFamilySource(
        context, context.index.getDetails(), context.index.getClassDetails(), records::packTag("ub"), beamStubDirectionClass, true);
    if (!selected) {
        return;
    }
    const auto& source = *selected;
    using BeamStubDirectionTarget = musx::dom::details::BeamStubDirection;
    for (const auto& [partId, entryHigh] : recordKeys(source)) {
        for (const auto entryLow : source.pool->secondCmpersForTag(source.identity, entryHigh, partId)) {
            if (entryHigh == 0 && entryLow == 0) {
                continue;
            }
            const auto* row = source.pool->get(source.identity, entryHigh, entryLow, 0, partId);
            if (!row) {
                continue;
            }
            const auto words = collectRecordWords(source, std::span(row, 1), context.profile.byteOrder);
            if (words.size() != records::detailWordCount) {
                continue;
            }
            const auto entry = (static_cast<musx::dom::EntryNumber>(entryHigh) << 16U) | entryLow;
            auto target = std::make_shared<BeamStubDirectionTarget>(context.document, row->partId, recordShareMode(source, *row), entry);
            const auto storedMask = static_cast<std::uint16_t>(words[4]);
            target->mask = storedMask & 0x03ffU;
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<BeamStubDirectionTarget>(row->partId, entryHigh, std::nullopt, entryLow);
                reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                reportLegacyField(reporting, key, source, *row, "mask", source.byteOffsetInRow(8), storedMask);
            });
            context.document->getDetails()->add(BeamStubDirectionTarget::XmlNodeName, std::move(target));
        }
    }
}

void importEntrySizeRecord(const ImportContext& context)
{
    const auto selected =
        selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), records::packTag("AS"), entrySizeClass, true);
    if (!selected) {
        return;
    }
    const auto& source = *selected;
    using EntrySizeTarget = musx::dom::details::EntrySize;
    for (const auto& [partId, entryHigh] : recordKeys(source)) {
        for (const auto entryLow : source.pool->secondCmpersForTag(source.identity, entryHigh, partId)) {
            if (entryHigh == 0 && entryLow == 0) {
                continue;
            }
            const auto* row = source.pool->get(source.identity, entryHigh, entryLow, 0, partId);
            if (!row) {
                continue;
            }
            const auto words = collectRecordWords(source, std::span(row, 1), context.profile.byteOrder);
            if (words.size() != records::detailWordCount) {
                continue;
            }
            const auto entry = (static_cast<musx::dom::EntryNumber>(entryHigh) << 16U) | entryLow;
            auto target = std::make_shared<EntrySizeTarget>(context.document, row->partId, recordShareMode(source, *row), entry);
            target->percent = words[1];
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<EntrySizeTarget>(row->partId, entryHigh, std::nullopt, entryLow);
                reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                reportLegacyField(reporting, key, source, *row, "percent", source.byteOffsetInRow(2), words[1]);
            });
            context.document->getDetails()->add(EntrySizeTarget::XmlNodeName, std::move(target));
        }
    }
}

void importSecondaryBeamBreakRecord(const ImportContext& context)
{
    const auto selected = selectRecordFamilySource(
        context, context.index.getDetails(), context.index.getClassDetails(), records::packTag("sB"), secondaryBeamBreakClass, true);
    if (!selected) {
        return;
    }
    const auto& source = *selected;
    using SecondaryBeamBreakTarget = musx::dom::details::SecondaryBeamBreak;
    for (const auto& [partId, entryHigh] : recordKeys(source)) {
        for (const auto entryLow : source.pool->secondCmpersForTag(source.identity, entryHigh, partId)) {
            if (entryHigh == 0 && entryLow == 0) {
                continue;
            }
            const auto* row = source.pool->get(source.identity, entryHigh, entryLow, 0, partId);
            if (!row) {
                continue;
            }
            const auto payload = source.pool->effectivePayloadOf(*row);
            if (payload.size() != records::detailInciByteCount) {
                continue;
            }
            const auto entry = (static_cast<musx::dom::EntryNumber>(entryHigh) << 16U) | entryLow;
            auto target = std::make_shared<SecondaryBeamBreakTarget>(context.document, row->partId, recordShareMode(source, *row), entry);
            target->mask = secondaryBeamBreakMask(payload, false);
            const auto beamEnd = payload.begin() + secondaryBreakBeamCount;
            const auto firstSet = std::find_if(payload.begin(), beamEnd, [](std::uint8_t value) { return value != 0; });
            target->breakThrough = firstSet != beamEnd && std::all_of(firstSet, beamEnd, [](std::uint8_t value) { return value != 0; });
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<SecondaryBeamBreakTarget>(row->partId, entryHigh, std::nullopt, entryLow);
                reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                auto maskInfo = typename Reporting::FieldInfo{
                    Reporting::Origin::LegacyMus, row->blockOffset, row->decodedOffset + source.byteOffsetInRow(0), target->mask, source.identity};
                // A word-ordered upgrade exchanges each byte pair, including the unused tenth byte.
                maskInfo.finaleUpgradeLossValue = secondaryBeamBreakMask(payload, true);
                reporting.report().setField(key, "mask", std::move(maskInfo));
                reportFallbackField(reporting, key, "breakThrough", Reporting::Origin::LegacyBehavior, target->breakThrough);
            });
            context.document->getDetails()->add(SecondaryBeamBreakTarget::XmlNodeName, std::move(target));
        }
    }
}

template <typename Target>
void importStemAlterationFamily(const ImportContext& context, const char* tag, records::LegacyTag classId)
{
    const auto selected =
        selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), records::packTag(tag), classId, true);
    if (!selected) {
        return;
    }
    const auto& source = *selected;
    for (const auto& [partId, entryHigh] : recordKeys(source)) {
        for (const auto entryLow : source.pool->secondCmpersForTag(source.identity, entryHigh, partId)) {
            if (entryHigh == 0 && entryLow == 0) {
                continue;
            }
            const auto* row = source.pool->get(source.identity, entryHigh, entryLow, 0, partId);
            if (!row) {
                continue;
            }
            const auto words = collectRecordWords(source, std::span(row, 1), context.profile.byteOrder);
            if (words.size() != stemAlterationWordCount) {
                continue;
            }
            const auto entry = (static_cast<musx::dom::EntryNumber>(entryHigh) << 16U) | entryLow;
            auto target = std::make_shared<Target>(context.document, row->partId, recordShareMode(source, *row), entry);
            target->upVertAdjust = words[0];
            target->downVertAdjust = words[1];
            const auto packed = static_cast<std::uint16_t>(words[4]);
            const auto signedByte = [](std::uint16_t value) { return static_cast<musx::dom::Evpu>(value < 0x80 ? value : value - 0x100); };
            const auto upHorizontal = signedByte(packed >> 8U);
            const auto downHorizontal = signedByte(packed & 0xffU);
            target->upHorzAdjust = upHorizontal;
            target->downHorzAdjust = downHorizontal;
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<Target>(target->getSourcePartId(), entryHigh, std::nullopt, entryLow);
                reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                const auto reportField = [&](const char* name, std::size_t byteOffset, std::int64_t value) {
                    reportLegacyField(reporting, key, source, *row, name, source.byteOffsetInRow(byteOffset), value);
                };
                reportField("upVertAdjust", 0, words[0]);
                reportField("downVertAdjust", 2, words[1]);
                const bool bigEndian = context.profile.byteOrder == ByteOrder::BigEndian;
                reportField("upHorzAdjust", 8 + (bigEndian ? 0 : 1), upHorizontal);
                reportField("downHorzAdjust", 8 + (bigEndian ? 1 : 0), downHorizontal);
            });
            context.document->getDetails()->add(Target::XmlNodeName, std::move(target));
        }
    }
}

template <typename Target>
void importBeamExtensionFamily(const ImportContext& context, const char* tag, records::LegacyTag classId)
{
    const auto selected =
        selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), records::packTag(tag), classId, true);
    if (!selected) {
        return;
    }
    const auto& source = *selected;
    for (const auto& [partId, entryHigh] : recordKeys(source)) {
        for (const auto entryLow : source.pool->secondCmpersForTag(source.identity, entryHigh, partId)) {
            if (entryHigh == 0 && entryLow == 0) {
                continue;
            }
            const auto* row = source.pool->get(source.identity, entryHigh, entryLow, 0, partId);
            if (!row) {
                continue;
            }
            const auto words = collectRecordWords(source, std::span(row, 1), context.profile.byteOrder);
            if (words.size() != shortBeamWordCount && words.size() != fullBeamWordCount) {
                continue;
            }
            const auto entry = (static_cast<musx::dom::EntryNumber>(entryHigh) << 16U) | entryLow;
            auto target = std::make_shared<Target>(context.document, row->partId, recordShareMode(source, *row), entry);
            target->leftOffset = words[0];
            target->rightOffset = words[1];
            const auto packed = static_cast<std::uint16_t>(words[4]);
            target->mask = packed & 0x03ffU;
            // Believed: this independent bit represents the legacy beyond-eighth choice.
            target->extBeyond8th = (packed & extensionBeyondEighth) != 0;
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<Target>(row->partId, entryHigh, std::nullopt, entryLow);
                reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                reportLegacyField(reporting, key, source, *row, "leftOffset", source.byteOffsetInRow(0), words[0]);
                reportLegacyField(reporting, key, source, *row, "rightOffset", source.byteOffsetInRow(2), words[1]);
                reportLegacyField(reporting, key, source, *row, "mask", source.byteOffsetInRow(8), packed);
                reportLegacyField(reporting, key, source, *row, "extBeyond8th", source.byteOffsetInRow(8), packed);
            });
            context.document->getDetails()->add(Target::XmlNodeName, std::move(target));
        }
    }
}

template <typename Target>
void reportBeamAlteration(const ImportContext& context, const Target& target, const RecordFamilySource& source,
    std::span<const records::LegacyRow> rows, std::span<const std::int16_t> words, std::size_t at)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto entry = target.getEntryNumber();
        const auto key = reporting.template instanceKey<Target>(
            target.getSourcePartId(), static_cast<musx::dom::Cmper>(entry >> 16U), target.getInci(), static_cast<musx::dom::Cmper>(entry));
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        constexpr std::array<const char*, 7> names = {
            "leftOffsetH", "leftOffsetY", "rightOffsetH", "rightOffsetY", "dura", "flattenStyle", "beamWidth"};
        const auto mappedFields = (std::min)(words.size(), names.size());
        for (std::size_t slot = 0; slot < mappedFields; ++slot) {
            const auto& row = source.rowOfWord(rows, at + slot);
            reporting.report().setField(key, names[slot],
                {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + source.byteOffsetInRow((at + slot) * sizeof(std::uint16_t)),
                    words[slot], source.identity});
        }
        for (std::size_t slot = mappedFields; slot < names.size(); ++slot) {
            const auto value = slot == 5 ? static_cast<std::int64_t>(target.flattenStyle) : target.beamWidth;
            const auto origin = slot == 5 ? Reporting::Origin::Finale27Default : Reporting::Origin::LegacyBehavior;
            reportFallbackField(reporting, key, names[slot], origin, value);
        }
    });
}

template <typename Target, bool Secondary>
void importBeamAlterationFamily(const ImportContext& context, const char* tag, records::LegacyTag classId)
{
    const auto selected =
        selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), records::packTag(tag), classId, true);
    if (!selected) {
        return;
    }
    const auto& source = *selected;
    const bool beamWidthStoredAsEvpu = sourcePredatesVersion(context.profile, FormatEpoch::DclLegacy, versions::finale2002);
    // A logical alteration is a pair of five-word physical incidences whose second half may be
    // absent at the end of a record. Finale converts an earlier source's rows one by one, each
    // into a complete alteration with the default mode and width.
    const auto elementWords =
        sourcePredatesVersion(context.profile, FormatEpoch::UncompressedLegacy, versions::finale2000_build1) ? shortBeamWordCount : fullBeamWordCount;
    for (const auto& [partId, entryHigh] : recordKeys(source)) {
        for (const auto entryLow : source.pool->secondCmpersForTag(source.identity, entryHigh, partId)) {
            const auto rows = source.pool->getArray(source.identity, entryHigh, entryLow, partId);
            const auto words = collectRecordWords(source, rows, context.profile.byteOrder);
            const auto entry = (static_cast<musx::dom::EntryNumber>(entryHigh) << 16U) | entryLow;
            std::size_t at = 0;
            if constexpr (!Secondary) {
                // Finale keeps one primary alteration per entry. Believed: the last element.
                at = words.empty() ? 0 : (words.size() - 1) / elementWords * elementWords;
            }
            musx::dom::Inci inci = 0;
            while (at < words.size()) {
                const auto count = (std::min)(elementWords, words.size() - at);
                if (count != shortBeamWordCount && count != fullBeamWordCount) {
                    context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning,
                        "Beam alteration for entry " + std::to_string(entry) + " does not contain a complete five- or ten-word element."});
                    break;
                }
                const auto shareMode = recordShareMode(source, rows.front());
                std::shared_ptr<Target> target;
                if constexpr (Secondary) {
                    target = std::make_shared<Target>(context.document, rows.front().partId, shareMode, entry, inci);
                    if (shareMode == musx::dom::EnigmaBase::ShareMode::Partial) {
                        initializePartialFromScore(target, context.document->getDetails()->get<Target>(musx::dom::SCORE_PARTID, entry, inci));
                    }
                } else {
                    target = std::make_shared<Target>(context.document, rows.front().partId, shareMode, entry);
                    if (shareMode == musx::dom::EnigmaBase::ShareMode::Partial) {
                        initializePartialFromScore(target, context.document->getDetails()->get<Target>(musx::dom::SCORE_PARTID, entry));
                    }
                }
                const std::span<const std::int16_t> element(words.data() + at, count);
                target->leftOffsetH = element[0];
                target->leftOffsetY = element[1];
                target->rightOffsetH = element[2];
                target->rightOffsetY = element[3];
                target->dura = element[4];
                target->beamWidth = -1;
                if (count == fullBeamWordCount) {
                    target->flattenStyle = static_cast<typename Target::FlattenStyle>(element[5]);
                    if constexpr (!Secondary) {
                        const auto storedWidth = static_cast<musx::dom::Efix>(element[6]);
                        target->beamWidth = beamWidthStoredAsEvpu && storedWidth == 0 ? -1
                                            : beamWidthStoredAsEvpu && storedWidth > 0
                                                ? storedWidth * static_cast<musx::dom::Efix>(musx::dom::EFIX_PER_EVPU)
                                                : storedWidth;
                    } else {
                        target->beamWidth = element[6];
                    }
                }
                reportBeamAlteration(context, *target, source, rows, element, at);
                context.document->getDetails()->add(Target::XmlNodeName, std::move(target));
                at += count;
                ++inci;
            }
        }
    }
}

} // namespace

void importBeamAlterationsDownStem(const ImportContext& context)
{
    importBeamAlterationFamily<musx::dom::details::BeamAlterationsDownStem, false>(context, "BL", downPrimaryBeamClass);
}

void importBeamAlterationsUpStem(const ImportContext& context)
{
    importBeamAlterationFamily<musx::dom::details::BeamAlterationsUpStem, false>(context, "BH", upPrimaryBeamClass);
}

void importBeamExtensionDownStem(const ImportContext& context)
{
    importBeamExtensionFamily<musx::dom::details::BeamExtensionDownStem>(context, "DE", downBeamExtensionClass);
}

void importBeamExtensionUpStem(const ImportContext& context)
{
    importBeamExtensionFamily<musx::dom::details::BeamExtensionUpStem>(context, "UE", upBeamExtensionClass);
}

void importBeamStubDirection(const ImportContext& context)
{
    importBeamStubDirectionRecord(context);
}

void importCustomDownStem(const ImportContext& context)
{
    importCustomStemFamily<musx::dom::details::CustomDownStem>(context, "sl", customDownStemClass);
}

void importCustomUpStem(const ImportContext& context)
{
    importCustomStemFamily<musx::dom::details::CustomUpStem>(context, "SU", customUpStemClass);
}

void importEntrySize(const ImportContext& context)
{
    importEntrySizeRecord(context);
}

void importSecondaryBeamAlterationsDownStem(const ImportContext& context)
{
    importBeamAlterationFamily<musx::dom::details::SecondaryBeamAlterationsDownStem, true>(context, "bL", downSecondaryBeamClass);
}

void importSecondaryBeamAlterationsUpStem(const ImportContext& context)
{
    importBeamAlterationFamily<musx::dom::details::SecondaryBeamAlterationsUpStem, true>(context, "bH", upSecondaryBeamClass);
}

void importSecondaryBeamBreak(const ImportContext& context)
{
    importSecondaryBeamBreakRecord(context);
}

void importStemAlterations(const ImportContext& context)
{
    importStemAlterationFamily<musx::dom::details::StemAlterations>(context, "ST", plainStemClass);
}

void importStemAlterationsUnderBeam(const ImportContext& context)
{
    importStemAlterationFamily<musx::dom::details::StemAlterationsUnderBeam>(context, "St", beamedStemClass);
}

} // namespace details
} // namespace finale_mus_reader
