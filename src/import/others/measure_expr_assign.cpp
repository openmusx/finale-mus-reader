// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"
#include "import/shared/coda_expression_shape.h"
#include "import/shared/expression_common.h"
#include "import/shared/measure_records.h"
#include "musx/musx.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <tuple>

namespace finale_mus_reader {
namespace others {
namespace {

using ExpressionAssign = musx::dom::others::MeasureExprAssign;
constexpr auto expressionAssignTag = records::packTag("DY");
constexpr records::LegacyTag expressionAssignClass = 0x00b1;
constexpr const char* expressionAssignFields[] = {"textExprId", "shapeExprId", "horzEvpuOff", "eduPosition", "vertEvpuOff", "staffAssign", "layer",
    "voice2", "channelSwitch", "dontScaleWithEntry", "playbackStart", "showStaffList", "createdByHp", "hidden", "staffGroup", "staffList",
    "graceNoteIndex", "rehearsalMarkOffset"};

std::optional<ExpressionAssign::ChannelSwitchTarget> expressionAssignmentChannel(unsigned channel)
{
    using Channel = ExpressionAssign::ChannelSwitchTarget;
    constexpr Channel channels[] = {
        Channel::Current, Channel::ToLayer1, Channel::ToLayer2, Channel::ToLayer3, Channel::ToLayer4, Channel::ToChord, Channel::ToExpression};
    return channel < std::size(channels) ? std::optional(channels[channel]) : std::nullopt;
}

void importModernExpressionAssigns(const ImportContext& context)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), expressionAssignTag, expressionAssignClass);
    if (!source) {
        return;
    }
    for (const auto& [partId, measure] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, measure, 0, partId);
        const auto words = collectRecordWords(*source, rows, context.profile.byteOrder);
        constexpr std::size_t stride = 12;
        if (words.size() % stride != 0) {
            context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning, "Measure expression assignment has an incomplete tuple."});
        }
        for (std::size_t at = 0; at + stride <= words.size(); at += stride) {
            const auto inci = static_cast<musx::dom::Inci>(at / stride);
            auto target = createOthersRecordTarget<ExpressionAssign>(context.document, *source, source->rowOfWord(rows, at), measure, inci);
            if (!target) {
                continue;
            }
            const auto instance = ReportInstance::of<ExpressionAssign>(partId, measure, inci);
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.instanceKey(instance);
                reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
                for (const auto* member : expressionAssignFields) {
                    reportFallbackField(reporting, key, member, Reporting::Origin::Unmapped, 0);
                }
            });
            const auto assign = [&](auto member, const char* name, auto value, std::size_t slot) {
                target.get()->*member = value;
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    reportLegacyField(reporting, reporting.instanceKey(instance), *source, source->rowOfWord(rows, at + slot), name,
                        source->byteOffsetInRow((at + slot) * 2), words[at + slot]);
                });
            };
            const auto flags = static_cast<std::uint16_t>(words[at + 5]);
            const auto id = static_cast<musx::dom::Cmper>(words[at]);
            assign(&ExpressionAssign::textExprId, "textExprId", (flags & 0x2000U) ? musx::dom::Cmper(0) : id, 0);
            assign(&ExpressionAssign::shapeExprId, "shapeExprId", (flags & 0x2000U) ? id : musx::dom::Cmper(0), 0);
            assign(&ExpressionAssign::horzEvpuOff, "horzEvpuOff", words[at + 2], 2);
            const auto edu = (std::uint32_t(static_cast<std::uint16_t>(words[at + 8])) << 16U) | static_cast<std::uint16_t>(words[at + 1]);
            assign(&ExpressionAssign::eduPosition, "eduPosition", static_cast<std::int32_t>(edu), 1);
            assign(&ExpressionAssign::vertEvpuOff, "vertEvpuOff", words[at + 3], 3);
            assign(&ExpressionAssign::staffAssign, "staffAssign", words[at + 4], 4);
            if ((flags & 7U) <= 4) {
                assign(&ExpressionAssign::layer, "layer", flags & 7U, 5);
            }
            assign(&ExpressionAssign::voice2, "voice2", bool(flags & 8U), 5);
            if (const auto channel = expressionAssignmentChannel((flags >> 4U) & 7U)) {
                assign(&ExpressionAssign::channelSwitch, "channelSwitch", *channel, 5);
            }
            assign(&ExpressionAssign::dontScaleWithEntry, "dontScaleWithEntry", bool(flags & 0x100U), 5);
            using Playback = ExpressionAssign::PlaybackStart;
            assign(&ExpressionAssign::playbackStart, "playbackStart",
                (flags & 0x200U)   ? Playback::AlignmentPoint
                : (flags & 0x400U) ? Playback::PositionInMeasure
                                   : Playback::BeginningOfMeasure,
                5);
            using Show = ExpressionAssign::ShowStaffList;
            if ((flags & 0x1800U) != 0x1800U) {
                assign(&ExpressionAssign::showStaffList, "showStaffList",
                    (flags & 0x800U)    ? Show::ScoreOnly
                    : (flags & 0x1000U) ? Show::PartOnly
                                        : Show::ScoreAndPart,
                    5);
            }
            assign(&ExpressionAssign::hidden, "hidden", bool(flags & 0x4000U), 5);
            assign(&ExpressionAssign::createdByHp, "createdByHp", bool(flags & 0x8000U), 5);
            assign(&ExpressionAssign::staffGroup, "staffGroup", words[at + 6], 6);
            assign(&ExpressionAssign::staffList, "staffList", static_cast<musx::dom::Cmper>(words[at + 7]), 7);
            assign(&ExpressionAssign::graceNoteIndex, "graceNoteIndex", static_cast<std::uint16_t>(words[at + 9]), 9);
            assign(&ExpressionAssign::rehearsalMarkOffset, "rehearsalMarkOffset", words[at + 10], 10);
            context.document->getOthers()->add(ExpressionAssign::XmlNodeName, std::move(target));
        }
    }
}

using ExprCmper = musx::dom::Cmper;
using ExprStaff = musx::dom::StaffCmper;
using ExprInci = musx::dom::Inci;
using ExprKey = std::pair<ExprCmper, ExprCmper>;

