// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <optional>

#include "finale_mus_reader/reader.h"
#include "import/shared/coda_expression_shape.h"
#include "musx/musx.h"
#include "records/legacy_record_index.h"

/// @brief SmartShapes stored before the fixed shape record.
/// @details sX names the end measure and the shape type, eX the start measure, DY rows the
/// endpoints and centers, oX the endpoint and break offsets, and aX the curvature. A center can
/// also have bX break offsets and dX curvature details, keyed by shape and center row.
namespace finale_mus_reader::early_smart_shape {

inline constexpr records::LegacyTag startTag = records::packTag("eX");
inline constexpr records::LegacyTag endTag = records::packTag("sX");
inline constexpr records::LegacyTag endpointTag = records::packTag("DY");
inline constexpr records::LegacyTag offsetTag = records::packTag("oX");
inline constexpr records::LegacyTag curveTag = records::packTag("aX");
inline constexpr records::LegacyTag centerBreakTag = records::packTag("bX");
inline constexpr records::LegacyTag centerCurveTag = records::packTag("dX");

// Word 5 of an endpoint or center row. Its low byte is the staff; the high bits mark the row as
// part of a shape and say which part.
inline constexpr std::uint16_t kindMask = 0xe000;
inline constexpr std::uint16_t startKind = 0x8000;
inline constexpr std::uint16_t endKind = 0xc000;
inline constexpr std::uint16_t centerKind = 0xa000;
inline constexpr std::uint16_t codaStartMarker = 0x8000;
inline constexpr std::uint16_t codaCenterMarker = 0x1000;

inline std::uint16_t flags(const records::LegacyRow& row)
{
    return static_cast<std::uint16_t>(row.words[5]);
}

inline std::uint16_t staff(const records::LegacyRow& row)
{
    return flags(row) & 0x00ff;
}

/// @brief The records that define one early shape.
struct Shape
{
    const records::LegacyRow* start{};         ///< The eX row.
    const records::LegacyRow* end{};           ///< The sX row.
    const records::LegacyRow* startEndpoint{}; ///< The start DY row.
    const records::LegacyRow* endEndpoint{};   ///< The end DY row.

    [[nodiscard]] std::uint16_t startMeasure() const { return static_cast<std::uint16_t>(start->words[0]); }
    [[nodiscard]] std::uint16_t endMeasure() const { return static_cast<std::uint16_t>(end->words[0]); }
};

/// @brief Returns the first row in a measure that belongs to a shape with the given kind.
inline const records::LegacyRow* findRow(const records::LegacyRowPool& pool, std::uint16_t shapeId, std::uint16_t measure, std::uint16_t kind)
{
    for (const auto& row : pool.getArray(endpointTag, measure)) {
        if (static_cast<std::uint16_t>(row.words[0]) == shapeId && (flags(row) & kindMask) == kind) {
            return &row;
        }
    }
    return nullptr;
}

/// @brief Returns a shape's endpoint rows, or nothing when the shape is incomplete.
/// @details A shape needs a nonzero end measure no earlier than its start measure. From Finale
/// 3.0, each endpoint is the first row of its kind in its marker's measure. In a Coda-banner
/// document, smartShapeEndpointMask marks a shape row and codaStartMarker places it in the start
/// measure; the first start row with an end row on the same staff forms the pair, and other
/// marked rows are discarded. A center row chosen as that end leaves the shape without one.
inline std::optional<Shape> find(const records::LegacyRowPool& pool, FormatEpoch epoch, std::uint16_t shapeId)
{
    Shape shape{pool.get(startTag, shapeId, 0, 0), pool.get(endTag, shapeId, 0, 0)};
    if (shapeId == 0 || !shape.start || !shape.end || shape.endMeasure() == 0 || shape.start->words[0] > shape.end->words[0]) {
        return std::nullopt;
    }
    if (epoch != FormatEpoch::CodaBanner) {
        shape.startEndpoint = findRow(pool, shapeId, shape.startMeasure(), startKind);
        shape.endEndpoint = findRow(pool, shapeId, shape.endMeasure(), endKind);
        if (!shape.startEndpoint || !shape.endEndpoint) {
            return std::nullopt;
        }
        return shape;
    }
    const auto marked = [&](const records::LegacyRow& row, bool start) {
        return static_cast<std::uint16_t>(row.words[0]) == shapeId && (flags(row) & coda_expression_shape::smartShapeEndpointMask)
               && ((flags(row) & codaStartMarker) != 0) == start;
    };
    const auto ends = pool.getArray(endpointTag, shape.endMeasure());
    for (const auto& start : pool.getArray(endpointTag, shape.startMeasure())) {
        if (!marked(start, true)) {
            continue;
        }
        for (const auto& end : ends) {
            if (marked(end, false) && staff(end) == staff(start)) {
                if (flags(end) & codaCenterMarker) {
                    return std::nullopt;
                }
                shape.startEndpoint = &start;
                shape.endEndpoint = &end;
                return shape;
            }
        }
    }
    return std::nullopt;
}

/// @brief Returns a measure's center row for a shape. Coda-banner documents keep none.
inline const records::LegacyRow* findCenter(const records::LegacyRowPool& pool, FormatEpoch epoch, std::uint16_t shapeId, std::uint16_t measure)
{
    return epoch == FormatEpoch::CodaBanner ? nullptr : findRow(pool, shapeId, measure, centerKind);
}

/// @brief Converts stored curvature to control points.
/// @details A slur stores two horizontal insets and a height, and a hairpin stores its opening.
/// Other shape types have no control points, and the adjustment stays inactive and zero.
inline void convertControlPoints(musx::dom::smartshape::ControlPointAdjustment& adjustment, musx::dom::others::SmartShape::ShapeType type,
    std::int16_t left, std::int16_t right, std::int16_t height)
{
    using ShapeType = musx::dom::others::SmartShape::ShapeType;
    const auto scaled = [](int value) {
        const double result = value * (64.0 / 9.0);
        return static_cast<int>(result + (result >= 0 ? 0.5 : -0.5));
    };
    switch (type) {
    case ShapeType::Decrescendo:
    case ShapeType::Crescendo:
        adjustment.active = true;
        adjustment.startCtlPtY = static_cast<std::int16_t>(height * 2 + 24);
        return;
    case ShapeType::SlurDown:
    case ShapeType::SlurUp:
    case ShapeType::DashSlurDown:
    case ShapeType::DashSlurUp: {
        const bool down = type == ShapeType::SlurDown || type == ShapeType::DashSlurDown;
        const auto y = static_cast<std::int16_t>(down ? -24 - height : height + 24);
        adjustment.active = true;
        adjustment.startCtlPtX = static_cast<std::int16_t>(scaled(left + right + 24));
        adjustment.startCtlPtY = y;
        adjustment.endCtlPtX = static_cast<std::int16_t>(-scaled(right - left + 24));
        adjustment.endCtlPtY = y;
        return;
    }
    default: return;
    }
}

} // namespace finale_mus_reader::early_smart_shape
