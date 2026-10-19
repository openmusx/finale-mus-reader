// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "import/support/legacy_mapping.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using PartDefinitionTarget = musx::dom::others::PartDefinition;

// Linked parts arrived with Finale 2007, which is also where the container becomes zlib, so this
// class has exactly one physical layout and it is a class record. No earlier epoch stores a part
// definition under any tag, so every earlier era gets the score object below instead.
constexpr records::LegacyTag partDefinitionClass = 0x011a;

// Six words, unchanged from Finale 2007 through 2012 and in both byte orders.
constexpr std::size_t partDefinitionPayloadSize = 12;
constexpr std::size_t nameIdOffset = 0;
constexpr std::size_t partOrderOffset = 2;
constexpr std::size_t copiesOffset = 4;
constexpr std::size_t flagsOffset = 6;
constexpr std::size_t smartMusicInstOffset = 8;
constexpr std::size_t defaultNameOffset = 10;

constexpr std::uint8_t printPartBit = 0;
constexpr std::uint8_t extractPartBit = 1;
constexpr std::uint8_t applyFormatBit = 2;

// The word at @ref defaultNameOffset names one object of either of two kinds by its sign: a
// positive value is a staff comparator, a negative value is a staff group's comparator negated,
// and zero names neither. musxdom splits the two apart into its own members.
[[nodiscard]] musx::dom::Cmper defaultNameStaffOf(std::int16_t stored)
{
    return stored > 0 ? static_cast<musx::dom::Cmper>(stored) : 0;
}

[[nodiscard]] musx::dom::Cmper defaultNameGroupOf(std::int16_t stored)
{
    return stored < 0 ? static_cast<musx::dom::Cmper>(-stored) : 0;
}

// **Believed:** the word at @ref smartMusicInstOffset carries a SmartMusic instrument only from
// Finale 2011. Earlier releases leave it zero for the score and for every linked part alike, and a
// linked part of such a release had no instrument, which is what -1 spells. The near alternative --
// that the word always carries the instrument and zero simply means none -- would make the stored
// zero correct and is not distinguishable from this by any release's records.
//
// A version gate inside one epoch, deliberately: nothing in the record states which layout it uses,
// so the version decides, and a document whose version cannot be recovered fails closed and keeps
// the stored word. Flag-word bit 8 tracks the same boundary and is tempting as a structural marker,
// but its own meaning is unknown; gating on it would assume a meaning rather than read a shape.
[[nodiscard]] bool storesSmartMusicInstrument(const SourceProfile& profile)
{
    return sourceAtOrAfter(profile, FormatEpoch::ZlibLegacy, versions::finale2011);
}

/// @brief Supplies the score part for a document that stores no record for it.
/// @details Order zero, one copy, printed: what the score was before any release offered a dialog
/// to change them. The name stays null, because a score had no name object before linked parts
/// existed; a later Finale synthesizes one during its own upgrade, and that block is not in the
/// source. Every member is reported as the era's behavior rather than as a default.
void applyScoreBehavior(const ImportContext& context, PartDefinitionTarget& target)
{
    target.partOrder = 0;
    target.copies = 1;
    target.printPart = true;
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<PartDefinitionTarget>(musx::dom::SCORE_PARTID, musx::dom::SCORE_PARTID);
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyBehavior);
        for (const auto& [member, value] : {std::pair<const char*, std::int64_t>{"nameId", 0}, {"partOrder", 0}, {"copies", 1}, {"printPart", 1},
                 {"extractPart", 0}, {"applyFormat", 0}, {"needsRecalc", 0}, {"useAsSmpInst", 0}, {"smartMusicInst", 0}, {"defaultNameStaff", 0},
                 {"defaultNameGroup", 0}}) {
            reporting.report().setField(key, member, typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, value});
        }
    });
}

/// @brief Decodes one stored part definition.
void reportPartDefinition(ImportReport& report, const PartDefinitionTarget& target, const records::LegacyRow& row, records::LegacyTag identity,
    std::uint16_t partId, musx::dom::Cmper cmper, std::int64_t defaultName, bool storesInstrument)
{
    withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<PartDefinitionTarget>(partId, cmper);
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        const auto reportField = [&](const char* member, std::size_t offset, std::int64_t stored) {
            reporting.report().setField(key, member,
                typename Reporting::FieldInfo{Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset + offset, stored, identity});
        };
        reportField("nameId", nameIdOffset, target.nameId);
        reportField("partOrder", partOrderOffset, target.partOrder);
        reportField("copies", copiesOffset, target.copies);
        reportField("printPart", flagsOffset, target.printPart);
        reportField("extractPart", flagsOffset, target.extractPart);
        reportField("applyFormat", flagsOffset, target.applyFormat);
        reportField("defaultNameStaff", defaultNameOffset, defaultName);
        reportField("defaultNameGroup", defaultNameOffset, defaultName);
        for (const auto* member : {"needsRecalc", "useAsSmpInst"}) {
            reporting.report().setField(key, member, typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, !target.isScore()});
        }
        if (storesInstrument) {
            reportField("smartMusicInst", smartMusicInstOffset, target.smartMusicInst);
        } else {
            reporting.report().setField(
                key, "smartMusicInst", typename Reporting::FieldInfo{Reporting::Origin::LegacyBehavior, 0, 0, target.smartMusicInst});
        }
    });
}