/// @brief Numbers converted legacy assignments the way Finale 27 stores them.
/// @details Score assignments take consecutive incidences per measure. A part assignment that
/// overrides a score assignment takes that assignment's incidence; any other part assignment
/// follows the score's. A part measure holding any assignment holds a partially linked copy of
/// every score assignment it does not override, so its incidences run from zero through at least
/// the score's last. Every score assignment must therefore exist before the first part assignment
/// is placed.
struct LegacyExpressionAssignments
{
    using SourceKey = std::tuple<ExprCmper, ExprStaff, std::uint32_t, std::size_t, bool>;
    std::map<ExprKey, EmittedIncidence> next;
    std::map<SourceKey, ExprInci> scoreIndices;
    std::map<std::tuple<ExprCmper, ExprCmper, ExprInci>, bool> below;

    [[nodiscard]] ExprInci emittedCount(ExprCmper part, ExprCmper measure) const
    {
        const auto found = next.find({part, measure});
        return found == next.end() ? 0 : found->second.current();
    }

    [[nodiscard]] ExprInci index(ExprCmper part, ExprCmper measure, const SourceKey& source) const
    {
        if (part) {
            if (const auto found = scoreIndices.find(source); found != scoreIndices.end()) {
                return found->second;
            }
            return (std::max)(emittedCount(part, measure), emittedCount(musx::dom::SCORE_PARTID, measure));
        }
        return emittedCount(part, measure);
    }

    void add(const ImportContext& context, const SourceKey& source, std::shared_ptr<ExpressionAssign> target)
    {
        const auto part = target->getSourcePartId();
        const auto measure = target->getCmper();
        const auto inci = target->getInci().value();
        if (part) {
            copyScoreAssignments(context, part, measure, inci);
        } else {
            scoreIndices[source] = inci;
        }
        next[{part, measure}].add(*context.document->getOthers(), std::move(target));
    }

    /// @brief Completes every part measure that holds an assignment with the score's remaining ones.
    void completePartMeasures(const ImportContext& context)
    {
        std::vector<ExprKey> partMeasures;
        for (const auto& [key, incidence] : next) {
            if (key.first != musx::dom::SCORE_PARTID) {
                partMeasures.push_back(key);
            }
        }
        for (const auto& [part, measure] : partMeasures) {
            copyScoreAssignments(context, part, measure, emittedCount(musx::dom::SCORE_PARTID, measure));
        }
    }

private:
    /// @brief Adds a fully linked copy of each score assignment from the part's next incidence up to @p end.
    void copyScoreAssignments(const ImportContext& context, ExprCmper part, ExprCmper measure, ExprInci end)
    {
        auto& incidence = next[{part, measure}];
        for (auto inci = incidence.current(); inci < end; inci = incidence.current()) {
            const auto score = context.document->getOthers()->get<ExpressionAssign>(musx::dom::SCORE_PARTID, measure, inci);
            if (!score) {
                throw std::logic_error("A part expression assignment precedes the score assignment it copies.");
            }
            auto copy = std::make_shared<ExpressionAssign>(context.document, part, musx::dom::EnigmaBase::ShareMode::Partial, measure, inci);
            initializePartialFromScore(copy, score);
            if (const auto found = below.find({musx::dom::SCORE_PARTID, measure, inci}); found != below.end()) {
                below[{part, measure, inci}] = found->second;
            }
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                // The copy holds the score assignment's values, so it carries their provenance.
                const auto scoreKey = reporting.template instanceKey<ExpressionAssign>(musx::dom::SCORE_PARTID, measure, inci);
                const auto key = reporting.template instanceKey<ExpressionAssign>(part, measure, inci);
                if (const auto found = reporting.report().fields.find(scoreKey); found != reporting.report().fields.end()) {
                    reporting.report().fields[key] = found->second;
                }
                if (const auto* origin = reporting.report().findInstanceOrigin(scoreKey)) {
                    reporting.report().setInstanceOrigin(key, *origin);
                }
            });
            incidence.add(*context.document->getOthers(), std::move(copy));
        }
    }
};

/// @brief Returns every defined part other than the score, the parts Finale converts expressions in.
std::vector<ExprCmper> definedParts(const ImportContext& context)
{
    std::vector<ExprCmper> parts;
    for (const auto& definition : context.document->getOthers()->getArray<musx::dom::others::PartDefinition>(musx::dom::SCORE_PARTID)) {
        if (definition->getCmper() != musx::dom::SCORE_PARTID) {
            parts.push_back(definition->getCmper());
        }
    }
    return parts;
}

/// @brief Returns each staff's position in the score's base staff order.
std::map<ExprStaff, std::size_t> scoreSlots(const ImportContext& context)
{
    std::map<ExprStaff, std::size_t> slots;
    for (const auto& used :
        context.document->getOthers()->getArray<musx::dom::others::StaffUsed>(musx::dom::SCORE_PARTID, musx::dom::BASE_SYSTEM_ID)) {
        slots.emplace(used->staffId, slots.size());
    }
    return slots;
}

void initializeLegacyAssignment(const ImportContext& context, const ExpressionAssign& target)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<ExpressionAssign>(target.getSourcePartId(), target.getCmper(), target.getInci());
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMusAdjusted);
        for (const auto* name : expressionAssignFields) {
            reportFallbackField(reporting, key, name, Reporting::Origin::LegacyBehavior, 0);
        }
    });
}

void reportLegacyAssignmentField(const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows,
    const ExpressionAssign& target, const char* name, std::size_t word, bool adjusted = false)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<ExpressionAssign>(target.getSourcePartId(), target.getCmper(), target.getInci());
        const auto words = collectRecordWords(source, rows, context.profile.byteOrder);
        reportLegacyField(reporting, key, source, source.rowOfWord(rows, word), name, source.byteOffsetInRow(word * 2), words[word],
            adjusted ? Reporting::Origin::LegacyMusAdjusted : Reporting::Origin::LegacyMus);
    });
}

