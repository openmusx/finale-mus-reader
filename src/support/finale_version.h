// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <compare>
#include <cstdint>

namespace finale_mus_reader {

/// @brief A Finale version boundary, ordered by the packed source version components.
/// @details Minor versions participate because the Finale 3.x line and Finale 97 share
/// internal major version 3. Maintenance, development status, and build distinguish
/// capabilities introduced within a point release.
struct VersionBound
{
    constexpr VersionBound(std::uint8_t sourceMajor)
        : major(sourceMajor)
    {}
    constexpr VersionBound(std::uint8_t sourceMajor, std::uint8_t sourceMinor)
        : major(sourceMajor), minor(sourceMinor)
    {}
    constexpr VersionBound(std::uint8_t sourceMajor, std::uint8_t sourceMinor, std::uint8_t sourceMaint)
        : major(sourceMajor), minor(sourceMinor), maint(sourceMaint)
    {}
    constexpr VersionBound(
        std::uint8_t sourceMajor, std::uint8_t sourceMinor, std::uint8_t sourceMaint, std::uint8_t sourceDevStatus, std::uint8_t sourceBuild)
        : major(sourceMajor), minor(sourceMinor), maint(sourceMaint), devStatus(sourceDevStatus), build(sourceBuild)
    {}

    std::uint8_t major;
    std::uint8_t minor{};
    std::uint8_t maint{};
    std::uint8_t devStatus{};
    std::uint8_t build{};

    constexpr auto operator<=>(const VersionBound&) const = default;
};

namespace versions {
inline constexpr SourceVersion finale3_3_v1_source{.major = 3, .minor = 3, .build = 1};
inline constexpr SourceVersion finale3_3_v2_source{.major = 3, .minor = 3, .build = 2};
inline constexpr VersionBound finale1_0{1, 0};
inline constexpr VersionBound finale2_6{2, 6};
inline constexpr VersionBound finale3_0{3, 0};
inline constexpr VersionBound finale3_2{3, 2};
inline constexpr VersionBound finale3_3_v2{3, 3, 0, 0, 2};
inline constexpr VersionBound finale3_5{3, 5};
inline constexpr VersionBound finale3_5_1{3, 5, 1};
inline constexpr VersionBound finale3_7{3, 7};
inline constexpr VersionBound finale97{3, 8};
inline constexpr VersionBound finale98{4};
inline constexpr VersionBound finale2000{5};
inline constexpr VersionBound finale2000_build1{5, 0, 0, 0, 1};
inline constexpr VersionBound finale2001{6};
inline constexpr VersionBound finale2002{7};
inline constexpr VersionBound finale2003{8};
inline constexpr VersionBound finale2004{9};
inline constexpr VersionBound finale2005{10};
inline constexpr VersionBound finale2006{11};
inline constexpr VersionBound finale2007{12};
inline constexpr VersionBound finale2008{13};
inline constexpr VersionBound finale2009{14};
inline constexpr VersionBound finale2010{15};
inline constexpr VersionBound finale2011{16};
inline constexpr VersionBound finale2012{17};
} // namespace versions

} // namespace finale_mus_reader
