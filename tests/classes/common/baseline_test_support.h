// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include "class_test_support.h"

namespace finale_mus_reader_tests {
namespace classes {

inline musx::dom::DocumentPtr baselineReferenceDocument()
{
    auto session = musx::factory::DocumentFactory::begin();
    const auto document = session.getDocument();
    const auto addExpression = [&]<typename T>(musx::dom::Evpu displacement) {
        auto value =
            std::make_shared<T>(document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper{0}, musx::dom::Cmper{0});
        value->baselineDisplacement = displacement;
        document->getDetails()->add(T::XmlNodeName, std::move(value));
    };
    const auto add = [&]<typename T>() {
        for (musx::dom::Inci inci = 0; inci < 10; ++inci) {
            auto value = std::make_shared<T>(
                document, musx::dom::SCORE_PARTID, musx::dom::EnigmaBase::ShareMode::All, musx::dom::Cmper{0}, musx::dom::Cmper{0}, inci);
            value->baselineDisplacement = -144 - 40 * inci;
            value->lyricNumber = static_cast<musx::dom::Cmper>(inci + 1);
            document->getDetails()->add(T::XmlNodeName, std::move(value));
        }
    };
    addExpression.template operator()<musx::dom::details::BaselineExpressionsAbove>(144);
    addExpression.template operator()<musx::dom::details::BaselineExpressionsBelow>(-144);
    add.template operator()<musx::dom::details::BaselineLyricsChorus>();
    add.template operator()<musx::dom::details::BaselineLyricsSection>();
    add.template operator()<musx::dom::details::BaselineLyricsVerse>();
    return std::move(session).finish();
}

inline ImportReport importBaselines(
    const finale_mus_reader::container::ParsedContainer& parsed, const SourceProfile& profile, const musx::dom::DocumentPtr& document)
{
    ImportReport report(profile.epoch);
    const auto index = LegacyRecordIndex::build(parsed);
    const auto reference = baselineReferenceDocument();
    finale_mus_reader::PendingReferences pending;
    musx::factory::ConstructionContext construction;
    const finale_mus_reader::ImportContext context{index, profile, noSource, document, reference, report, pending, construction};
    finale_mus_reader::details::importBaselines(context);
    finale_mus_reader::runDeferredChecks(pending);
    return report;
}

} // namespace classes
} // namespace finale_mus_reader_tests