/// @brief Applies an assignment's separate placement in the assignment's own part.
/// @return The placement's horizontal adjustment, or zero when the part has none.
int applySeparatePlacement(const ImportContext& context, ExpressionAssign& target, ExprStaff staff, std::int16_t key)
{
    using musx::dom::others::StaffUsed;
    const auto part = target.getSourcePartId();
    auto actualStaff = staff;
    if (staff < 0) {
        const auto used = context.document->getOthers()->getArray<StaffUsed>(part, musx::dom::BASE_SYSTEM_ID);
        if (!used.empty()) {
            actualStaff = (staff == -1 ? used.front() : used.back())->staffId;
        }
    }
    const auto placements =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), records::packTag("DI"), 0x00cf);
    if (!placements) {
        return 0;
    }
    auto placementRows = placements->pool->getArray(placements->identity, target.getCmper(), 0, part);
    if (placementRows.empty() && part != musx::dom::SCORE_PARTID) {
        // Parts without individual placements inherit the score placement collection.
        placementRows = placements->pool->getArray(placements->identity, target.getCmper(), 0, musx::dom::SCORE_PARTID);
    }
    const auto placementWords = collectRecordWords(*placements, placementRows, context.profile.byteOrder);
    for (std::size_t pos = 0; pos + 6 <= placementWords.size(); pos += 6) {
        if (placementWords[pos] != actualStaff || placementWords[pos + 1] != key) {
            continue;
        }
        target.vertEvpuOff += placementWords[pos + 3];
        target.hidden = context.profile.epoch == FormatEpoch::ZlibLegacy && (placementWords[pos + 5] & 1);
        reportLegacyAssignmentField(context, *placements, placementRows, target, "vertEvpuOff", pos + 3, true);
        if (context.profile.epoch == FormatEpoch::ZlibLegacy) {
            reportLegacyAssignmentField(context, *placements, placementRows, target, "hidden", pos + 5);
        }
        return placementWords[pos + 2];
    }
    return 0;
}

template <typename List>
bool expressionListContains(const ImportContext& context, ExprCmper part, ExprCmper id, ExprStaff staff)
{
    const auto list = context.document->getOthers()->get<List>(part, id);
    return list && std::ranges::find(list->values, staff) != list->values.end();
}

auto expressionMeasureAlignment(const ImportContext& context, ExprCmper part, ExprCmper id, bool shape)
{
    using Alignment = musx::dom::others::HorizontalMeasExprAlign;
    if (shape) {
        const auto definition = context.document->getOthers()->get<musx::dom::others::ShapeExpressionDef>(part, id);
        if (definition) {
            return definition->horzMeasExprAlign;
        }
    } else {
        const auto definition = context.document->getOthers()->get<musx::dom::others::TextExpressionDef>(part, id);
        if (definition) {
            return definition->horzMeasExprAlign;
        }
    }
    return Alignment::Manual;
}

