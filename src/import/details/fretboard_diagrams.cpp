// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/details.h"

#include <memory>
#include <string>

#include "import/support/legacy_mapping.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace details {
namespace {

using FretboardDiagramTarget = musx::dom::details::FretboardDiagram;
constexpr records::LegacyTag fretboardDiagramClass = 0x0413;
constexpr records::LegacyTag fretboardDiagramTag = records::packTag("fb");
constexpr std::size_t fretboardDiagramHeaderSize = 10;
constexpr std::size_t fretboardDiagramItemSize = 4;
constexpr std::size_t fretboardDiagramItemsPerIncidence = 2;
constexpr std::size_t fretboardDiagramIncidenceSize = 10;

std::size_t fretboardDiagramItemOffset(std::size_t index)
{
    return fretboardDiagramHeaderSize + (index / fretboardDiagramItemsPerIncidence) * fretboardDiagramIncidenceSize
           + (index % fretboardDiagramItemsPerIncidence) * fretboardDiagramItemSize;
}

std::size_t fretboardDiagramArraySize(std::size_t itemCount)
{
    return ((itemCount + fretboardDiagramItemsPerIncidence - 1) / fretboardDiagramItemsPerIncidence) * fretboardDiagramIncidenceSize;
}

FretboardDiagramTarget::Shape fretboardCellShape(std::uint16_t symbols)
{
    switch (symbols & 0x00ffU) {
    case 0x0001: return FretboardDiagramTarget::Shape::Closed;
    case 0x0002: return FretboardDiagramTarget::Shape::Open;
    case 0x0004: return FretboardDiagramTarget::Shape::Muted;
    case 0x0008: return FretboardDiagramTarget::Shape::Custom;
    default: return FretboardDiagramTarget::Shape::None;
    }
}

void reportFretboardDiagram(ImportReport& report, const FretboardDiagramTarget& target, const records::LegacyRow& row, std::uint16_t partId,
    std::uint16_t cmper1, std::uint16_t cmper2)
{
    withReporting(report, [&]<typename Reporting>(Reporting& reporting) {
        const auto key = reporting.template instanceKey<FretboardDiagramTarget>(partId, cmper1, musx::dom::Inci{}, cmper2);
        reporting.report().setInstanceOrigin(key, Reporting::Origin::LegacyMus);
        const auto report = [&](std::string member, auto value) {
            reporting.report().setField(key, std::move(member), {Reporting::Origin::LegacyMus, row.blockOffset, row.decodedOffset, value});
        };
        report("numFrets", target.numFrets);
        report("fretboardNum", target.fretboardNum);
        report("lock", target.lock);
        report("showNum", target.showNum);
        report("numFretCells", target.numFretCells);
        report("numFretBarres", target.numFretBarres);
        for (std::size_t index = 0; index < target.cells.size(); ++index) {
            report("cells[" + std::to_string(index) + "].string", target.cells[index]->string);
            report("cells[" + std::to_string(index) + "].fret", target.cells[index]->fret);
            report("cells[" + std::to_string(index) + "].shape", static_cast<std::int64_t>(target.cells[index]->shape));
            report("cells[" + std::to_string(index) + "].fingerNum", target.cells[index]->fingerNum);
        }
        for (std::size_t index = 0; index < target.barres.size(); ++index) {
            report("barres[" + std::to_string(index) + "].fret", target.barres[index]->fret);
            report("barres[" + std::to_string(index) + "].startString", target.barres[index]->startString);
            report("barres[" + std::to_string(index) + "].endString", target.barres[index]->endString);
        }
    });
}

} // namespace

void importFretboardDiagrams(const ImportContext& context)
{
    const auto source = selectRecordFamilySource(
        context, context.index.getDetails(), context.index.getClassDetails(), fretboardDiagramTag, fretboardDiagramClass, true);
    if (!source) {
        return;
    }
    for (const auto& [partId, cmper1] : recordKeys(*source)) {
        for (const auto cmper2 : source->pool->secondCmpersForTag(source->identity, cmper1, partId)) {
            const auto rows = source->pool->getArray(source->identity, cmper1, cmper2, partId);
            if (rows.empty()) {
                continue;
            }
            const auto payload = collectRecordPayload(*source, rows);
            if (payload.size() < fretboardDiagramHeaderSize) {
                continue;
            }
            auto target = createDetailsRecordTarget<FretboardDiagramTarget>(context.document, *source, rows.front(), cmper1, cmper2);
            if (!target) {
                continue;
            }
            target->numFrets = payloadWord(payload, 0, context.profile.byteOrder);
            target->fretboardNum = payloadWord(payload, 2, context.profile.byteOrder);
            const auto flags = payloadWord(payload, 4, context.profile.byteOrder);
            target->lock = flags & 0x0001U;
            target->showNum = flags & 0x0004U;
            target->numFretCells = payloadWord(payload, 6, context.profile.byteOrder);
            target->numFretBarres = payloadWord(payload, 8, context.profile.byteOrder);
            const auto cellCount = static_cast<std::size_t>(target->numFretCells);
            const auto barreCount = static_cast<std::size_t>(target->numFretBarres);
            const auto barreOffset = fretboardDiagramHeaderSize + fretboardDiagramArraySize(cellCount);
            const auto requiredSize = barreOffset + fretboardDiagramArraySize(barreCount);
            if (requiredSize > payload.size()) {
                context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                    "Fretboard diagram " + std::to_string(cmper1) + ", " + std::to_string(cmper2) + " has fewer items than its header declares."});
                continue;
            }
            for (int index = 0; index < target->numFretCells; ++index) {
                const auto at = fretboardDiagramItemOffset(static_cast<std::size_t>(index));
                const auto fretString = payloadWord(payload, at, context.profile.byteOrder);
                const auto symbols = payloadWord(payload, at + 2, context.profile.byteOrder);
                auto cell = std::make_shared<FretboardDiagramTarget::Cell>();
                cell->fret = fretString & 0x00ffU;
                cell->string = (fretString & 0xf800U) >> 11U;
                cell->shape = fretboardCellShape(symbols);
                cell->fingerNum = (symbols & 0xe000U) >> 13U;
                target->cells.push_back(std::move(cell));
            }
            for (int index = 0; index < target->numFretBarres; ++index) {
                const auto at = barreOffset + fretboardDiagramItemOffset(static_cast<std::size_t>(index)) - fretboardDiagramHeaderSize;
                const auto fretString = payloadWord(payload, at, context.profile.byteOrder);
                const auto endpoints = payloadWord(payload, at + 2, context.profile.byteOrder);
                auto barre = std::make_shared<FretboardDiagramTarget::Barre>();
                barre->fret = fretString & 0x00ffU;
                const auto firstString = static_cast<unsigned>(endpoints & 0x00ffU);
                const auto lastString = static_cast<unsigned>(endpoints >> 8U);
                barre->startString = (std::min)(firstString, lastString);
                barre->endString = (std::max)(firstString, lastString);
                target->barres.push_back(std::move(barre));
            }
            reportFretboardDiagram(context.report, *target, rows.front(), partId, cmper1, cmper2);
            context.document->getDetails()->add(FretboardDiagramTarget::XmlNodeName, std::move(target));
        }
    }
}

} // namespace details
} // namespace finale_mus_reader
