// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "import/others.h"
#include "import/shared/enclosure.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "import/support/text_encoding.h"
#include "musx/musx.h"

namespace finale_mus_reader {
namespace others {
namespace {

using Region = musx::dom::others::MeasureNumberRegion;
using Data = Region::ScorePartData;
using Font = musx::dom::FontInfo;
using Enclosure = musx::dom::others::Enclosure;

constexpr auto regionTag = records::packTag("MN");
constexpr records::LegacyTag regionClass = 0x00a4;

struct RegionBytes
{
    std::span<const std::uint8_t> bytes;
    ByteOrder order;

    std::uint16_t word(std::size_t offset) const { return payloadWord(bytes, offset, order); }
    int signedWord(std::size_t offset) const { return static_cast<std::int16_t>(word(offset)); }
    std::uint32_t longValue(std::size_t offset) const
    {
        if (order == ByteOrder::BigEndian) {
            return (std::uint32_t(word(offset)) << 16) | word(offset + 2);
        }
        return std::uint32_t(word(offset)) | (std::uint32_t(word(offset + 2)) << 16);
    }
    std::string bytesText(std::size_t offset, std::size_t length) const
    {
        std::string value(reinterpret_cast<const char*>(bytes.data() + offset), length);
        if (const auto terminator = value.find('\0'); terminator != std::string::npos) {
            value.resize(terminator);
        }
        return value;
    }
};

std::shared_ptr<Font> readFont(const ImportContext& context, const RegionBytes& source, std::size_t offset)
{
    auto result = std::make_shared<Font>(context.document);
    result->fontId = source.word(offset);
    result->fontSize = source.signedWord(offset + 2);
    result->setEnigmaStyles(source.word(offset + 4));
    return result;
}

std::shared_ptr<Enclosure> readEnclosure(const ImportContext& context, const RegionBytes& source, std::size_t offset)
{
    auto result = std::make_shared<Enclosure>(context.document);
    std::array<std::uint16_t, 6> words;
    for (std::size_t slot = 0; slot < words.size(); ++slot) {
        words[slot] = source.word(offset + slot * 2);
    }
    decodeLegacyEnclosure(*result, words);
    return result;
}

std::shared_ptr<Enclosure> makeCodaEnclosure(const ImportContext& context, unsigned shape)
{
    if (shape == 0 || shape > static_cast<unsigned>(Enclosure::Shape::Octogon)) {
        return {};
    }
    auto result = std::make_shared<Enclosure>(context.document);
    result->shape = static_cast<Enclosure::Shape>(shape);
    result->xMargin = 9;
    result->yMargin = 9;
    result->lineWidth = 224;
    result->notTall = true;
    return result;
}

constexpr musx::dom::AlignJustify measureNumberAlignment(unsigned value)
{
    return value <= static_cast<unsigned>(musx::dom::AlignJustify::Center) ? static_cast<musx::dom::AlignJustify>(value)
                                                                           : musx::dom::AlignJustify::Left;
}

void readModernData(const ImportContext& context, const RegionBytes& source, std::size_t offset, bool unicode, const std::shared_ptr<Data>& data)
{
    data->startFont = readFont(context, source, offset);
    data->multipleFont = readFont(context, source, offset + 6);
    data->mmRestFont = readFont(context, source, offset + 12);
    data->startEnclosure = readEnclosure(context, source, offset + 18);
    data->multipleEnclosure = readEnclosure(context, source, offset + 30);
    data->startXdisp = source.signedWord(offset + 42);
    data->startYdisp = source.signedWord(offset + 44);
    data->multipleXdisp = source.signedWord(offset + 46);
    data->multipleYdisp = source.signedWord(offset + 48);
    data->mmRestXdisp = source.signedWord(offset + 50);
    data->mmRestYdisp = source.signedWord(offset + 52);
    data->leftMmBracketChar = unicode ? source.longValue(offset + 54) : source.word(offset + 54);
    data->rightMmBracketChar = unicode ? source.longValue(offset + 58) : source.word(offset + 56);
    const auto tail = offset + (unicode ? 62 : 58);
    data->startWith = source.signedWord(tail);
    data->incidence = source.signedWord(tail + 2);
    const auto flags = source.word(tail + 4);
    const auto flags2 = source.word(tail + 6);
    data->startAlign = measureNumberAlignment(flags & 3U);
    data->multipleAlign = measureNumberAlignment((flags >> 2) & 3U);
    data->mmRestAlign = measureNumberAlignment((flags >> 4) & 3U);
    data->showOnStart = (flags & 0x0040U) != 0;
    data->showOnEvery = (flags & 0x0080U) != 0;
    data->hideFirstMeasure = (flags & 0x0100U) != 0;
    data->showMmRange = (flags & 0x0200U) != 0;
    data->showOnMmRest = (flags & 0x0400U) != 0;
    data->useStartEncl = (flags & 0x0800U) != 0;
    data->useMultipleEncl = (flags & 0x1000U) != 0;
    data->showOnTop = (flags & 0x2000U) != 0;
    data->showOnBottom = (flags & 0x4000U) != 0;
    data->excludeOthers = (flags & 0x8000U) != 0;
    data->breakMmRest = (flags2 & 1U) != 0;
    data->startJustify = measureNumberAlignment((flags2 >> 2) & 3U);
    data->multipleJustify = measureNumberAlignment((flags2 >> 4) & 3U);
    data->mmRestJustify = measureNumberAlignment((flags2 >> 6) & 3U);
}

void readOldData(const ImportContext& context, const RegionBytes& source, bool hasTail, const std::shared_ptr<Data>& data)
{
    data->startFont = readFont(context, source, 0);
    data->multipleFont = readFont(context, source, 0);
    data->mmRestFont = readFont(context, source, 0);
    data->startEnclosure = readEnclosure(context, source, 70);
    data->multipleEnclosure = readEnclosure(context, source, 70);
    data->useStartEncl = data->startEnclosure->shape != Enclosure::Shape::NoEnclosure && (source.word(82) & 0x2000U) != 0;
    data->useMultipleEncl = data->useStartEncl;
    data->startXdisp = data->multipleXdisp = data->mmRestXdisp = source.signedWord(18);
    data->startYdisp = data->multipleYdisp = data->mmRestYdisp = source.signedWord(20);
    if (hasTail) {
        data->startWith = source.signedWord(84);
        data->leftMmBracketChar = text::codepointFromByte(
            static_cast<std::uint8_t>(source.word(88)), context.document, data->mmRestFont->fontId, text::UnresolvedFontFallback::Symbol);
        data->rightMmBracketChar = text::codepointFromByte(
            static_cast<std::uint8_t>(source.word(90)), context.document, data->mmRestFont->fontId, text::UnresolvedFontFallback::Symbol);
    }
    data->incidence = source.signedWord(10);
    const auto flags = source.word(82);
    const auto alignment = measureNumberAlignment(flags & 0x0003U);
    data->startAlign = data->multipleAlign = data->mmRestAlign = alignment;
    data->startJustify = data->multipleJustify = data->mmRestJustify = alignment;
    data->showOnStart = (flags & 0x8000U) != 0;
    data->showOnEvery = (flags & 0x4000U) != 0;
    data->hideFirstMeasure = (flags & 0x1000U) != 0;
    data->showMmRange = hasTail && (flags & 0x0800U) != 0;
    data->showOnMmRest = hasTail && (flags & 0x0400U) != 0;
    data->showOnTop = (flags & 0x0008U) != 0;
    data->showOnBottom = (flags & 0x0004U) != 0;
}

void readRegion(
    const ImportContext& context, const RegionBytes& source, bool modern, bool unicode, bool hasTail, const std::shared_ptr<Region>& region)
{
    region->scoreData = std::make_shared<Data>(region);
    region->partData = std::make_shared<Data>(region);
    if (modern) {
        const auto dataSize = unicode ? 70U : 66U;
        readModernData(context, source, 0, unicode, region->scoreData);
        readModernData(context, source, dataSize, unicode, region->partData);
        const auto base = dataSize * 2;
        region->startMeas = source.word(base);
        region->endMeas = source.word(base + 2);
        region->startChar = unicode ? source.longValue(base + 4) : source.word(base + 4);
        const auto style = base + (unicode ? 8U : 6U);
        region->base = source.signedWord(style);
        region->numberOffset = source.signedWord(style + 2);
        if (unicode) {
            const auto stringOffset = style + 4;
            auto decode = [&](std::size_t offset) {
                std::int16_t units[48]{};
                for (std::size_t i = 0; i < 48; ++i) {
                    units[i] = static_cast<std::int16_t>(source.word(offset + i * 2));
                }
                return text::utf16ToUtf8(units);
            };
            region->prefix = decode(stringOffset);
            region->suffix = decode(stringOffset + 96);
        } else {
            const auto stringOffset = style + 4;
            const auto fontId = region->scoreData->startFont->fontId;
            region->prefix = text::toUtf8(source.bytesText(stringOffset, 24), context.document, fontId, text::UnresolvedFontFallback::Text);
            region->suffix = text::toUtf8(source.bytesText(stringOffset + 24, 24), context.document, fontId, text::UnresolvedFontFallback::Text);
        }
        const auto flagOffset = unicode ? 344U : 190U;
        const auto flags = source.word(flagOffset);
        region->countFromOne = (flags & 0x0080U) != 0;
        region->time = (flags & 0x0010U) != 0;
        // The two style bits also carry time options. Preserve both views of each bit;
        // the time setting decides which view the application uses.
        region->noZero = region->smpteFrames = (flags & 0x0040U) != 0;
        region->doubleUp = region->includeHours = (flags & 0x0020U) != 0;
        region->useScoreInfoForPart = (flags & 0x0002U) != 0;
        region->region = source.word(flagOffset + 2);
        const auto flags2 = source.word(flagOffset + 4);
        region->timePrecision = static_cast<Region::TimePrecision>((flags2 >> 8) & 0x000fU);
        region->hideScroll = (flags2 & 0x0020U) != 0;
        region->hidePage = (flags2 & 0x0010U) != 0;
    } else {
        readOldData(context, source, hasTail, region->scoreData);
        readOldData(context, source, hasTail, region->partData);
        region->startMeas = source.word(6);
        region->endMeas = source.word(8);
        region->startChar = text::codepointFromByte(
            static_cast<std::uint8_t>(source.word(12)), context.document, region->scoreData->startFont->fontId, text::UnresolvedFontFallback::Text);
        region->base = source.signedWord(14);
        region->numberOffset = source.signedWord(16);
        const auto fontId = region->scoreData->startFont->fontId;
        region->prefix = text::toUtf8(source.bytesText(22, 24), context.document, fontId, text::UnresolvedFontFallback::Text);
        region->suffix = text::toUtf8(source.bytesText(46, 24), context.document, fontId, text::UnresolvedFontFallback::Text);
        const auto flags = source.word(82);
        region->countFromOne = (flags & 0x0080U) != 0;
        region->noZero = (flags & 0x0040U) != 0;
        region->smpteFrames = region->noZero;
        region->doubleUp = (flags & 0x0020U) != 0;
        region->includeHours = region->doubleUp;
        region->region = hasTail ? source.word(86) : region->getCmper();
        region->useScoreInfoForPart = true;
        if (!hasTail) {
            // Believed: the older layout uses the count bit to select the origin of its offset.
            // The modern representation uses a one-based origin for both stored forms.
            if (!region->countFromOne) {
                --region->numberOffset;
            }
            region->countFromOne = true;
        }
    }
}

void reportRegion(const ImportContext& context, const RecordFamilySource& source, std::span<const records::LegacyRow> rows, const Region& region,
    bool modern, bool unicode, bool hasTail, const records::LegacyRow* codaStyle = nullptr)
{
    withReporting(context.report, [&]<typename Reporting>(Reporting& reporting) {
        using Origin = typename Reporting::Origin;
        const auto key = reporting.template instanceKey<Region>(region.getSourcePartId(), region.getCmper());
        reporting.report().setInstanceOrigin(key, Origin::LegacyMus);
        const auto coda = codaStyle != nullptr;
        const RecordFamilySource styleSource{.pool = &context.index.getOthers(), .identity = records::packTag("Mn")};
        const auto set = [&](std::string name, std::int64_t value, std::optional<std::size_t> offset, Origin origin = Origin::LegacyMus,
                             bool style = false) {
            if (offset) {
                const auto& usedSource = style ? styleSource : source;
                const auto& usedRow = style ? *codaStyle : usedSource.rowOfByte(rows, *offset);
                reportLegacyField(reporting, key, usedSource, usedRow, name.c_str(), usedSource.byteOffsetInRow(*offset), value, origin);
            } else {
                reportFallbackField(reporting, key, name.c_str(), origin, value);
            }
        };
        const auto defaultValue = [&](std::string name, std::int64_t value) { set(std::move(name), value, std::nullopt, Origin::LegacyBehavior); };
        const auto fontFields = [&](const std::string& path, const Font& font, std::size_t fontOffset, bool packed, bool present) {
            const auto fontSource = present ? std::optional<std::size_t>(fontOffset) : std::nullopt;
            const auto sizeSource = present ? std::optional<std::size_t>(fontOffset + (packed ? 0U : 2U)) : std::nullopt;
            const auto effectSource = present ? std::optional<std::size_t>(fontOffset + 4U) : std::nullopt;
            const auto emit = [&](const char* leaf, std::int64_t value, std::optional<std::size_t> offset, bool style) {
                set(path + leaf, value, offset, offset ? Origin::LegacyMus : Origin::Unmapped, style);
            };
            emit("fontId", font.fontId, fontSource, packed);
            emit("fontSize", font.fontSize, sizeSource, packed);
            emit("bold", font.bold, effectSource, packed);
            emit("italic", font.italic, effectSource, packed);
            emit("underline", font.underline, effectSource, packed);
            emit("strikeout", font.strikeout, effectSource, packed);
            emit("absolute", font.absolute, effectSource, packed);
            emit("hidden", font.hidden, effectSource, packed);
        };
        const auto enclosureFields = [&](const std::string& path, const std::shared_ptr<Enclosure>& enclosure, std::optional<std::size_t> offset) {
            const auto emit = [&](const char* leaf, std::int64_t value, std::size_t delta, bool musxOnly = false) {
                if (coda && std::string_view(leaf) == "shape") {
                    set(path + leaf, value, 10);
                } else if (coda && enclosure && !musxOnly) {
                    defaultValue(path + leaf, value);
                } else if (offset && !musxOnly) {
                    set(path + leaf, value, *offset + delta);
                } else {
                    set(path + leaf, value, std::nullopt, musxOnly ? Origin::MusxOnly : Origin::Unmapped);
                }
            };
            emit("xAdd", enclosure ? enclosure->xAdd : 0, 0);
            emit("yAdd", enclosure ? enclosure->yAdd : 0, 2);
            emit("xMargin", enclosure ? enclosure->xMargin : 0, 4);
            emit("yMargin", enclosure ? enclosure->yMargin : 0, 6);
            emit("lineWidth", enclosure ? enclosure->lineWidth : 0, 8);
            emit("shape", enclosure ? static_cast<int>(enclosure->shape) : 0, 10);
            emit("cornerRadius", enclosure ? enclosure->cornerRadius : 0, 0, true);
            emit("fixedSize", enclosure && enclosure->fixedSize, 10);
            emit("equalAspect", enclosure && enclosure->equalAspect, 10);
            emit("notTall", enclosure && enclosure->notTall, 10);
            emit("opaque", enclosure && enclosure->opaque, 10);
            emit("roundCorners", enclosure && enclosure->roundCorners, 0, true);
        };
        const auto dataFields = [&](const char* name, const Data& data, std::size_t base) {
            const auto path = std::string(name) + ".";
            const auto fontBase = coda ? 4U : modern ? base : 0U;
            const auto fontStyle = coda;
            fontFields(path + "startFont.", *data.startFont, fontBase, fontStyle, true);
            fontFields(path + "multipleFont.", *data.multipleFont, modern ? base + 6 : fontBase, fontStyle, true);
            fontFields(path + "mmRestFont.", *data.mmRestFont, modern ? base + 12 : fontBase, fontStyle, true);
            const auto enclosureOffset = coda ? std::nullopt : std::optional<std::size_t>(modern ? base + 18 : 70);
            enclosureFields(path + "startEnclosure.", data.startEnclosure, enclosureOffset);
            enclosureFields(
                path + "multipleEnclosure.", data.multipleEnclosure, coda ? std::nullopt : std::optional<std::size_t>(modern ? base + 30 : 70));
            const auto position = coda ? 0U : modern ? base + 42 : 18U;
            const auto positionStyle = coda;
            set(path + "startXdisp", data.startXdisp, position, Origin::LegacyMus, positionStyle);
            set(path + "startYdisp", data.startYdisp, position + 2, Origin::LegacyMus, positionStyle);
            set(path + "multipleXdisp", data.multipleXdisp, modern ? position + 4 : position, Origin::LegacyMus, positionStyle);
            set(path + "multipleYdisp", data.multipleYdisp, modern ? position + 6 : position + 2, Origin::LegacyMus, positionStyle);
            set(path + "mmRestXdisp", data.mmRestXdisp, modern ? position + 8 : position, Origin::LegacyMus, positionStyle);
            set(path + "mmRestYdisp", data.mmRestYdisp, modern ? position + 10 : position + 2, Origin::LegacyMus, positionStyle);
            const auto tail = base + (unicode ? 62U : 58U);
            const auto flags = modern ? std::optional<std::size_t>(tail + 4) : std::optional<std::size_t>(coda ? 10U : 82U);
            const auto flags2 = modern ? std::optional<std::size_t>(tail + 6) : std::nullopt;
            const auto alignmentSource = coda ? std::nullopt : flags;
            const auto oldRestFlags = !modern && hasTail ? flags : std::nullopt;
            const auto oldLeftChar = !modern && hasTail ? std::optional<std::size_t>(88) : std::nullopt;
            const auto oldRightChar = !modern && hasTail ? std::optional<std::size_t>(90) : std::nullopt;
            const auto modernChar = modern ? std::optional<std::size_t>(base + 54) : std::nullopt;
            const auto oldStartWith = !modern && hasTail ? std::optional<std::size_t>(84) : std::nullopt;
            const auto startWith = modern ? std::optional<std::size_t>(tail) : oldStartWith;
            const auto incidence = modern ? std::optional<std::size_t>(tail + 2) : std::optional<std::size_t>(coda ? 4U : 10U);
            const auto show = [&](const char* leaf, std::int64_t value, std::optional<std::size_t> offset) {
                set(path + leaf, value, offset, offset ? Origin::LegacyMus : Origin::LegacyBehavior);
            };
            show("leftMmBracketChar", data.leftMmBracketChar, modern ? modernChar : oldLeftChar);
            show("rightMmBracketChar", data.rightMmBracketChar, modern ? std::optional<std::size_t>(base + (unicode ? 58U : 56U)) : oldRightChar);
            show("startWith", data.startWith, startWith);
            show("incidence", data.incidence, incidence);
            show("startAlign", static_cast<int>(data.startAlign), alignmentSource);
            show("multipleAlign", static_cast<int>(data.multipleAlign), alignmentSource);
            show("mmRestAlign", static_cast<int>(data.mmRestAlign), alignmentSource);
            show("showOnStart", data.showOnStart, flags);
            show("showOnEvery", data.showOnEvery, flags);
            show("hideFirstMeasure", data.hideFirstMeasure, flags);
            show("showMmRange", data.showMmRange, modern ? flags : oldRestFlags);
            show("showOnMmRest", data.showOnMmRest, modern ? flags : oldRestFlags);
            set(path + "useStartEncl", data.useStartEncl,
                coda     ? std::optional<std::size_t>(10)
                : modern ? flags
                         : std::optional<std::size_t>(82),
                modern || coda ? Origin::LegacyMus : Origin::LegacyBehavior);
            set(path + "useMultipleEncl", data.useMultipleEncl,
                coda     ? std::optional<std::size_t>(10)
                : modern ? flags
                         : std::optional<std::size_t>(82),
                modern || coda ? Origin::LegacyMus : Origin::LegacyBehavior);
            show("showOnTop", data.showOnTop, flags);
            show("showOnBottom", data.showOnBottom, flags);
            show("excludeOthers", data.excludeOthers, modern ? flags : std::nullopt);
            show("breakMmRest", data.breakMmRest, flags2);
            show("startJustify", static_cast<int>(data.startJustify), modern ? flags2 : alignmentSource);
            show("multipleJustify", static_cast<int>(data.multipleJustify), modern ? flags2 : alignmentSource);
            show("mmRestJustify", static_cast<int>(data.mmRestJustify), modern ? flags2 : alignmentSource);
        };
        dataFields("scoreData", *region.scoreData, 0);
        dataFields("partData", *region.partData, unicode ? 70U : 66U);
        const auto top = modern ? (unicode ? 140U : 132U) : coda ? 0U : 6U;
        const auto style = modern ? top + (unicode ? 8U : 6U) : coda ? 0U : 14U;
        const auto flag = modern ? (unicode ? 344U : 190U) : coda ? 10U : 82U;
        set("startMeas", region.startMeas, top);
        set("endMeas", region.endMeas, top + 2);
        set("startChar", region.startChar, modern ? top + 4 : coda ? 6U : 12U);
        set("base", region.base, modern ? style : coda ? 8U : 14U);
        set("numberOffset", region.numberOffset, modern ? style + 2 : coda ? 6U : 16U, Origin::LegacyMus, coda);
        if (coda) {
            set("prefix", 0, 10, Origin::LegacyMus, true);
            defaultValue("suffix", 0);
        } else {
            const auto textOffset = modern ? style + 4 : 22U;
            set("prefix", 0, textOffset);
            set("suffix", 0, textOffset + (unicode ? 96U : 24U));
        }
        set("countFromOne", region.countFromOne, flag);
        set("noZero", region.noZero, flag);
        set("doubleUp", region.doubleUp, flag);
        set("time", region.time, modern ? std::optional<std::size_t>(flag) : std::nullopt, modern ? Origin::LegacyMus : Origin::LegacyBehavior);
        set("includeHours", region.includeHours, flag);
        set("smpteFrames", region.smpteFrames, flag);
        set("useScoreInfoForPart", region.useScoreInfoForPart, modern ? std::optional<std::size_t>(flag) : std::nullopt,
            modern ? Origin::LegacyMus : Origin::LegacyBehavior);
        set("region", region.region,
            modern     ? std::optional<std::size_t>(flag + 2)
            : !hasTail ? std::nullopt
                       : std::optional<std::size_t>(86),
            !hasTail ? Origin::LegacyBehavior : Origin::LegacyMus);
        set("timePrecision", static_cast<int>(region.timePrecision), modern ? std::optional<std::size_t>(flag + 4) : std::nullopt,
            modern ? Origin::LegacyMus : Origin::MusxOnly);
        set("hideScroll", region.hideScroll, modern ? std::optional<std::size_t>(flag + 4) : std::nullopt,
            modern ? Origin::LegacyMus : Origin::MusxOnly);
        set("hidePage", region.hidePage, modern ? std::optional<std::size_t>(flag + 4) : std::nullopt, modern ? Origin::LegacyMus : Origin::MusxOnly);
    });
}

void importCodaRegions(const ImportContext& context, const RecordFamilySource& source)
{
    const auto baseRows = source.pool->getArray(regionTag, 0, 0);
    const auto styleRows = context.index.getOthers().getArray(records::packTag("Mn"), 0, 0);
    for (const auto& row : baseRows) {
        if (row.inci >= styleRows.size() || styleRows[row.inci].inci != row.inci) {
            context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info, "Measure number region has no matching Coda style row."});
            continue;
        }
        const auto& style = styleRows[row.inci];
        const auto cmper = static_cast<musx::dom::Cmper>(row.inci + 1);
        auto target = createOthersRecordTarget<Region>(context.document, source, row, cmper);
        target->scoreData = std::make_shared<Data>(target);
        target->partData = std::make_shared<Data>(target);
        const auto sourceWord = [&](std::size_t slot) { return static_cast<std::uint16_t>(row.words[slot]); };
        const auto styleWord = [&](std::size_t slot) { return static_cast<std::uint16_t>(style.words[slot]); };
        const auto packedFont = styleWord(2);
        const auto enclosureShape = (sourceWord(5) >> 8) & 0x000fU;
        const auto useEnclosures = (sourceWord(5) & 0x2000U) != 0;
        for (const auto& data : {target->scoreData, target->partData}) {
            auto font = std::make_shared<Font>(context.document);
            font->fontId = packedFont & 0x00ffU;
            font->fontSize = packedFont >> 8;
            font->setEnigmaStyles(styleWord(4));
            data->startFont = font;
            data->multipleFont = std::make_shared<Font>(*font);
            data->mmRestFont = std::make_shared<Font>(*font);
            data->startXdisp = data->multipleXdisp = data->mmRestXdisp = static_cast<std::int16_t>(styleWord(0));
            data->startYdisp = data->multipleYdisp = data->mmRestYdisp = static_cast<std::int16_t>(styleWord(1));
            data->incidence = sourceWord(2);
            data->showOnStart = (sourceWord(5) & 0x8000U) != 0;
            data->showOnEvery = (sourceWord(5) & 0x4000U) != 0;
            data->hideFirstMeasure = (sourceWord(5) & 0x1000U) != 0;
            data->showOnTop = (sourceWord(5) & 0x0008U) != 0;
            data->showOnBottom = (sourceWord(5) & 0x0004U) != 0;
            data->startEnclosure = makeCodaEnclosure(context, enclosureShape);
            data->multipleEnclosure = makeCodaEnclosure(context, enclosureShape);
            data->useStartEncl = data->useMultipleEncl = useEnclosures && data->startEnclosure != nullptr;
        }
        target->startMeas = sourceWord(0);
        target->endMeas = sourceWord(1);
        target->startChar = text::codepointFromByte(
            static_cast<std::uint8_t>(sourceWord(3)), context.document, target->scoreData->startFont->fontId, text::UnresolvedFontFallback::Text);
        target->base = sourceWord(4);
        target->numberOffset = static_cast<std::int16_t>(styleWord(3));
        const auto packedPrefix = styleWord(5);
        const char prefixBytes[] = {static_cast<char>(packedPrefix >> 8), static_cast<char>(packedPrefix & 0x00ffU)};
        std::string rawPrefix(prefixBytes, 2);
        if (const auto end = rawPrefix.find('\0'); end != std::string::npos) {
            rawPrefix.resize(end);
        }
        target->prefix = text::toUtf8(rawPrefix, context.document, target->scoreData->startFont->fontId, text::UnresolvedFontFallback::Text);
        target->countFromOne = (sourceWord(5) & 0x0080U) != 0;
        target->noZero = (sourceWord(5) & 0x0040U) != 0;
        target->smpteFrames = target->noZero;
        target->doubleUp = (sourceWord(5) & 0x0020U) != 0;
        target->includeHours = target->doubleUp;
        target->region = cmper;
        target->useScoreInfoForPart = true;
        reportRegion(context, source, std::span(&row, 1), *target, false, false, false, &style);
        context.document->getOthers()->add(Region::XmlNodeName, std::move(target));
    }
}

} // namespace