/// @brief Converts the old measure assignments in the score, or in every defined part.
/// @details Finale converts each score record again in each part, reading the part's own view of
/// the record and resolving placement, floating staves, and timing in that part. Every part
/// therefore holds a list for every measure with a converted score assignment, even where the
/// part changes nothing. A stored part record for an undefined part is not converted.
void importLegacyMeasureAssignments(const ImportContext& context, LegacyExpressionAssignments& state, bool partPass)
{
    using namespace musx::dom::others;
    const auto source =
        selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), expressionAssignTag, expressionAssignClass);
    if (!source) {
        return;
    }
    const bool separateStaff = sourceAtOrAfter(context.profile, FormatEpoch::ZlibLegacy, versions::finale2008);
    const bool discardFlaggedEarlyAssignments = context.profile.epoch == FormatEpoch::UncompressedLegacy
                                                && sourcePredatesVersion(context.profile, FormatEpoch::UncompressedLegacy, versions::finale3_3_v2);
    const bool coda = context.profile.epoch == FormatEpoch::CodaBanner;
    const auto misc = context.document->getOptions()->get<musx::dom::options::MiscOptions>();
    const bool showParts = misc && misc->showRepeatsForParts;
    const auto globals = context.document->getOthers()->get<PartGlobals>(musx::dom::SCORE_PARTID, musx::dom::MUSX_GLOBALS_CMPER);
    const bool specialExtraction = globals && globals->isSpecialPartExtraction();
    // Finale visits the top and bottom floating staves first, then the score's staves in order.
    std::vector<ExprStaff> candidates{-1, -2};
    for (const auto& staff : context.document->getOthers()->getArray<StaffUsed>(musx::dom::SCORE_PARTID, musx::dom::BASE_SYSTEM_ID)) {
        if (std::ranges::find(candidates, staff->staffId) == candidates.end()) {
            candidates.push_back(staff->staffId);
        }
    }
    // A real staff is visible in parts only through a defined part that holds it, judged by that
    // part's own lists. A document without parts judges by the lists alone.
    const auto parts = definedParts(context);
    std::map<ExprStaff, std::vector<ExprCmper>> holdingParts;
    for (const auto holder : parts) {
        for (const auto& staff : context.document->getOthers()->getArray<StaffUsed>(holder, musx::dom::BASE_SYSTEM_ID)) {
            holdingParts[staff->staffId].push_back(holder);
        }
    }
    const auto convertMeasure = [&](ExprCmper part, ExprCmper measure) {
        const auto scoreRows = source->pool->getArray(source->identity, measure, 0, musx::dom::SCORE_PARTID);
        const auto scoreWords = collectRecordWords(*source, scoreRows, context.profile.byteOrder);
        const auto partRows = part ? source->pool->getArray(source->identity, measure, 0, part) : scoreRows;
        const auto partWords = part ? collectRecordWords(*source, partRows, context.profile.byteOrder) : scoreWords;
        struct Pending
        {
            std::size_t at{};
            ExprStaff staff{};
            bool score{};
            bool parts{};
            std::uint16_t flags{};
        };
        std::vector<Pending> pending;
        for (std::size_t at = 0; at + 6 <= scoreWords.size(); at += 6) {
            // A part payload shorter than the score's leaves the remaining assignments linked.
            const auto& words = at + 6 <= partWords.size() ? partWords : scoreWords;
            auto flags = static_cast<std::uint16_t>(words[at + 5]);
            if (discardFlaggedEarlyAssignments && (flags & 0x8000U)) {
                continue;
            }
            if (coda) {
                // Old SmartShape endpoint records share this selector but do not assign expressions.
                if (flags & coda_expression_shape::smartShapeEndpointMask) {
                    continue;
                }
                flags = (flags & 0x0fffU) | ((flags & 0x8000U) ? 0 : 0x2000U) | ((flags >> 1U) & 0x1000U);
            }
            if (sourcePredatesVersion(context.profile, FormatEpoch::UncompressedLegacy, versions::finale3_7)) {
                const auto mask = (flags & 0xffU) ? ((flags & 0x1600U) ? 0xf9ffU : 0xff00U) : 0xe9ffU;
                flags = (flags & 0x100U) ? (flags & mask & 0xfeffU) | 0x400U : flags & mask & 0xfbffU;
            }
            if (!static_cast<std::uint16_t>(words[at])) {
                continue;
            }
            const ExprCmper list = separateStaff ? static_cast<ExprCmper>(words[at + 4]) : static_cast<ExprCmper>(flags & 0x3ff);
            if (flags & 0x1000) {
                pending.push_back({at, static_cast<ExprStaff>(list), true, true, flags});
                continue;
            }
            const auto listedForParts = [&](ExprCmper listPart, ExprStaff staff) {
                return (list == 0 && staff > 0) || expressionListContains<StaffListRepeatParts>(context, listPart, list, staff)
                       || expressionListContains<StaffListRepeatPartsForced>(context, listPart, list, staff);
            };
            for (const auto staff : candidates) {
                bool score = (list == 0 && staff > 0) || expressionListContains<StaffListRepeatScore>(context, part, list, staff)
                             || expressionListContains<StaffListRepeatScoreForced>(context, part, list, staff);
                if (showParts || (staff > 0 && specialExtraction)) {
                    score = listedForParts(part, staff);
                }
                bool partsVisible = listedForParts(part, staff);
                if (staff > 0 && !parts.empty()) {
                    const auto holders = holdingParts.find(staff);
                    partsVisible = holders != holdingParts.end()
                                   && std::ranges::any_of(holders->second, [&](ExprCmper holder) { return listedForParts(holder, staff); });
                }
                if (score || partsVisible) {
                    pending.push_back({at, staff, score, partsVisible, flags});
                }
            }
        }
        if (part) {
            // A part's own record can expand differently; assignments the score also holds keep its
            // order, and the rest follow it.
            std::ranges::stable_sort(pending, [&](const Pending& lhs, const Pending& rhs) {
                const auto order = [&](const Pending& item) {
                    const auto found = state.scoreIndices.find({measure, item.staff, 0, item.at, false});
                    return found == state.scoreIndices.end() ? (std::numeric_limits<std::uint32_t>::max)() : std::uint32_t(found->second);
                };
                return order(lhs) < order(rhs);
            });
        }
        for (const auto& [at, staff, scoreVisible, partsVisible, flags] : pending) {
            const bool ownRecord = !part || at + 6 <= partWords.size();
            const auto rows = ownRecord ? partRows : scoreRows;
            const auto& words = ownRecord ? partWords : scoreWords;
            const LegacyExpressionAssignments::SourceKey sourceKey{measure, staff, 0, at, false};
            const auto inci = state.index(part, measure, sourceKey);
            std::shared_ptr<ExpressionAssign> target;
            if (ownRecord) {
                target = createOthersRecordTarget<ExpressionAssign>(context.document, *source, source->rowOfWord(rows, at), measure, inci);
            } else {
                target = std::make_shared<ExpressionAssign>(context.document, part, musx::dom::EnigmaBase::ShareMode::Partial, measure, inci);
                initializePartialFromScore(target, context.document->getOthers()->get<ExpressionAssign>(musx::dom::SCORE_PARTID, measure, inci));
            }
            if (!target) {
                continue;
            }
            const ExprCmper definition = static_cast<std::uint16_t>(words[at]);
            const bool shape = flags & 0x2000;
            initializeLegacyAssignment(context, *target);
            target->textExprId = shape ? 0 : definition;
            target->shapeExprId = shape ? definition : 0;
            target->staffAssign = staff;
            target->hidden = (flags & 0x4000U) != 0;
            reportLegacyAssignmentField(context, *source, rows, *target, "hidden", at + 5, coda);
            target->vertEvpuOff = words[at + 2];
            reportLegacyAssignmentField(context, *source, rows, *target, "vertEvpuOff", at + 2);
            target->dontScaleWithEntry = true;
            target->playbackStart =
                (flags & 0x400) ? ExpressionAssign::PlaybackStart::PositionInMeasure : ExpressionAssign::PlaybackStart::BeginningOfMeasure;
            target->showStaffList = scoreVisible && partsVisible ? ExpressionAssign::ShowStaffList::ScoreAndPart
                                    : scoreVisible               ? ExpressionAssign::ShowStaffList::ScoreOnly
                                                                 : ExpressionAssign::ShowStaffList::PartOnly;
            int horizontal = words[at + 1];
            if (flags & 0x800) {
                horizontal += applySeparatePlacement(context, *target, staff, words[at + 3]);
            }
            const auto alignment = expressionMeasureAlignment(context, part, definition, shape);
            const bool time = alignment == HorizontalMeasExprAlign::Manual || alignment == HorizontalMeasExprAlign::StartOfMusic;
            target->eduPosition = 0;
            target->horzEvpuOff = 0;
            if (time && horizontal >= 0) {
                target->eduPosition = horizontal;
                if (const auto measureObject = context.document->getOthers()->get<Measure>(part, measure)) {
                    const auto duration = measureObject->calcDuration().calcEduDuration();
                    musx::dom::Edu longest = 0;
                    // Timing uses the assignment staff; floating staves are resolved only for placement.
                    if (context.document->getDetails()->get<musx::dom::details::GFrameHold>(part, staff, measure)) {
                        musx::dom::details::GFrameHoldContext frame(context.document, part, staff, measure);
                        frame.iterateEntries([&](const musx::dom::EntryInfoPtr& entry) {
                            longest = (std::max)(longest, (entry->elapsedDuration + entry->actualDuration).calcEduDuration());
                            return true;
                        });
                    }
                    if (longest > duration && duration > 0) {
                        target->eduPosition = static_cast<musx::dom::Edu>(std::int64_t(horizontal) * longest / duration);
                    }
                }
            } else {
                target->horzEvpuOff = horizontal;
            }
            for (const auto* name : {"textExprId", "shapeExprId"}) {
                reportLegacyAssignmentField(context, *source, rows, *target, name, at);
            }
            reportLegacyAssignmentField(context, *source, rows, *target, time && horizontal >= 0 ? "eduPosition" : "horzEvpuOff", at + 1,
                coda || (flags & 0x800) || target->eduPosition != horizontal);
            reportLegacyAssignmentField(context, *source, rows, *target, "staffAssign", at + (separateStaff ? 4 : 5), !(flags & 0x1000));
            reportLegacyAssignmentField(context, *source, rows, *target, "playbackStart", at + 5, true);
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto key = reporting.template instanceKey<ExpressionAssign>(part, measure, inci);
                reportFallbackField(reporting, key, "dontScaleWithEntry", Reporting::Origin::LegacyBehavior, true);
                reportFallbackField(reporting, key, "showStaffList", Reporting::Origin::LegacyMusAdjusted, static_cast<int>(target->showStaffList));
            });
            state.add(context, sourceKey, std::move(target));
        }
    };
    std::vector<ExprCmper> measures;
    for (const auto& [part, measure] : recordKeys(*source)) {
        if (part == musx::dom::SCORE_PARTID) {
            measures.push_back(measure);
        }
    }
    for (const auto part : partPass ? definedParts(context) : std::vector<ExprCmper>{musx::dom::SCORE_PARTID}) {
        for (const auto measure : measures) {
            convertMeasure(part, measure);
        }
    }
}

