// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/support/embedded_graphics.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "musx/util/Logger.h"

namespace finale_mus_reader {
namespace {

constexpr std::uint16_t zlibGraphicBlockType = 0x0013;
constexpr std::size_t graphicHeaderSize = 6;
constexpr std::size_t graphicFooterSize = 5;

std::uint32_t readGraphicLong(std::span<const std::uint8_t> bytes, std::size_t at, ByteOrder byteOrder)
{
    if (byteOrder == ByteOrder::BigEndian) {
        return (static_cast<std::uint32_t>(bytes[at]) << 24U) | (static_cast<std::uint32_t>(bytes[at + 1]) << 16U)
               | (static_cast<std::uint32_t>(bytes[at + 2]) << 8U) | bytes[at + 3];
    }
    return static_cast<std::uint32_t>(bytes[at]) | (static_cast<std::uint32_t>(bytes[at + 1]) << 8U)
           | (static_cast<std::uint32_t>(bytes[at + 2]) << 16U) | (static_cast<std::uint32_t>(bytes[at + 3]) << 24U);
}

std::optional<std::string> graphicExtension(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() >= 8 && bytes[0] == 0x89 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G') {
        return "png";
    }
    if (bytes.size() >= 4 && bytes[0] == 0xc5 && bytes[1] == 0xd0 && bytes[2] == 0xd3 && bytes[3] == 0xc6) {
        return "eps";
    }
    if (bytes.size() >= 3 && bytes[0] == 0xff && bytes[1] == 0xd8 && bytes[2] == 0xff) {
        return "jpg";
    }
    if (bytes.size() >= 6 && bytes[0] == 'G' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == '8' && (bytes[4] == '7' || bytes[4] == '9')
        && bytes[5] == 'a') {
        return "gif";
    }
    constexpr std::string_view pdfPrefix = "%PDF-";
    if (bytes.size() >= pdfPrefix.size() && std::equal(pdfPrefix.begin(), pdfPrefix.end(), bytes.begin())) {
        return "pdf";
    }
    if (bytes.size() >= 4
        && ((bytes[0] == 'I' && bytes[1] == 'I' && bytes[2] == 0x2a && bytes[3] == 0)
            || (bytes[0] == 'M' && bytes[1] == 'M' && bytes[2] == 0 && bytes[3] == 0x2a))) {
        return "tif";
    }
    constexpr std::string_view epsPrefix = "%!PS-Adobe";
    if (bytes.size() >= epsPrefix.size() && std::equal(epsPrefix.begin(), epsPrefix.end(), bytes.begin())) {
        return "eps";
    }
    return std::nullopt;
}

} // namespace

musx::dom::EmbeddedGraphicsMap recoverEmbeddedGraphics(const container::ParsedContainer& parsed, ImportReport& report)
{
    musx::dom::EmbeddedGraphicsMap result;
    // Finale 2006 introduced embedding while still using DCL blocks. Its stored 0x0013
    // block has the same nested item framing as the zlib epoch.
    if (parsed.formatEpoch != FormatEpoch::DclLegacy && parsed.formatEpoch != FormatEpoch::ZlibLegacy) {
        return result;
    }
    musx::dom::Cmper nextCmper = 1;

    for (const auto& block : parsed.blocks) {
        if (!block.info.stored || block.info.type != zlibGraphicBlockType || block.data.empty()) {
            continue;
        }
        const std::span<const std::uint8_t> bytes(block.data);
        std::size_t at = 0;
        while (at < bytes.size()) {
            if (bytes.size() - at < graphicHeaderSize) {
                report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning, "An embedded-graphics block ends inside an item header."});
                break;
            }
            const auto payloadSize = static_cast<std::size_t>(readGraphicLong(bytes, at + 2, parsed.byteOrder));
            if (payloadSize > bytes.size() - at - graphicHeaderSize || bytes.size() - at - graphicHeaderSize - payloadSize < graphicFooterSize) {
                report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning, "An embedded-graphics item has a truncated payload or footer."});
                break;
            }
            const auto payloadAt = at + graphicHeaderSize;
            const auto payload = bytes.subspan(payloadAt, payloadSize);
            const auto footerAt = payloadAt + payloadSize;
            if (readGraphicLong(bytes, footerAt, parsed.byteOrder) != 1) {
                report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                    "Embedded graphic " + std::to_string(nextCmper) + " has an unsupported footer version and was skipped."});
            } else if (const auto extension = graphicExtension(payload)) {
                result.emplace(nextCmper, musx::dom::EmbeddedGraphicData{*extension, {payload.begin(), payload.end()}});
            } else {
                report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                    "Embedded graphic " + std::to_string(nextCmper) + " has an unrecognized file signature and was skipped."});
            }
            // The final footer byte varies with the graphic kind or save operation. It is
            // not needed to delimit, identify, or reproduce the file bytes: a block is
            // consumed exactly by the stated length plus this five-byte footer.
            at = footerAt + graphicFooterSize;
            ++nextCmper;
        }
    }
    if (!result.empty()) {
        report.diagnostics.push_back(
            {musx::util::Logger::LogLevel::Verbose, "Imported " + std::to_string(result.size()) + " embedded graphic file(s)."});
    }
    return result;
}

} // namespace finale_mus_reader
