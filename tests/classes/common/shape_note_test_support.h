// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace classes {

inline musx::dom::DocumentPtr shapeNoteDocument()
{
    return musx::factory::DocumentFactory::create<TestXmlDocument>(std::string(R"(<finale><options><musicSymbolOptions>
        <noteheadDblWhole>87</noteheadDblWhole><noteheadWhole>119</noteheadWhole>
        <noteheadHalf>104</noteheadHalf><noteheadQuarter>113</noteheadQuarter>
        </musicSymbolOptions><fontOptions><font type="noteheads"><fontID>3</fontID></font></fontOptions></options>
        <others><fontName cmper="3"><charsetBank>Mac</charsetBank><charsetVal>4095</charsetVal><name>Test Symbol</name></fontName></others></finale>)"));
}

inline ImportReport importShapeNoteTest(const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile,
    const musx::dom::DocumentPtr& document, bool reverse = false)
{
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, document, report, pending, construction};
    finale_mus_reader::details::importShapeNotes(context);
    finale_mus_reader::details::importShapeNoteStyles(context);
    if (reverse) {
        for (auto& steps : pending.steps) {
            std::reverse(steps.begin(), steps.end());
        }
    }
    finale_mus_reader::runDeferredChecks(pending);
    return report;
}

inline void appendShapeNoteRecord(finale_mus_reader::container::ParsedContainer& parsed, std::uint16_t cmper, std::uint16_t duration,
    std::vector<std::int16_t> words, bool style = false, std::uint16_t part = 0, bool continuation = false,
    const std::vector<std::uint16_t>& masks = {})
{
    if (parsed.formatEpoch != FormatEpoch::ZlibLegacy) {
        words.resize((words.size() + 4) / 5 * 5);
    }
    auto record = parsed.formatEpoch == FormatEpoch::ZlibLegacy
                      ? makeDetailClassContainer(cmper, duration, part, words, parsed.byteOrder, style ? 0x0427 : 0x0426, continuation, masks)
                      : makeDetailContainer(parsed.formatEpoch, cmper, duration, words, style ? "HY" : "HI", parsed.byteOrder);
    if (parsed.blocks.empty()) {
        parsed.blocks = std::move(record.blocks);
    } else {
        auto& block = parsed.blocks.front();
        block.data.insert(block.data.end(), record.blocks.front().data.begin(), record.blocks.front().data.end());
        block.info.decodedSize = block.data.size();
    }
}

} // namespace classes
} // namespace finale_mus_reader_tests