/// @brief Sets a measure's expression flag the way Finale's entry conversion does.
/// @details Finale sets the flag only on the score measure. A part measure follows it unless the
/// part's own record unlinks that bit; a compact part record stores no such bit and always follows.
/// That holds for every part storing the measure, including one with no part definition.
void markMeasureHasExpression(const ImportContext& context, ExprCmper measure, const std::set<ExprCmper>& measureParts)
{
    using musx::dom::others::Measure;
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), measureTag, measureClass);
    const auto mark = [&](const musx::dom::MusxInstance<Measure>& target) {
        if (!target || target->hasExpression) {
            return;
        }
        std::const_pointer_cast<Measure>(target)->hasExpression = true;
        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
            const auto key = reporting.template instanceKey<Measure>(target->getSourcePartId(), measure);
            reporting.report().fieldProvenance(key, "hasExpression").origin = Reporting::Origin::LegacyMusAdjusted;
        });
    };
    mark(context.document->getOthers()->get<Measure>(musx::dom::SCORE_PARTID, measure));
    for (const auto part : measureParts) {
        const auto partMeasure = context.document->getOthers()->get<Measure>(part, measure);
        if (!source || !partMeasure || partMeasure->getSourcePartId() != part
            || partMeasure->getShareMode() != musx::dom::EnigmaBase::ShareMode::Partial) {
            continue;
        }
        const auto rows = source->pool->getArray(source->identity, measure, 0, part);
        if (!(partUnlinkedBits(*source, rows, measureSecondaryFlagsSlot, context.profile.byteOrder).value_or(0) & secondaryHasExpressionMask)) {
            mark(partMeasure);
        }
    }
}

