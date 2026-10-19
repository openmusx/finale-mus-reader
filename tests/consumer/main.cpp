// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include <iostream>
#include <stdexcept>

#include "finale_mus_reader/reader.h"
#include "fixture.h"
#include "musx/xml/PugiXmlImpl.h"

#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
#error Consumer builds should default to disabled instrumentation
#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)

int main()
{
    try {
        const auto reader = finale_mus_reader::Reader::create<musx::xml::pugi::Document>();
        const auto document = reader.read(std::span<const std::uint8_t>(fixture));
        if (!document || !document->getOptions()->get<musx::dom::options::MusicSpacingOptions>()
            || document->getOthers()->getArray<musx::dom::others::Measure>(musx::dom::SCORE_PARTID).empty()
            || document->getOthers()->getArray<musx::dom::others::Staff>(musx::dom::SCORE_PARTID).empty()) {
            throw std::runtime_error("MUS import did not produce score content and options");
        }
        try {
            (void)reader.read(std::span<const std::uint8_t>{});
        } catch (const std::invalid_argument&) {
            return 0;
        }
        throw std::runtime_error("Empty input was not rejected");
    } catch (const std::exception& error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
