// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include <filesystem>
#include <stdexcept>
#include <string>

#include "finale_mus_reader/reader.h"
#include "musx/musx.h"

#ifndef MUSX_USE_PUGIXML
#define MUSX_USE_PUGIXML
#define FINALE_MUS_READER_CONSTRUCTION_TEST_UNDEFINE_MUSX_USE_PUGIXML
#endif // !defined(MUSX_USE_PUGIXML)

#include "musx/xml/PugiXmlImpl.h"

#ifdef FINALE_MUS_READER_CONSTRUCTION_TEST_UNDEFINE_MUSX_USE_PUGIXML
#undef MUSX_USE_PUGIXML
#undef FINALE_MUS_READER_CONSTRUCTION_TEST_UNDEFINE_MUSX_USE_PUGIXML
#endif // defined(FINALE_MUS_READER_CONSTRUCTION_TEST_UNDEFINE_MUSX_USE_PUGIXML)

// Named rather than anonymous: this file shares a unity translation unit with
// reporting_tests.cpp, whose anonymous-namespace helpers would otherwise collide.
namespace disabled_reporting_construction_test {

using BeatChartElement = musx::dom::others::BeatChartElement;

void check(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

musx::dom::DocumentPtr readFixture(const std::filesystem::path& relativePath)
{
    return finale_mus_reader::Reader::read<musx::xml::pugi::Document>(std::filesystem::path(FINALE_MUS_READER_TEST_SOURCE_DIR) / relativePath);
}

std::shared_ptr<const BeatChartElement> beatChart(const musx::dom::DocumentPtr& document, musx::dom::Inci inci)
{
    return document->getOthers()->get<BeatChartElement>(musx::dom::SCORE_PARTID, musx::dom::Cmper(1), inci);
}

// Report callbacks never run in a non-instrumented build, so values an importer assigns inside
// one silently vanish there. These fixtures are read through the public API in both modes.
void testBeatChartConstruction()
{
    const auto uncompressed = readFixture("evidence/F97/F97-disptime.mus");
    const auto control = beatChart(uncompressed, 0);
    check(control && control->control, "Uncompressed beat chart lost its control");
    check(control->control->totalDur == 4096 && control->control->totalWidth == 624 && control->control->minWidth == 660
              && control->control->allotWidth == 1066,
        "Uncompressed beat chart control values changed");
    const auto last = beatChart(uncompressed, 12);
    check(last && last->dur == 3584 && last->pos == 564 && last->endPos == 624 && last->minPos == 578,
        "Uncompressed beat chart element values changed");

    const auto dcl = readFixture("evidence/F2004/F2004-brakpos-17.mus");
    const auto dclControl = beatChart(dcl, 0);
    check(dclControl && dclControl->control, "DCL beat chart lost its control");
    check(dclControl->control->totalDur == 4096 && dclControl->control->allotWidth == 189, "DCL beat chart control values changed");
    const auto dclElement = beatChart(dcl, 2);
    check(dclElement && dclElement->dur == 1024 && dclElement->pos == 84 && dclElement->endPos == 168 && dclElement->minPos == 59,
        "DCL beat chart element values changed");
}

void testDisabledReportingConstruction()
{
    testBeatChartConstruction();
}

} // namespace disabled_reporting_construction_test