void importMeasureNumberRegions(const ImportContext& context)
{
    const auto source = selectRecordFamilySource(context, context.index.getOthers(), context.index.getClassOthers(), regionTag, regionClass);
    if (!source) {
        return;
    }
    const auto before37 = sourcePredatesVersion(context.profile, FormatEpoch::UncompressedLegacy, versions::finale3_7);
    const auto pairedRows = !source->classRecords && !source->pool->getArray(regionTag, 0, 0).empty()
                            && !context.index.getOthers().getArray(records::packTag("Mn"), 0, 0).empty();
    if (before37 && pairedRows) {
        importCodaRegions(context, *source);
        return;
    }
    for (const auto& [partId, cmper] : recordKeys(*source)) {
        const auto rows = source->pool->getArray(source->identity, cmper, 0, partId);
        if (rows.empty()) {
            continue;
        }
        const auto payload = collectRecordPayload(*source, rows);
        const auto length = payload.size();
        const bool modern = source->classRecords && length != 96;
        const bool unicode = modern && length == 360;
        if ((source->classRecords && length != 96 && length != 204 && length != 360) || (!source->classRecords && (length != 84 && length != 96))
            || (!source->classRecords && length == 84 && !before37)) {
            context.report.diagnostics.push_back({musx::util::Logger::LogLevel::Info,
                "Measure number region " + std::to_string(cmper) + " has an unsupported payload length " + std::to_string(length) + "."});
            continue;
        }
        if (length >= 96 && cmper == 0) {
            // Believed: zero is reserved for legacy storage once each region has its own
            // comparator. The incidence-based layout is handled before this loop.
            continue;
        }
        auto target = createOthersRecordTarget<Region>(context.document, *source, rows.front(), cmper);
        const auto hasTail = length != 84;
        readRegion(context, {payload, context.profile.byteOrder}, modern, unicode, hasTail, target);
        reportRegion(context, *source, rows, *target, modern, unicode, hasTail);
        context.document->getOthers()->add(Region::XmlNodeName, std::move(target));
    }
}

} // namespace others
} // namespace finale_mus_reader
