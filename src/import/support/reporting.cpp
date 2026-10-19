// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/support/reporting.h"

#if defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)

namespace finale_mus_reader {

std::optional<InstanceKey> ReportWriter::importedInstance(const musx::dom::EnigmaBase& object)
{
    if (const auto* other = dynamic_cast<const musx::dom::OthersBase*>(&object)) {
        return InstanceKey{typeid(object), other->getSourcePartId(), other->getCmper(), other->getInci(), std::nullopt};
    }
    if (const auto* detail = dynamic_cast<const musx::dom::DetailsBase*>(&object)) {
        return InstanceKey{typeid(object), detail->getSourcePartId(), detail->getCmper1(), detail->getInci(), detail->getCmper2()};
    }
    if (const auto* text = dynamic_cast<const musx::dom::TextsBase*>(&object)) {
        return InstanceKey{typeid(object), musx::dom::SCORE_PARTID, text->getTextNumber(), std::nullopt, std::nullopt};
    }
    if (dynamic_cast<const musx::dom::OptionsBase*>(&object)) {
        return InstanceKey{typeid(object), musx::dom::SCORE_PARTID, std::nullopt, std::nullopt, std::nullopt};
    }
    return std::nullopt;
}

std::string ReportWriter::memberName(const char* memberPath)
{
    std::string result;
    for (const char* at = memberPath; *at != '\0'; ++at) {
        if (at[0] == '-' && at[1] == '>') {
            result += '.';
            ++at;
            continue;
        }
        result += *at;
    }
    return result;
}

} // namespace finale_mus_reader

#endif // defined(FINALE_MUS_READER_ENABLE_INSTRUMENTATION)
