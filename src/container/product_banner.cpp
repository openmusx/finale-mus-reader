// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "container/product_banner.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <limits>
#include <string_view>

namespace finale_mus_reader {
namespace banner {

namespace {

/// @brief How far into a file a banner may begin.
/// @details The two pre-signature spellings sit at offset 0 and the registered spelling at
/// 0x20. The window is wider than either so a variant placed slightly differently is still
/// found, and narrow enough that `Finale` occurring in document text cannot be mistaken for
/// a banner.
constexpr std::size_t searchWindow = 0x40;

/// @brief Where the product ends. The first match wins, so order is not significant.
constexpr std::array<std::string_view, 3> productTerminators = {
    " Copyright",
    " File Converter",
    // Finale 1.0.0 puts this where every later era puts a copyright notice.
    //
    // DO NOT CORRECT THE SPELLING. `ENIGA` is Coda's own typo, present in the file, and
    // this string has to match those bytes exactly. Finale 1.0.0 documents carry
    // `45 4e 49 47 41` — E N I G A, with no M — and never spell it `ENIGMA`. Repairing the
    // typo here would stop the terminator matching, and the
    // product would silently come back as `1.0.0 ENIGA Structures` instead of `1.0.0`.
    " ENIGA Structures",
};

constexpr std::uint8_t macTrademarkSign = 0xAA;

struct SpellingPattern
{
    Spelling spelling;
    std::string_view prefix;
};

// `Finale\xAA` is matched as `Finale` plus the byte, since a string literal cannot hold a
// high byte portably. The two parenthesized spellings are plain text.
constexpr std::array<SpellingPattern, 2> textPatterns = {{
    {Spelling::Registered, "Finale(R) "},
    {Spelling::Trademark, "Finale(TM) "},
}};

constexpr std::string_view macTrademarkStem = "Finale";

/// @brief Reads a NUL-terminated string, bounded by the available bytes.
std::string boundedString(const std::uint8_t* data, std::size_t size)
{
    const auto* end = std::find(data, data + size, std::uint8_t{0});
    return std::string(reinterpret_cast<const char*>(data), static_cast<std::size_t>(end - data));
}

/// @brief Splits the product out of a banner whose spelling has already been matched.
std::string productFrom(const std::string& text, std::size_t prefixLength)
{
    if (prefixLength >= text.size()) {
        return {};
    }
    const auto rest = text.substr(prefixLength);
    auto productEnd = rest.size();
    for (const auto terminator : productTerminators) {
        if (const auto found = rest.find(terminator); found != std::string::npos) {
            productEnd = (std::min)(productEnd, found);
        }
    }
    return rest.substr(0, productEnd);
}

} // namespace

ProductBanner parse(const std::uint8_t* data, std::size_t size)
{
    if (data == nullptr) {
        return {};
    }
    const auto window = (std::min)(size, searchWindow);

    for (std::size_t offset = 0; offset < window; ++offset) {
        const auto remaining = size - offset;

        for (const auto& pattern : textPatterns) {
            if (remaining <= pattern.prefix.size() || std::memcmp(data + offset, pattern.prefix.data(), pattern.prefix.size()) != 0) {
                continue;
            }
            ProductBanner result;
            result.spelling = pattern.spelling;
            result.offset = offset;
            result.text = boundedString(data + offset, remaining);
            result.product = productFrom(result.text, pattern.prefix.size());
            return result;
        }

        // `Finale` + 0xAA + a space. Checked separately because the trademark sign is a
        // high byte rather than text.
        const std::size_t macPrefixLength = macTrademarkStem.size() + 2;
        if (remaining > macPrefixLength && std::memcmp(data + offset, macTrademarkStem.data(), macTrademarkStem.size()) == 0
            && data[offset + macTrademarkStem.size()] == macTrademarkSign && data[offset + macTrademarkStem.size() + 1] == ' ') {
            ProductBanner result;
            result.spelling = Spelling::MacTrademark;
            result.offset = offset;
            result.text = boundedString(data + offset, remaining);
            result.product = productFrom(result.text, macPrefixLength);
            return result;
        }
    }
    return {};
}

std::optional<SourceVersion> versionFromProduct(const std::string& product)
{
    SourceVersion version;
    std::size_t consumed = 0;
    std::uint8_t* const components[] = {&version.major, &version.minor, &version.maint};
    for (auto* component : components) {
        std::size_t digits = 0;
        unsigned value = 0;
        while (consumed + digits < product.size() && std::isdigit(static_cast<unsigned char>(product[consumed + digits]))) {
            value = value * 10 + static_cast<unsigned>(product[consumed + digits] - '0');
            ++digits;
        }
        if (digits == 0 || value > (std::numeric_limits<std::uint8_t>::max)()) {
            break;
        }
        *component = static_cast<std::uint8_t>(value);
        consumed += digits;
        if (consumed >= product.size() || product[consumed] != '.') {
            break;
        }
        ++consumed;
    }
    if (version.major == 0 || version.major > maximumFinaleMajorVersion) {
        return std::nullopt;
    }
    return version;
}

} // namespace banner
} // namespace finale_mus_reader