void importLegacyEntryAssignments(const ImportContext& context, LegacyExpressionAssignments& state, bool partPass)
{
    const auto source =
        selectRecordFamilySource(context, context.index.getDetails(), context.index.getClassDetails(), records::packTag("ED"), 0x0423, true);
    if (!source) {
        return;
    }
    const bool modernNotePosition = sourceAtOrAfter(context.profile, FormatEpoch::DclLegacy, versions::finale2004);
    std::set<ExprCmper> measureParts;
    for (const auto& measure : context.document->getOthers()->getAllSources<musx::dom::others::Measure>()) {
        if (measure->getSourcePartId() != musx::dom::SCORE_PARTID) {
            measureParts.insert(measure->getSourcePartId());
        }
    }
    const auto slots = scoreSlots(context);
    // Finale visits only score measures and listed staves, in measure and staff order.
    const auto slotOf = [&slots](ExprStaff staff) {
        const auto found = slots.find(staff);
        return found == slots.end() ? slots.size() : found->second;
    };
    for (const auto part : partPass ? definedParts(context) : std::vector<ExprCmper>{musx::dom::SCORE_PARTID}) {
        auto frames = context.document->getDetails()->getArray<musx::dom::details::GFrameHold>(part);
        const auto measureCount = context.document->getOthers()->getArray<musx::dom::others::Measure>(musx::dom::SCORE_PARTID).size();
        std::ranges::stable_sort(frames, [&](const auto& lhs, const auto& rhs) {
            return std::pair(lhs->getMeasure(), slotOf(lhs->getStaff())) < std::pair(rhs->getMeasure(), slotOf(rhs->getStaff()));
        });
        for (const auto& frame : frames) {
            const auto measure = frame->getMeasure();
            if (!slots.contains(frame->getStaff()) || measure <= 0 || static_cast<std::size_t>(measure) > measureCount) {
                continue;
            }
            bool hasAssignment = false;
            frame->iterateRawEntries([&](const musx::dom::MusxInstance<musx::dom::Entry>& entry, musx::dom::LayerIndex) {
                const auto number = entry->getEntryNumber();
                hasAssignment =
                    hasAssignment
                    || !source->pool->getArray(source->identity, static_cast<ExprCmper>(number >> 16U), static_cast<ExprCmper>(number), part).empty();
                return !hasAssignment;
            });
            if (!hasAssignment) {
                continue;
            }
            musx::dom::details::GFrameHoldContext frameContext(context.document, part, frame->getStaff(), measure);
            frameContext.iterateEntries([&](const musx::dom::EntryInfoPtr& entry) {
                const auto number = entry->getEntry()->getEntryNumber();
                const auto rows =
                    source->pool->getArray(source->identity, static_cast<ExprCmper>(number >> 16U), static_cast<ExprCmper>(number), part);
                const auto words = collectRecordWords(*source, rows, context.profile.byteOrder);
                for (std::size_t at = 0; at + 5 <= words.size(); at += 5) {
                    const auto flags = static_cast<std::uint16_t>(words[at + 4]);
                    const auto id = static_cast<ExprCmper>(words[at]);
                    // An assignment naming no definition assigns nothing; Finale discards it.
                    if (!id) {
                        continue;
                    }
                    const auto& row = source->rowOfWord(rows, at + 1);
                    const auto continuation = source->pool->continuationOf(row);
                    const auto selectedMask = [&](std::size_t word) -> std::uint16_t {
                        const auto offset = source->byteOffsetInRow(word * 2);
                        if (offset >= row.payloadSize) {
                            return 0;
                        }
                        constexpr std::size_t prefixSize = sizeof(std::uint32_t);
                        if (offset + prefixSize >= row.payloadSize) {
                            return static_cast<std::uint16_t>(offset + prefixSize == row.payloadSize ? row.trailerFirst : row.trailerSecond);
                        }
                        const auto maskOffset = offset + prefixSize;
                        const auto first = continuation[maskOffset];
                        const auto second = continuation[maskOffset + 1];
                        return context.profile.byteOrder == ByteOrder::BigEndian ? (first << 8U) | second : first | (second << 8U);
                    };
                    // Finale converts a part's assignment only where the part unlinks its horizontal offset,
                    // vertical offset, or hidden bit; otherwise the part keeps the score's conversion.
                    // Sharing depends on the selected bits, even when the part value equals the score's.
                    if (part && row.continuationOverlayReady && !(selectedMask(at + 1) || selectedMask(at + 2) || (selectedMask(at + 4) & 1U))) {
                        continue;
                    }
                    const LegacyExpressionAssignments::SourceKey sourceKey{measure, frame->getStaff(), number, at, true};
                    const auto inci = state.index(part, measure, sourceKey);
                    auto target = createOthersRecordTarget<ExpressionAssign>(context.document, *source, source->rowOfWord(rows, at), measure, inci);
                    if (!target) {
                        continue;
                    }
                    initializeLegacyAssignment(context, *target);
                    target->textExprId = flags & 0x8000 ? id : 0;
                    target->shapeExprId = flags & 0x8000 ? 0 : id;
                    target->staffAssign = frame->getStaff();
                    target->layer = entry.getLayerIndex() + 1;
                    target->voice2 = entry->getEntry()->voice2;
                    target->eduPosition = entry->elapsedDuration.calcEduDuration();
                    target->graceNoteIndex = entry->graceIndex;
                    target->horzEvpuOff = words[at + 1];
                    const auto systems = context.document->getOthers()->getArray<musx::dom::others::StaffSystem>(part);
                    const auto system = std::ranges::find_if(
                        systems, [measure](const auto& value) { return measure >= value->startMeas && measure < value->endMeas; });
                    if (system != systems.end()) {
                        const auto scale = (*system)->calcStaffScaling(frame->getStaff());
                        if (scale > 0) {
                            target->horzEvpuOff = static_cast<musx::dom::Evpu>(std::lround((scale * musx::util::Fraction(words[at + 1])).toDouble()));
                        }
                    }
                    target->vertEvpuOff = words[at + 2];
                    target->hidden = entry->getEntry()->isHidden || (modernNotePosition && (flags & 1));
                    target->playbackStart =
                        flags & 0x100 ? ExpressionAssign::PlaybackStart::PositionInMeasure : ExpressionAssign::PlaybackStart::AlignmentPoint;
                    auto channel = (flags >> 4U) & 0xfU;
                    if (context.profile.epoch == FormatEpoch::CodaBanner) {
                        channel = (channel & 8) ? 1 : (channel & 4) ? 2 : (channel & 2) ? 14 : (channel & 1) ? 15 : 0;
                    }
                    if (const auto targetChannel = expressionAssignmentChannel(channel == 14 ? 5 : channel == 15 ? 6 : channel)) {
                        target->channelSwitch = *targetChannel;
                        reportLegacyAssignmentField(context, *source, rows, *target, "channelSwitch", at + 4, true);
                    } else {
                        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                            const auto key = reporting.template instanceKey<ExpressionAssign>(part, measure, inci);
                            reportFallbackField(reporting, key, "channelSwitch", Reporting::Origin::Unmapped, channel);
                        });
                    }
                    for (const auto* name : {"textExprId", "shapeExprId"}) {
                        reportLegacyAssignmentField(context, *source, rows, *target, name, at);
                    }
                    reportLegacyAssignmentField(context, *source, rows, *target, "horzEvpuOff", at + 1, target->horzEvpuOff != words[at + 1]);
                    reportLegacyAssignmentField(context, *source, rows, *target, "vertEvpuOff", at + 2);
                    for (const auto* name : {"hidden", "playbackStart"}) {
                        reportLegacyAssignmentField(context, *source, rows, *target, name, at + 4, true);
                    }
                    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                        const auto key = reporting.template instanceKey<ExpressionAssign>(part, measure, inci);
                        for (const auto& [name, value] : {std::pair{"staffAssign", std::int64_t(target->staffAssign)}, {"layer", target->layer},
                                 {"voice2", target->voice2}, {"eduPosition", target->eduPosition}, {"graceNoteIndex", target->graceNoteIndex}}) {
                            reportFallbackField(reporting, key, name, Reporting::Origin::LegacyMusAdjusted, value);
                        }
                    });
                    state.below[{part, measure, inci}] = !modernNotePosition && (flags & 0x800);
                    state.add(context, sourceKey, std::move(target));
                    if (!part) {
                        markMeasureHasExpression(context, measure, measureParts);
                    }
                }
                return true;
            });
        }
    }
}

