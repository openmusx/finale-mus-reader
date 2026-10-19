// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/header.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>

#include "container/product_banner.h"

namespace finale_mus_reader {
namespace header {
namespace {

constexpr std::size_t headerSize = 0x200;
constexpr std::string_view bannerSignature = "ENIGMA BINARY FILE";

std::string fixedString(const std::uint8_t* data, std::size_t size)
{
    const auto* end = std::find(data, data + size, std::uint8_t{0});
    return std::string(reinterpret_cast<const char*>(data), static_cast<std::size_t>(end - data));
}

SourcePlatform parsePlatform(const std::string& platform)
{
    if (platform == "MAC") {
        return SourcePlatform::MacOS;
    }
    if (platform == "WIN") {
        return SourcePlatform::Windows;
    }
    return SourcePlatform::Unknown;
}

musx::dom::header::Platform toDomPlatform(SourcePlatform platform)
{
    switch (platform) {
    case SourcePlatform::MacOS: return musx::dom::header::Platform::Mac;
    case SourcePlatform::Windows: return musx::dom::header::Platform::Windows;
    case SourcePlatform::Unknown: return musx::dom::header::Platform::Other;
    }
    return musx::dom::header::Platform::Other;
}

SourceVersion decodeVersionValue(std::uint32_t value)
{
    SourceVersion version;
    version.raw = value;
    version.major = static_cast<std::uint8_t>(value >> 24U);
    version.minor = static_cast<std::uint8_t>((value >> 20U) & 0x0fU);
    version.maint = static_cast<std::uint8_t>((value >> 16U) & 0x0fU);
    version.devStatus = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
    version.build = static_cast<std::uint8_t>(value & 0xffU);
    return version;
}

SourceVersion decodeVersion(const std::uint8_t* raw, ByteOrder byteOrder)
{
    const auto bigEndian = (static_cast<std::uint32_t>(raw[0]) << 24U) | (static_cast<std::uint32_t>(raw[1]) << 16U)
                           | (static_cast<std::uint32_t>(raw[2]) << 8U) | raw[3];
    const auto littleEndian = (static_cast<std::uint32_t>(raw[3]) << 24U) | (static_cast<std::uint32_t>(raw[2]) << 16U)
                              | (static_cast<std::uint32_t>(raw[1]) << 8U) | raw[0];

    return decodeVersionValue(byteOrder == ByteOrder::LittleEndian ? littleEndian : bigEndian);
}

void applyVersion(const SourceVersion& decoded, musx::dom::header::FinaleVersion& version)
{
    version.major = decoded.major;
    version.minor = decoded.minor;
    if (decoded.maint != 0) {
        version.maint = decoded.maint;
    }
    if (decoded.build != 0) {
        version.build = decoded.build;
    }
}

void populateVersion(const std::uint8_t* raw, ByteOrder byteOrder, musx::dom::header::FinaleVersion& version)
{
    applyVersion(decodeVersion(raw, byteOrder), version);
}

musx::dom::header::FileInfo parseFileInfo(const std::uint8_t* data, std::size_t dateOffset, std::size_t tupleOffset, ByteOrder byteOrder)
{
    musx::dom::header::FileInfo info;
    const int month = data[dateOffset + 1];
    const int day = data[dateOffset + 2];
    if (month >= 1 && month <= 12 && day >= 1 && day <= 31) {
        info.year = 1900 + data[dateOffset];
        info.month = month;
        info.day = day;
    }
    populateVersion(data + tupleOffset, byteOrder, info.finaleVersion);
    info.application = fixedString(data + tupleOffset + 4, 4);
    info.platform = toDomPlatform(parsePlatform(fixedString(data + tupleOffset + 8, 4)));
    populateVersion(data + tupleOffset + 12, byteOrder, info.appVersion);
    populateVersion(data + tupleOffset + 16, byteOrder, info.fileVersion);
    return info;
}

bool describeCodaBannerIdentity(const std::uint8_t* data, std::size_t size, ImportReport& report)
{
    const auto parsed = banner::parse(data, size);
    if (!parsed.isPreSignature() || parsed.offset != 0) {
        return false;
    }
    report.banner = parsed.text;
    report.savingProduct = parsed.product;
    report.sourceVersion = banner::versionFromProduct(parsed.product);
    // This era has no platform field at 0x074, so the banner product is the only place it
    // can say. A `PC` product states Windows; anything else states nothing and stays
    // Unknown. Believed: the era had only two platforms, which would make every other product
    // a Mac one, but a Windows release writing a numeric product would break the inference
    // silently. Unknown already falls back to the macOS baseline, so claiming Mac would buy
    // nothing and risk being wrong.
    //
    // Recovering the Windows case matters beyond diagnostics: the platform selects which
    // pinned Finale 27 baseline seeds the options pool, so without this a Windows document
    // would be completed from macOS defaults.
    if (parsed.hasPcProduct()) {
        report.sourcePlatform = SourcePlatform::Windows;
    }
    return true;
}

// The order the header's 32-bit fields are stored in: the container's, or when a standalone
// caller has not classified the file, the order the header itself states. A version tuple read
// in the wrong order frequently still lands inside Finale's version range, so the order is never
// guessed from the values.
ByteOrder headerByteOrder(const std::uint8_t* data, std::size_t size, const ImportReport& report)
{
    if (report.byteOrder != ByteOrder::Unknown) {
        return report.byteOrder;
    }
    return container::statedByteOrder(data, size).value_or(ByteOrder::Unknown);
}

} // namespace

bool hasBanner(const std::uint8_t* data, std::size_t size)
{
    return size > bannerSignature.size() && std::memcmp(data, bannerSignature.data(), bannerSignature.size()) == 0
           && data[bannerSignature.size()] == 0;
}

void describeSourceIdentity(const std::uint8_t* data, std::size_t size, ImportReport& report)
{
    if (describeCodaBannerIdentity(data, size, report)) {
        return;
    }
    if (!hasBanner(data, size) || size < headerSize) {
        return;
    }
    report.byteOrder = headerByteOrder(data, size, report);
    const auto parsed = banner::parse(data, size);
    report.banner = parsed.text;
    report.savingProduct = parsed.product;
    const auto modifiedPlatform = parsePlatform(fixedString(data + 0x09a, 4));
    const auto createdPlatform = parsePlatform(fixedString(data + 0x074, 4));
    report.sourcePlatform = modifiedPlatform != SourcePlatform::Unknown ? modifiedPlatform : createdPlatform;

    const auto modified = decodeVersion(data + 0x092, report.byteOrder);
    const auto created = decodeVersion(data + 0x06c, report.byteOrder);
    const auto& selected = modified.major != 0 ? modified : created;
    if (created.major != 0 && created.major <= maximumFinaleMajorVersion) {
        report.creatorVersion = created;
    }
    if (selected.major <= maximumFinaleMajorVersion) {
        report.sourceVersion = selected;
    } else {
        report.diagnostics.push_back({musx::util::Logger::LogLevel::Warning,
            "Recovered Finale major version " + std::to_string(selected.major) + " is outside the valid range 0-"
                + std::to_string(maximumFinaleMajorVersion) + "; version-gated mappings are skipped."});
    }
}

musx::dom::header::HeaderPtr recover(const std::uint8_t* data, std::size_t size, const ImportReport& report)
{
    auto result = std::make_shared<musx::dom::header::Header>();
    const auto byteOrder = headerByteOrder(data, size, report);
    result->wordOrder = byteOrder == ByteOrder::BigEndian ? musx::dom::header::WordOrder::BigEndian : musx::dom::header::WordOrder::LittleEndian;
    if (report.sourcePlatform == SourcePlatform::MacOS) {
        result->textEncoding = musx::dom::header::TextEncoding::Mac;
    } else if (report.sourcePlatform == SourcePlatform::Windows) {
        result->textEncoding = musx::dom::header::TextEncoding::Windows;
    }

    if (hasBanner(data, size) && size >= 0x0a6) {
        result->created = parseFileInfo(data, 0x066, 0x06c, byteOrder);
        result->modified = parseFileInfo(data, 0x08c, 0x092, byteOrder);
    } else if (report.formatEpoch == FormatEpoch::CodaBanner && report.sourceVersion) {
        applyVersion(*report.sourceVersion, result->modified.finaleVersion);
    }
    return result;
}

} // namespace header
} // namespace finale_mus_reader