void importOnePartDefinition(
    const ImportContext& context, const RecordFamilySource& source, const records::LegacyRow& row, std::uint16_t partId, std::uint16_t cmper)
{
    const auto payload = source.pool->effectivePayloadOf(row);
    if (payload.size() < partDefinitionPayloadSize) {
        context.report.diagnostics.push_back(
            {musx::util::Logger::LogLevel::Info, "Part definition " + std::to_string(cmper) + " is shorter than its layout."});
        return;
    }
    auto instance = createOthersRecordTarget<PartDefinitionTarget>(context.document, source, row, cmper);
    if (!instance) {
        return;
    }
    auto* target = instance.get();
    const auto byteOrder = context.profile.byteOrder;
    const auto word = [&](std::size_t offset) { return static_cast<std::int16_t>(payloadWord(payload, offset, byteOrder)); };
    const auto flags = static_cast<std::uint16_t>(word(flagsOffset));
    const auto defaultName = word(defaultNameOffset);
    const bool storesInstrument = storesSmartMusicInstrument(context.profile);

    target->nameId = static_cast<musx::dom::Cmper>(word(nameIdOffset));
    target->partOrder = word(partOrderOffset);
    target->copies = word(copiesOffset);
    target->printPart = (flags & (1U << printPartBit)) != 0;
    target->extractPart = (flags & (1U << extractPartBit)) != 0;
    target->applyFormat = (flags & (1U << applyFormatBit)) != 0;
    // **Believed:** these two are properties of being a linked part rather than stored values.
    // Neither has a bit in the record or a Finale interface of its own, and a linked part carries
    // both while the score carries neither: a part Finale can extract is a part whose layout it
    // recalculates and whose staves it offers as a SmartMusic instrument.
    //
    // Extraction is not the test, though the extraction bit tracks it in most documents. A part
    // created but never extracted carries both with that bit clear. They are reported as the era's
    // behavior, so a bit later found to carry one of them is not silently overridden.
    target->needsRecalc = !target->isScore();
    target->useAsSmpInst = !target->isScore();
    target->smartMusicInst = storesInstrument ? word(smartMusicInstOffset) : (target->isScore() ? 0 : -1);
    target->defaultNameStaff = defaultNameStaffOf(defaultName);
    target->defaultNameGroup = defaultNameGroupOf(defaultName);

    reportPartDefinition(context.report, *target, row, source.identity, partId, cmper, defaultName, storesInstrument);
    context.document->getOthers()->add(PartDefinitionTarget::XmlNodeName, std::move(instance));
}

/// @brief The one member no known record supplies.
/// @details The record does not carry `unlinkInsts`: no bit of its twelve bytes distinguishes a
/// part that has it set from one that does not. Some other record may, which is what
/// @ref ValueOrigin::Unmapped says and why the member is not synthesized here.
template <typename Reporting>
void reportUnmappedMembers(Reporting& reporting, const typename Reporting::InstanceKey& key, const PartDefinitionTarget& target)
{
    reporting.unmappedField(key, "unlinkInsts", target.unlinkInsts);
}

} // namespace

void importPartDefinitions(const ImportContext& context)
{
    // Named directly rather than through @ref selectRecordFamilySource, because this class has
    // no fixed-row family for that helper to choose between.
    if (sourceMatches(context.profile, EpochMask::Zlib)) {
        const RecordFamilySource source{.pool = &context.index.getClassOthers(), .identity = partDefinitionClass, .classRecords = true};
        for (const auto& [partId, cmper] : recordKeys(source)) {
            const auto rows = source.pool->getArray(source.identity, cmper, 0, partId);
            if (rows.empty()) {
                continue;
            }
            importOnePartDefinition(context, source, rows.front(), partId, cmper);
        }
    }

    // musxdom requires the score part to exist, and every era has one whether or not it wrote a
    // record for it: before Finale 2007 no release wrote one at all, and a document whose record
    // pool did not frame is indistinguishable from one that never stored it.
    // Deliberately not @ref musx::dom::others::PartDefinition::getScore, which raises an integrity
    // error when the score part is absent. Absent is precisely the case being tested for here, and
    // every pre-zlib document would log one on the way to being handled correctly.
    if (!context.document->getOthers()->get<PartDefinitionTarget>(musx::dom::SCORE_PARTID, musx::dom::SCORE_PARTID)) {
        auto instance = std::make_shared<PartDefinitionTarget>(
            context.document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::SCORE_PARTID);
        applyScoreBehavior(context, *instance);
        context.document->getOthers()->add(PartDefinitionTarget::XmlNodeName, std::move(instance));
    }

    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        for (const auto& instance : context.document->getOthers()->getAllSources<PartDefinitionTarget>()) {
            reportUnmappedMembers(
                reporting, reporting.template instanceKey<PartDefinitionTarget>(instance->getSourcePartId(), instance->getCmper()), *instance);
        }
    });
}

} // namespace others
} // namespace finale_mus_reader
