// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "defaults/default_document.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

#include <zlib.h>

#include "embedded_default.h"
#include "musx/dom/Options.h"
#include "musx/dom/Others.h"

namespace finale_mus_reader {
namespace defaults {
namespace {

constexpr std::size_t maximumDefaultXmlSize = 1024U * 1024U;

// One pinned platform baseline: its embedded gzip bytes and the exact size the
// inflated EnigmaXML must have.
struct EmbeddedBaseline
{
    const std::uint8_t* gzip;
    std::size_t gzipSize;
    std::size_t expectedXmlSize;
    const char* name;
};

constexpr EmbeddedBaseline macOSBaseline{generated::macosDefaultGzip, 0, 91059, "macOS"};
constexpr EmbeddedBaseline windowsBaseline{generated::windowsDefaultGzip, 0, 86844, "Windows"};

EmbeddedBaseline selectBaseline(SourcePlatform platform)
{
    // A source-platform match is preferred. An unknown platform falls back to macOS
    // because that is the baseline the reader was first validated against.
    auto baseline = platform == SourcePlatform::Windows ? windowsBaseline : macOSBaseline;
    if (platform == SourcePlatform::Windows) {
        baseline.gzipSize = generated::windowsDefaultGzipSize;
    } else {
        baseline.gzipSize = generated::macosDefaultGzipSize;
    }
    return baseline;
}

std::string inflateDefault(const EmbeddedBaseline& baseline)
{
    if (baseline.gzipSize > UINT_MAX) {
        throw std::runtime_error("Embedded default gzip exceeds zlib's input limit");
    }

    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(baseline.gzip));
    stream.avail_in = static_cast<uInt>(baseline.gzipSize);
    if (inflateInit2(&stream, 15 + 16) != Z_OK) {
        throw std::runtime_error("Unable to initialize gzip inflation for the embedded default");
    }

    std::string output;
    std::array<char, 16384> chunk{};
    int result = Z_OK;
    while (result == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef*>(chunk.data());
        stream.avail_out = static_cast<uInt>(chunk.size());
        result = inflate(&stream, Z_NO_FLUSH);
        const auto produced = chunk.size() - stream.avail_out;
        if (produced > maximumDefaultXmlSize - output.size()) {
            inflateEnd(&stream);
            throw std::runtime_error("Embedded default expands beyond the configured limit");
        }
        output.append(chunk.data(), produced);
    }

    const bool valid = result == Z_STREAM_END && stream.avail_in == 0;
    inflateEnd(&stream);
    if (!valid || output.size() != baseline.expectedXmlSize) {
        throw std::runtime_error(std::string("Embedded Finale 27 ") + baseline.name + " default failed integrity validation");
    }
    return output;
}

const std::string& defaultXml(SourcePlatform platform)
{
    static const std::string macOS = inflateDefault(selectBaseline(SourcePlatform::MacOS));
    static const std::string windows = inflateDefault(selectBaseline(SourcePlatform::Windows));
    return platform == SourcePlatform::Windows ? windows : macOS;
}

musx::xml::XmlElementPtr requireChild(const musx::xml::XmlElementPtr& parent, const std::string& nodeName)
{
    auto child = parent->getFirstChildElement(nodeName);
    if (!child) {
        throw std::runtime_error("Embedded default is missing <" + nodeName + ">");
    }
    return child;
}

std::size_t countAccepted(const musx::xml::XmlElementPtr& parent, const musx::factory::NodeFilter& filter)
{
    std::size_t result = 0;
    for (auto child = parent->getFirstChildElement(); child; child = child->getNextSibling()) {
        if (filter(child)) {
            ++result;
        }
    }
    return result;
}

} // namespace

ParsedDefaultDocument parseDefault(XmlParser parseXml, DocumentParser parseDocument, SourcePlatform platform)
{
    // The baseline seeds exactly the modern layer range; musxdom names its size.
    constexpr auto expectedLayerAttributes = static_cast<std::size_t>(musx::dom::MAX_LAYERS);

    ParsedDefaultDocument result;
    result.platform = platform == SourcePlatform::Windows ? SourcePlatform::Windows : SourcePlatform::MacOS;
    const auto& xml = defaultXml(result.platform);
    result.xmlDocument = parseXml(xml.data(), xml.size());
    result.referenceDocument = parseDocument(xml.data(), xml.size());
    if (!result.referenceDocument) {
        throw std::runtime_error("Embedded default did not produce a reference document");
    }
    if (!result.referenceDocument->getOptions() || !result.referenceDocument->getOptions()->get<musx::dom::options::FontOptions>()) {
        throw std::runtime_error("Embedded default reference is missing FontOptions");
    }
    if (!result.referenceDocument->getOthers()
        || result.referenceDocument->getOthers()->getArray<musx::dom::others::FontDefinition>(musx::dom::SCORE_PARTID).empty()) {
        throw std::runtime_error("Embedded default reference is missing font definitions");
    }
    const auto root = result.xmlDocument ? result.xmlDocument->getRootElement() : nullptr;
    if (!root || root->getTagName() != "finale") {
        throw std::runtime_error("Embedded default is missing its <finale> element");
    }

    result.options = requireChild(root, "options");
    result.others = requireChild(root, "others");
    // Most baseline options are structurally safe defaults. Two are not, for the same
    // reason: they carry comparators into baseline tables that are deliberately not
    // imported. FontOptions holds font-definition ids throughout, and ClefOptions holds a
    // shape id and an optional font id on every clef definition. Both are rebuilt from the
    // source instead, and completed from the separately owned reference document.
    result.optionsFilter = [](const musx::xml::XmlElementPtr& node) {
        const auto name = node->getTagName();
        return name != musx::dom::options::FontOptions::XmlNodeName && name != musx::dom::options::ClefOptions::XmlNodeName;
    };
    // This allowlist is what keeps the fallback measures, staves, entries, text, parts,
    // and layouts of the baseline out of an imported document.
    result.optionLikeOthersFilter = [](const musx::xml::XmlElementPtr& node) {
        return node->getTagName() == musx::dom::others::LayerAttributes::XmlNodeName;
    };
    if (countAccepted(result.others, result.optionLikeOthersFilter) != expectedLayerAttributes) {
        throw std::runtime_error("Embedded default must contain exactly " + std::to_string(expectedLayerAttributes) + " layer attributes");
    }
    return result;
}

} // namespace defaults
} // namespace finale_mus_reader
