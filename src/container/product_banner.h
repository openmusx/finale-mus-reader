// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "finale_mus_reader/reader.h"

namespace finale_mus_reader {

/// @brief The highest major version Finale ever shipped.
/// @details Finale's history spans major versions 0 through 27. A recovered major outside
/// that range means the bytes were not the version they were taken for, whichever route
/// recovered them, so both the header tuple and the product banner gate on this.
inline constexpr std::uint8_t maximumFinaleMajorVersion = 27;

namespace banner {

/// @brief How a file spells its product banner.
/// @details Three spellings exist and every one of them has to be recognized, because a
/// file whose spelling is unmatched reports no product at all rather than reporting a
/// wrong one. Missing the third is what kept 22 Finale 1.0.0 documents unreadable.
enum class Spelling {
    None,
    /// @brief `Finale(R) 2003 Copyright (c) ...`, at 0x20 in signature-bearing files.
    Registered,
    /// @brief `Finale(TM) 2.6 Copyright 1987 by Coda.`, at offset 0 before the signature
    /// existed.
    Trademark,
    /// @brief `Finale<0xAA> 1.0.0 ENIGA Structures Copyright 1987 by Coda.`, Finale 1.0.0
    /// only. 0xAA is the MacRoman trademark sign, and the version is terminated by
    /// `ENIGA Structures` rather than by a copyright notice.
    ///
    /// `ENIGA` is Coda's typo, not this project's. It is what the files contain, so it is
    /// what the parser matches, and it must be preserved wherever it is written down.
    MacTrademark,
};

/// @brief A product banner located in a file's header.
struct ProductBanner
{
    Spelling spelling = Spelling::None;
    /// @brief Offset of the `Finale` text within the file.
    std::size_t offset{};
    /// @brief The banner text, from @ref offset to the first NUL.
    std::string text;
    /// @brief The product, between the spelling and its terminator: `2.6`, `1.0.0`, `97`.
    std::string product;

    [[nodiscard]] explicit operator bool() const { return spelling != Spelling::None; }

    /// @brief Whether this is one of the two pre-signature spellings.
    [[nodiscard]] bool isPreSignature() const { return spelling == Spelling::Trademark || spelling == Spelling::MacTrademark; }

    /// @brief Whether the product's leading token is `PC`, which names the platform.
    /// @details The pre-signature era states its platform here and nowhere else: its
    /// Windows documents carry `PC 1.0+` where its Mac documents carry a bare version such
    /// as `2.6`. No other product string contains `PC`.
    ///
    /// Only the token is matched. What follows is a version and must not participate:
    /// Believed: `1.0+` is the only version used here, but the platform is stated separately
    /// from the version on purpose. Matching the whole string would reject a Windows document
    /// from any other release. That version does not parse either, so such a file carries
    /// no recovered version and every version-gated mapping skips it.
    [[nodiscard]] bool hasPcProduct() const { return product.rfind("PC", 0) == 0 && (product.size() == 2 || product[2] == ' '); }
};

/// @brief Finds and parses the product banner in a header.
/// @details This is the single place any spelling of the banner is recognized. Every caller
/// goes through it, so adding a spelling cannot leave one site behind — which is exactly
/// how the three literals this replaced came to disagree.
/// @param data Start of the file.
/// @param size Bytes available.
[[nodiscard]] ProductBanner parse(const std::uint8_t* data, std::size_t size);

/// @brief Recovers a version from a dotted numeric product such as `2.6` or `1.0.0`.
/// @details Only the pre-signature eras need this. They carry no version tuple — their
/// whole 0x60-0x200 region is zero apart from a constant word at 0x80 — so the number in
/// the banner is the only place a version can come from. Returns nothing when the product
/// is not numeric or the major version is outside Finale's range.
[[nodiscard]] std::optional<SourceVersion> versionFromProduct(const std::string& product);

} // namespace banner
} // namespace finale_mus_reader