/// @brief Gives a duplicated text expression its own text block and text, as Finale's upgrade does.
/// @details The new block keeps the original's text, frame size, frame shape, low inset word, and
/// frame line, and takes the document's default text layout for everything else: line spacing, no
/// offset, word wrap, single-word expansion, and justification.
void duplicateExpressionTextBlock(const ImportContext& context, musx::dom::others::TextExpressionDef& copy)
{
    using Block = musx::dom::others::TextBlock;
    using RawText = musx::dom::texts::ExpressionText;
    const auto part = copy.getSourcePartId();
    const auto original = context.document->getOthers()->get<Block>(part, copy.textIdKey);
    const auto options = context.document->getOptions()->get<musx::dom::options::TextOptions>();
    if (!original || !options) {
        return;
    }
    const auto text = context.document->getTexts()->get<RawText>(original->textId);
    const auto textNumber = context.document->getTexts()->nextFreeCmper<RawText>();
    const auto blockNumber = context.document->getOthers()->nextFreeCmper<Block>(part);
    if (!text || !textNumber || !blockNumber) {
        context.report.diagnostics.push_back(
            {musx::util::Logger::LogLevel::Warning, "A duplicated text expression could not receive its own text block."});
        return;
    }
    auto rawCopy = std::make_shared<RawText>(context.document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, *textNumber);
    rawCopy->text = text->text;
    auto block = std::make_shared<Block>(context.document, part, musx::dom::EnigmaBase::ShareMode::All, *blockNumber);
    block->textId = *textNumber;
    block->textType = Block::TextType::Expression;
    block->width = original->width;
    block->height = original->height;
    block->shapeId = original->shapeId;
    block->inset = original->inset & 0xffff;
    block->stdLineThickness = original->stdLineThickness;
    block->lineSpacingPercentage = options->textLineSpacingPercent;
    block->lineSpacingEvpu = options->textLineSpacingEvpu;
    block->justify = options->textJustify;
    block->newPos36 = true;
    block->showShape = true;
    block->wordWrap = options->textWordWrap;
    block->noExpandSingleWord = !options->textExpandSingleWord;
    copy.textIdKey = *blockNumber;
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        reporting.report().setInstanceOrigin(
            reporting.template instanceKey<RawText>(musx::dom::SCORE_PARTID, *textNumber), Reporting::Origin::LegacyMusAdjusted);
        reporting.report().setInstanceOrigin(reporting.template instanceKey<Block>(part, *blockNumber), Reporting::Origin::LegacyBehavior);
    });
    context.document->getTexts()->add(RawText::XmlNodeName, std::move(rawCopy));
    context.document->getOthers()->add(Block::XmlNodeName, std::move(block));
}

/// @brief Converts the entry positioning of the score's definitions that entry assignments use.
/// @details Finale changes definitions only in the score. A definition that a measure assignment
/// also uses is duplicated for its entry assignments; otherwise it changes in place. Every part's
/// entry assignment follows the score's choice, and no part receives a definition of its own.
template <typename Definition>
void upgradeExpressionDefinitions(
    const ImportContext& context, const LegacyExpressionAssignments& state, records::LegacyTag tag, records::LegacyTag classId)
{
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), tag, classId);
    if (!source) {
        return;
    }
    constexpr auto part = musx::dom::SCORE_PARTID;
    const bool extended = sourceAtOrAfter(context.profile, FormatEpoch::DclLegacy, versions::finale2004);
    const auto allAssignments = context.document->getOthers()->getAllSources<ExpressionAssign>();
    for (const auto& definition : context.document->getOthers()->getArray<Definition>(part)) {
        const auto shareMode = definition->getShareMode();
        const auto id = definition->getCmper();
        const auto rows = source->pool->getArray(source->identity, id, 0, part);
        const auto words = collectRecordWords(*source, rows, context.profile.byteOrder);
        if (extended && words.size() < 18) {
            continue;
        }
        std::map<bool, std::vector<std::shared_ptr<ExpressionAssign>>> noteAssignments;
        bool hasMeasureAssignment = false;
        for (const auto& assignment : allAssignments) {
            const auto ref = std::is_same_v<Definition, musx::dom::others::TextExpressionDef> ? assignment->textExprId : assignment->shapeExprId;
            if (ref != id) {
                continue;
            }
            const auto found = state.below.find({assignment->getSourcePartId(), assignment->getCmper(), assignment->getInci().value()});
            if (found != state.below.end()) {
                noteAssignments[found->second].push_back(std::const_pointer_cast<ExpressionAssign>(assignment));
            } else if (assignment->getSourcePartId() == part) {
                if constexpr (std::is_same_v<Definition, musx::dom::others::TextExpressionDef>) {
                    // Finale ignores text assignments to absent staves or measures when deciding
                    // whether entry positioning needs a separate definition.
                    const auto staff = assignment->staffAssign;
                    if (staff > 0) {
                        const auto measures = context.document->getOthers()->getArray<musx::dom::others::Measure>(part);
                        if (assignment->getCmper() > measures.size() || !context.document->getOthers()->get<musx::dom::others::Staff>(part, staff)) {
                            continue;
                        }
                    } else if (staff < -3 || staff == 0
                               || context.document->getOthers()->getArray<musx::dom::others::StaffUsed>(part, musx::dom::BASE_SYSTEM_ID).empty()) {
                        continue;
                    }
                }
                hasMeasureAssignment = true;
            }
        }
        bool usedOriginal = hasMeasureAssignment;
        for (const auto& [below, assignments] : noteAssignments) {
            auto target = std::make_shared<Definition>(context.document, part, shareMode, id);
            *target = *definition;
            const auto horizontal = expressionNoteHorizontalAlignment(extended ? words[9] : 1);
            const auto vertical = expressionNoteVerticalAlignment(extended ? words[14] : below ? 4 : 3);
            if (!horizontal || !vertical) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "Expression note alignment has an unknown selector."});
                continue;
            }
            target->horzMeasExprAlign = *horizontal;
            target->vertMeasExprAlign = *vertical;
            if (extended) {
                if (const auto justify = expressionJustification(words[10])) {
                    target->horzExprJustification = *justify;
                }
                target->measXAdjust = words[11];
                target->yAdjustBaseline = words[15];
                target->yAdjustEntry = words[16];
            } else {
                // Earlier definitions store no note positioning; the upgrade's note defaults are
                // left justified with no adjustments.
                target->horzExprJustification = musx::dom::AlignJustify::Left;
                target->measXAdjust = 0;
                target->yAdjustBaseline = 0;
                target->yAdjustEntry = 0;
            }
            // Measure and note positioning share their baseline and entry adjustments, so only the
            // anchors, justification, and horizontal adjustment can differ. Either way the definition
            // takes the shared adjustments.
            const bool differs =
                target->horzMeasExprAlign != definition->horzMeasExprAlign || target->vertMeasExprAlign != definition->vertMeasExprAlign
                || target->horzExprJustification != definition->horzExprJustification || target->measXAdjust != definition->measXAdjust;
            if (differs && usedOriginal) {
                const auto copyId = context.document->getOthers()->nextFreeCmper<Definition>(part);
                if (!copyId) {
                    context.report.diagnostics.push_back(
                        {musx::util::Logger::LogLevel::Warning, "Expression definition comparator space is exhausted."});
                    continue;
                }
                auto copy = std::make_shared<Definition>(context.document, part, shareMode, *copyId);
                *copy = *target;
                target = std::move(copy);
                context.document->getOthers()->add(Definition::XmlNodeName, target);
                if constexpr (std::is_same_v<Definition, musx::dom::others::TextExpressionDef>) {
                    duplicateExpressionTextBlock(context, *target);
                    using TextEnclosure = musx::dom::others::TextExpressionEnclosure;
                    if (const auto enclosure = context.document->getOthers()->get<TextEnclosure>(part, id)) {
                        auto copyEnclosure = std::make_shared<TextEnclosure>(context.document, part, shareMode, target->getCmper());
                        *copyEnclosure = *enclosure;
                        context.document->getOthers()->add(TextEnclosure::XmlNodeName, copyEnclosure);
                        withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                            const auto oldKey = reporting.template instanceKey<TextEnclosure>(part, id);
                            const auto key = reporting.template instanceKey<TextEnclosure>(part, target->getCmper());
                            reporting.report().fields[key] = reporting.report().fields[oldKey];
                            reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMusAdjusted);
                        });
                    }
                }
            } else {
                *std::const_pointer_cast<Definition>(definition) = *target;
                usedOriginal = true;
            }
            withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                const auto oldKey = reporting.template instanceKey<Definition>(part, id);
                const auto key = reporting.template instanceKey<Definition>(part, target->getCmper());
                if (key != oldKey) {
                    reporting.report().fields[key] = reporting.report().fields[oldKey];
                    reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMusAdjusted);
                }
                for (const auto& [name, slot] : {std::pair{"horzMeasExprAlign", 9}, {"vertMeasExprAlign", 14}, {"horzExprJustification", 10},
                         {"measXAdjust", 11}, {"yAdjustBaseline", 15}, {"yAdjustEntry", 16}}) {
                    if (extended) {
                        reportLegacyField(reporting, key, *source, source->rowOfWord(rows, slot), name, source->byteOffsetInRow(slot * 2),
                            words[slot], Reporting::Origin::LegacyMusAdjusted);
                    } else {
                        reportFallbackField(reporting, key, name, Reporting::Origin::LegacyBehavior, 0);
                    }
                }
            });
            for (const auto& assignment : assignments) {
                constexpr bool text = std::is_same_v<Definition, musx::dom::others::TextExpressionDef>;
                (text ? assignment->textExprId : assignment->shapeExprId) = target->getCmper();
                if (target->getCmper() != id) {
                    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                        const auto key = reporting.template instanceKey<ExpressionAssign>(
                            assignment->getSourcePartId(), assignment->getCmper(), assignment->getInci());
                        reporting.report().fieldProvenance(key, text ? "textExprId" : "shapeExprId").origin = Reporting::Origin::LegacyMusAdjusted;
                    });
                }
            }
        }
    }
}

void convertLegacyExpressionAssignments(const ImportContext& context)
{
    LegacyExpressionAssignments state;
    // Part assignments are numbered against the score's, so every score assignment comes first.
    for (const bool partPass : {false, true}) {
        importLegacyMeasureAssignments(context, state, partPass);
        importLegacyEntryAssignments(context, state, partPass);
    }
    state.completePartMeasures(context);
    if (context.profile.epoch == FormatEpoch::CodaBanner) {
        for (const auto& assignment : context.document->getOthers()->getAllSources<ExpressionAssign>()) {
            if (assignment->eduPosition <= 0) {
                continue;
            }
            if (const auto measure =
                    context.document->getOthers()->get<musx::dom::others::Measure>(assignment->getSourcePartId(), assignment->getCmper())) {
                const auto position = measure->calcEduFromEvpu(assignment->eduPosition, beatChartJustification(context, *measure), true);
                const auto target = std::const_pointer_cast<ExpressionAssign>(assignment);
                target->eduPosition = (std::max)(musx::dom::Edu(0), position);
                if (position < 0) {
                    target->horzEvpuOff = position;
                }
                withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
                    const auto key = reporting.template instanceKey<ExpressionAssign>(
                        assignment->getSourcePartId(), assignment->getCmper(), assignment->getInci());
                    auto& eduField = reporting.report().fieldProvenance(key, "eduPosition");
                    eduField.origin = Reporting::Origin::LegacyMusAdjusted;
                    if (position < 0) {
                        reporting.report().fieldProvenance(key, "horzEvpuOff") = eduField;
                    }
                });
            }
        }
    }
    upgradeExpressionDefinitions<musx::dom::others::TextExpressionDef>(context, state, textExpressionTag, textExpressionClass);
    upgradeExpressionDefinitions<musx::dom::others::ShapeExpressionDef>(context, state, shapeExpressionTag, shapeExpressionClass);
}

} // namespace

void importMeasureExprAssigns(const ImportContext& context)
{
    if (sourceAtOrAfter(context.profile, FormatEpoch::ZlibLegacy, versions::finale2009)) {
        importModernExpressionAssigns(context);
    } else {
        context.pending.defer(DeferredStage::ReadComposites, [&context] { convertLegacyExpressionAssignments(context); });
    }
}

} // namespace others
} // namespace finale_mus_reader
