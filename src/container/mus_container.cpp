// Copyright (c) 2026 Robert G. Patterson
// SPDX-License-Identifier: MIT

#include "container/mus_container.h"

#include "container/product_banner.h"
#include "reader/timing.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <zlib.h>

extern "C" {
#include "blast.h"
}

#include "musx/util/Logger.h"

namespace finale_mus_reader {
namespace container {
namespace {

// The customary start of the record body, and the page size of a Coda-banner pool.
constexpr std::size_t defaultBodyOffset = 0x200;

// A banner-era header records where its record body actually begins, as a 16-bit value at
// 0x60 subject to the file's byte order. It reads 0x200 almost always, but the file-info
// strings that occupy 0x0b0 onwards can overrun that boundary and push the body later.
// Treating 0x200 as a constant leaves such a file unframed, and the failure looks like an
// unknown variant rather than a misread header.
constexpr std::size_t bodyOffsetField = 0x60;

// A signature-bearing header states its byte order in a flags byte at 0x62, whose bit 0 is set
// when the file is big-endian. The byte is a flag field rather than a word, so it reads the same
// in either order and can be tested before any order is known. The same byte carries other
// header flags, so only that bit may be tested.
constexpr std::size_t headerFlagsField = 0x62;
constexpr std::uint8_t bigEndianHeaderFlag = 0x01;

// A Coda-banner header is zero apart from a byte of format flags at 0x81. It is a byte, so it
// needs no byte order.
constexpr std::size_t codaFormatFlagsField = 0x81;

std::size_t readBodyOffset(const std::uint8_t* data, std::size_t size, ByteOrder byteOrder);
constexpr std::size_t maximumDecodedBlockSize = 64U * 1024U * 1024U;

std::uint16_t read16(const std::uint8_t* data, ByteOrder byteOrder)
{
    if (byteOrder == ByteOrder::BigEndian) {
        return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8U) | data[1]);
    }
    return static_cast<std::uint16_t>(data[0] | (static_cast<std::uint16_t>(data[1]) << 8U));
}

std::uint32_t read32(const std::uint8_t* data, ByteOrder byteOrder)
{
    if (byteOrder == ByteOrder::BigEndian) {
        return (static_cast<std::uint32_t>(data[0]) << 24U) | (static_cast<std::uint32_t>(data[1]) << 16U)
               | (static_cast<std::uint32_t>(data[2]) << 8U) | data[3];
    }
    return data[0] | (static_cast<std::uint32_t>(data[1]) << 8U) | (static_cast<std::uint32_t>(data[2]) << 16U)
           | (static_cast<std::uint32_t>(data[3]) << 24U);
}

// Falls back to the customary offset when the field is absent or implausible, so a file
// that predates the field, or one whose header is truncated, behaves as it always did.
std::size_t readBodyOffset(const std::uint8_t* data, std::size_t size, ByteOrder byteOrder)
{
    if (size < bodyOffsetField + 2) {
        return defaultBodyOffset;
    }
    const std::size_t recorded = read16(data + bodyOffsetField, byteOrder);
    return recorded >= defaultBodyOffset && recorded < size ? recorded : defaultBodyOffset;
}

struct BlastOutput
{
    std::vector<std::uint8_t> bytes;
    bool exceededLimit{};
};

unsigned noMoreBlastInput(void*, unsigned char**)
{
    return 0;
}

int appendBlastOutput(void* context, unsigned char* data, unsigned size)
{
    auto& output = *static_cast<BlastOutput*>(context);
    if (size > maximumDecodedBlockSize - output.bytes.size()) {
        output.exceededLimit = true;
        return 1;
    }
    output.bytes.insert(output.bytes.end(), data, data + size);
    return 0;
}

std::optional<std::vector<std::uint8_t>> inflateDcl(const std::uint8_t* data, std::size_t size)
{
    if (size > UINT_MAX) {
        return std::nullopt;
    }

    BlastOutput output;
    unsigned remaining = static_cast<unsigned>(size);
    auto* input = const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(data));

    // blast 1.3 lazily initializes shared decode tables without synchronization.
    static std::mutex blastMutex;
    std::lock_guard<std::mutex> lock(blastMutex);
    const int result = blast(noMoreBlastInput, nullptr, appendBlastOutput, &output, &remaining, &input);
    if (result != 0 || output.exceededLimit) {
        return std::nullopt;
    }
    return output.bytes;
}

std::optional<std::vector<std::uint8_t>> inflateZlib(const std::uint8_t* data, std::size_t size)
{
    if (size > UINT_MAX) {
        return std::nullopt;
    }

    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data));
    stream.avail_in = static_cast<uInt>(size);
    if (inflateInit(&stream) != Z_OK) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> output;
    std::array<std::uint8_t, 16384> chunk{};
    int result = Z_OK;
    while (result == Z_OK) {
        stream.next_out = chunk.data();
        stream.avail_out = static_cast<uInt>(chunk.size());
        result = inflate(&stream, Z_NO_FLUSH);
        const auto produced = chunk.size() - stream.avail_out;
        if (produced > maximumDecodedBlockSize - output.size()) {
            inflateEnd(&stream);
            return std::nullopt;
        }
        output.insert(output.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(produced));
    }

    const bool valid = result == Z_STREAM_END && stream.avail_in == 0;
    inflateEnd(&stream);
    return valid ? std::optional<std::vector<std::uint8_t>>(std::move(output)) : std::nullopt;
}

bool crcMatches(const std::vector<std::uint8_t>& bytes, std::uint32_t expected)
{
    uLong actual = crc32(0L, Z_NULL, 0);
    if (!bytes.empty()) {
        actual = crc32(actual, bytes.data(), static_cast<uInt>(bytes.size()));
    }
    return static_cast<std::uint32_t>(actual) == expected;
}

std::optional<ParsedContainer> tryUncompressed(const std::uint8_t* data, std::size_t size, ByteOrder byteOrder)
{
    FINALE_MUS_READER_CONTAINER_ATTEMPT(containerAttempt, timing::ContainerCandidate::Uncompressed, byteOrder == ByteOrder::BigEndian);
    const auto bodyOffset = readBodyOffset(data, size, byteOrder);
    if (size < bodyOffset + 6) {
        FINALE_MUS_READER_CONTAINER_ATTEMPT_FINISH(containerAttempt, timing::ContainerAttemptResult::InputBounds);
        return std::nullopt;
    }

    ParsedContainer parsed(FormatEpoch::UncompressedLegacy);
    parsed.byteOrder = byteOrder;
    std::size_t offset = bodyOffset;
    while (offset < size) {
        if (size - offset < 6) {
            FINALE_MUS_READER_CONTAINER_ATTEMPT_FINISH(containerAttempt, timing::ContainerAttemptResult::Framing);
            return std::nullopt;
        }
        const auto type = read16(data + offset, byteOrder);
        const auto storedSize32 = read32(data + offset + 2, byteOrder);
        const std::size_t storedSize = storedSize32;
        if (storedSize < 6 || storedSize > size - offset) {
            FINALE_MUS_READER_CONTAINER_ATTEMPT_FINISH(containerAttempt, timing::ContainerAttemptResult::Framing);
            return std::nullopt;
        }
        DecodedBlock block;
        block.info = {type, offset, storedSize, storedSize - 6, false, false};
        block.data.assign(data + offset + 6, data + offset + storedSize);
        parsed.blocks.push_back(std::move(block));
        offset += storedSize;
    }

    constexpr std::array<std::uint16_t, 4> expectedTypes{1, 2, 3, 4};
    if (parsed.blocks.size() != expectedTypes.size()) {
        FINALE_MUS_READER_CONTAINER_ATTEMPT_FINISH(containerAttempt, timing::ContainerAttemptResult::Framing);
        return std::nullopt;
    }
    for (std::size_t index = 0; index < expectedTypes.size(); ++index) {
        if (parsed.blocks[index].info.type != expectedTypes[index]) {
            FINALE_MUS_READER_CONTAINER_ATTEMPT_FINISH(containerAttempt, timing::ContainerAttemptResult::FirstType);
            return std::nullopt;
        }
    }
    FINALE_MUS_READER_CONTAINER_ATTEMPT_FINISH(containerAttempt, timing::ContainerAttemptResult::Accepted);
    return parsed;
}

// Which block types actually carry a compressed member. This is an allowlist on purpose.
// Treating every non-empty block as compressed is the wrong default: a document may embed a
// graphic, and there is no reason to assume it cannot embed audio or something else later.
// An unknown type is then stored verbatim instead of aborting the whole file, which is what
// used to happen -- one embedded EPS cost a document its options, fonts, and layers, because
// the failed member discarded every good block decoded before it.
//
// The DCL era uses 0x000f through 0x0013 and the zlib era 0x0013 and 0x0016 through 0x001d.
// **Believed:** no type outside those ranges is compressed.
bool isCompressedBlockType(std::uint16_t type, bool dcl)
{
    if (dcl) {
        // 0x0012 is compressed despite also being a terminal type number, so a rule keyed on
        // terminal types rather than on this list would discard a Finale 2006 block.
        return type >= 0x000f && type <= 0x0012;
    }
    return type == 0x0016 || type == 0x0017 || type == 0x001a || type == 0x001b;
}

std::optional<ParsedContainer> tryCompressed(const std::uint8_t* data, std::size_t size, ByteOrder byteOrder, bool dcl)
{
    FINALE_MUS_READER_CONTAINER_ATTEMPT(
        containerAttempt, dcl ? timing::ContainerCandidate::Dcl : timing::ContainerCandidate::Zlib, byteOrder == ByteOrder::BigEndian);
    const auto bodyOffset = readBodyOffset(data, size, byteOrder);
    if (size < bodyOffset + 6) {
        FINALE_MUS_READER_CONTAINER_ATTEMPT_FINISH(containerAttempt, timing::ContainerAttemptResult::InputBounds);
        return std::nullopt;
    }

    ParsedContainer parsed(dcl ? FormatEpoch::DclLegacy : FormatEpoch::ZlibLegacy);
    parsed.byteOrder = byteOrder;
    const auto expectedFirstType = dcl ? std::uint16_t{0x000f} : std::uint16_t{0x001a};
    if (read16(data + bodyOffset, byteOrder) != expectedFirstType) {
        FINALE_MUS_READER_CONTAINER_ATTEMPT_FINISH(containerAttempt, timing::ContainerAttemptResult::FirstType);
        return std::nullopt;
    }
    std::size_t offset = bodyOffset;
    while (offset < size) {
        if (size - offset < 6) {
            FINALE_MUS_READER_CONTAINER_ATTEMPT_FINISH(containerAttempt, timing::ContainerAttemptResult::Framing);
            return std::nullopt;
        }
        const auto type = read16(data + offset, byteOrder);
        const std::size_t storedSize = read32(data + offset + 2, byteOrder);
        if (storedSize < 6 || storedSize > size - offset) {
            FINALE_MUS_READER_CONTAINER_ATTEMPT_FINISH(containerAttempt, timing::ContainerAttemptResult::Framing);
            return std::nullopt;
        }

        DecodedBlock block;
        block.info.type = type;
        block.info.sourceOffset = offset;
        block.info.storedSize = storedSize;
        const bool isTerminal = dcl ? (type == 0x0012 || type == 0x0013) : (type == 0x0013 || type == 0x001d);
        if (storedSize == 6) {
            parsed.blocks.push_back(std::move(block));
            offset += storedSize;
            if (isTerminal) {
                parsed.trailingByteCount = size - offset;
                FINALE_MUS_READER_CONTAINER_ATTEMPT_FINISH(containerAttempt, timing::ContainerAttemptResult::Accepted);
                return parsed;
            }
            continue;
        }
        // A block whose type is not known to be compressed is kept verbatim. It has no
        // checksum word, so its payload begins right after the six-byte header rather than
        // after ten. In practice these hold embedded graphics; the reader preserves the
        // bytes and reports them rather than interpreting them.
        if (!isCompressedBlockType(type, dcl)) {
            block.info.stored = true;
            block.info.decodedSize = storedSize - 6;
            block.data.assign(data + offset + 6, data + offset + storedSize);
            parsed.blocks.push_back(std::move(block));
            offset += storedSize;
            if (isTerminal) {
                parsed.trailingByteCount = size - offset;
                FINALE_MUS_READER_CONTAINER_ATTEMPT_FINISH(containerAttempt, timing::ContainerAttemptResult::Accepted);
                return parsed;
            }
            continue;
        }
        if (storedSize < 10) {
            FINALE_MUS_READER_CONTAINER_ATTEMPT_FINISH(containerAttempt, timing::ContainerAttemptResult::Framing);
            return std::nullopt;
        }

        const auto expectedCrc = read32(data + offset + 6, byteOrder);
        const auto compressedSize = storedSize - 10;
        FINALE_MUS_READER_CONTAINER_DECOMPRESSION_BEGIN(containerAttempt, compressedSize);
        auto decoded = dcl ? inflateDcl(data + offset + 10, compressedSize) : inflateZlib(data + offset + 10, compressedSize);
        if (!decoded) {
            FINALE_MUS_READER_CONTAINER_ATTEMPT_FINISH(containerAttempt, timing::ContainerAttemptResult::Inflate);
            // Verbose, not Error. This runs inside a speculative attempt: parse() tries
            // both byte orders and both codecs and keeps the first that works, so a
            // rejection here is ordinary format detection and is routinely followed by a
            // successful parse of the same file. Error would say the import produced no
            // document, which this cannot know. When every attempt fails the epoch stays
            // Unknown and the reader raises that as a warning in its own right.
            musx::util::Logger::log(musx::util::Logger::LogLevel::Verbose,
                std::string(dcl ? "DCL" : "zlib") + " decompression failed at MUS offset " + std::to_string(offset));
            return std::nullopt;
        }
        FINALE_MUS_READER_CONTAINER_DECOMPRESSION_SUCCEEDED(containerAttempt, decoded->size());
        if (!crcMatches(*decoded, expectedCrc)) {
            FINALE_MUS_READER_CONTAINER_ATTEMPT_FINISH(containerAttempt, timing::ContainerAttemptResult::Checksum);
            // Verbose for the same reason as the decompression rejection above: a wrong
            // speculative byte order can inflate to bytes that simply fail the checksum.
            musx::util::Logger::log(musx::util::Logger::LogLevel::Verbose,
                std::string(dcl ? "DCL" : "zlib") + " checksum failed at MUS offset " + std::to_string(offset));
            return std::nullopt;
        }
        block.info.decodedSize = decoded->size();
        block.info.checksumPresent = true;
        block.info.checksumValid = true;
        block.data = std::move(*decoded);
        parsed.blocks.push_back(std::move(block));
        offset += storedSize;
    }

    if (parsed.blocks.empty()) {
        FINALE_MUS_READER_CONTAINER_ATTEMPT_FINISH(containerAttempt, timing::ContainerAttemptResult::Incomplete);
        return std::nullopt;
    }
    FINALE_MUS_READER_CONTAINER_ATTEMPT_FINISH(containerAttempt, timing::ContainerAttemptResult::Accepted);
    return parsed;
}

// A Coda-banner file has no per-block framing. Its records live in a chain of pools, each
// an 8-byte prologue holding a 512-byte page count and the page size, followed by that many
// pages. The pools appear in the same order the later eras use for their typed blocks, so
// they are reported under the uncompressed-era type numbers and everything downstream can
// treat all pre-2007 epochs alike. A file of this era holds three pools; the text region that
// follows them uses different framing and is not decoded here.
ParsedContainer parseCodaBanner(const std::uint8_t* data, std::size_t size, ByteOrder byteOrder)
{
    ParsedContainer parsed(FormatEpoch::CodaBanner);
    parsed.byteOrder = byteOrder;
    parsed.codaFormatFlags = data[codaFormatFlagsField];

    std::size_t offset = defaultBodyOffset;
    std::uint16_t type = 1;
    while (offset + 8 <= size) {
        const auto pages = read32(data + offset, byteOrder);
        const auto pageSize = read32(data + offset + 4, byteOrder);
        // The page size is what identifies a prologue, and it is the only thing that does. A
        // pool of zero pages is an ordinary empty pool, not the end of the chain: the earliest
        // documents store nothing in their details pool and still carry an entries pool behind
        // it. Ending the walk on an empty pool would hide every pool that follows.
        //
        // The chain needs no terminator of its own. What follows the last pool is the text
        // region, whose first four bytes are a chunk length rather than 0x200, so the page-size
        // test ends the walk there. The offset advances by the prologue even when the pool is
        // empty, so a run of empty pools cannot spin.
        if (pageSize != defaultBodyOffset) {
            break;
        }
        const auto body = offset + 8;
        const auto span = static_cast<std::size_t>(pages) * defaultBodyOffset;
        if (span > maximumDecodedBlockSize || body + span > size) {
            break;
        }

        DecodedBlock block;
        block.info.type = type;
        block.info.sourceOffset = body;
        block.info.storedSize = span + 8;
        block.data.assign(data + body, data + body + span);
        block.info.decodedSize = block.data.size();
        parsed.blocks.push_back(std::move(block));

        offset = body + span;
        ++type;
    }

    parsed.trailingByteCount = size - offset;
    parsed.textRegion.assign(data + offset, data + size);
    return parsed;
}

bool hasBannerSignature(const std::uint8_t* data, std::size_t size)
{
    constexpr char signature[] = "ENIGMA BINARY FILE";
    return size >= sizeof(signature) && std::memcmp(data, signature, sizeof(signature) - 1) == 0 && data[sizeof(signature) - 1] == 0;
}

// Files older than the signature open with a plain-text product banner instead, such as
// `Finale(TM) 2.6 Copyright 1987 by Coda.` or Finale 1.0.0's `Finale<0xAA> 1.0.0 ENIGA
// Structures ...`. A signature-bearing file always spells it `Finale(R)`, so a pre-signature
// spelling at offset 0 identifies the era on its own.
//
// The spellings themselves live in banner::parse, which is the only place any of them is
// recognized.
// The byte order a Coda-banner file is written in, or nothing when it is not one of these.
//
// The era's header states no order. The first pool's prologue settles it instead: a page count
// followed by the page size, and that page size reads 0x200 only in the file's own order and
// 0x00020000 in the other. A body that reads 0x200 in neither order is not a pool chain, which
// is also what rejects a pre-signature banner on something that is not a document at all -- an
// AppleDouble metadata artifact, for instance.
std::optional<ByteOrder> codaBannerByteOrder(const std::uint8_t* data, std::size_t size)
{
    const auto parsed = banner::parse(data, size);
    if (!parsed.isPreSignature() || parsed.offset != 0 || size < defaultBodyOffset + 8) {
        return std::nullopt;
    }
    for (const auto byteOrder : {ByteOrder::BigEndian, ByteOrder::LittleEndian}) {
        if (read32(data + defaultBodyOffset + 4, byteOrder) == defaultBodyOffset) {
            return byteOrder;
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<ByteOrder> statedByteOrder(const std::uint8_t* data, std::size_t size)
{
    if (hasBannerSignature(data, size)) {
        if (size <= headerFlagsField) {
            return std::nullopt;
        }
        return (data[headerFlagsField] & bigEndianHeaderFlag) != 0 ? ByteOrder::BigEndian : ByteOrder::LittleEndian;
    }
    return codaBannerByteOrder(data, size);
}

namespace {

bool startsWith(const std::uint8_t* data, std::size_t size, std::string_view prefix)
{
    return size >= prefix.size() && std::memcmp(data, prefix.data(), prefix.size()) == 0;
}

// A MUSX document is a ZIP archive whose first member is its MIME type, stored uncompressed.
bool isMusx(const std::uint8_t* data, std::size_t size)
{
    constexpr std::string_view mimeType = "application/vnd.makemusic.notation";
    const std::string_view head(reinterpret_cast<const char*>(data), (std::min)(size, std::size_t{0x60}));
    return startsWith(data, size, "PK") && head.find(mimeType) != std::string_view::npos;
}

// The four principal pools under each era's block numbers. The others, details, and text
// pools take one of several numbers in the zlib era; any of them counts.
enum class Pool : std::uint8_t {
    Others = 1U << 0U,
    Details = 1U << 1U,
    Entries = 1U << 2U,
    Text = 1U << 3U
};

std::uint8_t poolOfBlock(std::uint16_t type)
{
    switch (type) {
    case 0x0001:
    case 0x000f:
    case 0x0014:
    case 0x0018:
    case 0x001a: return static_cast<std::uint8_t>(Pool::Others);
    case 0x0002:
    case 0x0010:
    case 0x0015:
    case 0x0019:
    case 0x001b: return static_cast<std::uint8_t>(Pool::Details);
    case 0x0003:
    case 0x0011:
    case 0x0016: return static_cast<std::uint8_t>(Pool::Entries);
    case 0x0004:
    case 0x0012:
    case 0x0017: return static_cast<std::uint8_t>(Pool::Text);
    default: return 0;
    }
}

// The others pool's block number names the era: 1 uncompressed, 0x000f DCL, and any zlib number.
FormatEpoch epochOfOthersBlock(std::uint16_t type)
{
    if (type == 0x0001) {
        return FormatEpoch::UncompressedLegacy;
    }
    return type == 0x000f ? FormatEpoch::DclLegacy : FormatEpoch::ZlibLegacy;
}

// A signature-era file is a chain of blocks, each a 16-bit type and a 32-bit length that
// includes its own six-byte header. The chain ends at a block of type 0xffff or where fewer
// bytes remain than a block header needs; such a tail is padding, not damage. A score holds
// all four principal pools; a Finale file missing any of them is a library or another
// non-score kind. Only the block headers are read.
MusRecognitionResult recognizeSignatureFile(const std::uint8_t* data, std::size_t size, ByteOrder byteOrder)
{
    constexpr std::uint8_t allPools = static_cast<std::uint8_t>(Pool::Others) | static_cast<std::uint8_t>(Pool::Details)
                                      | static_cast<std::uint8_t>(Pool::Entries) | static_cast<std::uint8_t>(Pool::Text);
    std::uint8_t pools = 0;
    std::optional<FormatEpoch> epoch;
    for (std::size_t offset = readBodyOffset(data, size, byteOrder); offset < size;) {
        if (size - offset < 6) {
            break;
        }
        const auto type = read16(data + offset, byteOrder);
        if (type == 0xffff) {
            break;
        }
        const std::size_t length = read32(data + offset + 2, byteOrder);
        if (length < 6 || length > size - offset) {
            return {MusRecognition::Damaged, epoch, "Finale binary file whose block lengths overrun the file"};
        }
        const auto pool = poolOfBlock(type);
        if (pool == static_cast<std::uint8_t>(Pool::Others) && !epoch) {
            epoch = epochOfOthersBlock(type);
        }
        pools |= pool;
        offset += length;
    }
    if (pools != allPools) {
        return {MusRecognition::FinaleNonScore, epoch, "Finale library or other non-score Finale binary file"};
    }
    return {MusRecognition::Candidate, epoch, {}};
}

} // namespace

MusRecognitionResult recognize(const std::uint8_t* data, std::size_t size)
{
    if (!data || size == 0) {
        return {};
    }
    if (hasBannerSignature(data, size)) {
        const auto byteOrder = statedByteOrder(data, size);
        if (!byteOrder) {
            return {MusRecognition::Damaged, std::nullopt, "Finale binary file whose header is truncated"};
        }
        return recognizeSignatureFile(data, size, *byteOrder);
    }
    if (const auto parsed = banner::parse(data, size); parsed.isPreSignature() && parsed.offset == 0) {
        if (!codaBannerByteOrder(data, size)) {
            return {MusRecognition::Damaged, FormatEpoch::CodaBanner, "Coda-banner Finale file without a valid first pool"};
        }
        return {MusRecognition::Candidate, FormatEpoch::CodaBanner, {}};
    }
    if (startsWith(data, size, "ENIGMA TRANSPORTABLE FILE") || startsWith(data, size, "ENIGMA t")) {
        return {MusRecognition::FinaleNonScore, std::nullopt, "Finale ETF document"};
    }
    if (isMusx(data, size)) {
        return {MusRecognition::FinaleNonScore, std::nullopt, "Finale MUSX document"};
    }
    if (startsWith(data, size, "[Lesson ")) {
        return {MusRecognition::FinaleNonScore, std::nullopt, "Finale lesson file"};
    }
    if (startsWith(data, size, "FINALE PERFORMANCE ASSESSMENT")) {
        return {MusRecognition::FinaleNonScore, std::nullopt, "SmartMusic performance assessment"};
    }
    return {};
}

ParsedContainer parse(const std::uint8_t* data, std::size_t size)
{
    if (!data && size != 0) {
        throw std::invalid_argument("Null MUS input with nonzero size");
    }

    const auto recognized = recognize(data, size);
    if (recognized.verdict != MusRecognition::Candidate) {
        const auto detail = recognized.detail.empty() ? std::string_view("not a Finale file") : recognized.detail;
        throw std::invalid_argument("This file is not a Finale MUS score: " + std::string(detail) + ".");
    }

    // The order is the one the file states, and the framing is validated in that order only.
    // The pools the recognition walk found name the epoch, so only that epoch's framing is tried.
    const auto byteOrder = *statedByteOrder(data, size);
    std::optional<ParsedContainer> parsed;
    switch (*recognized.epoch) {
    case FormatEpoch::CodaBanner: return parseCodaBanner(data, size, byteOrder);
    case FormatEpoch::UncompressedLegacy: parsed = tryUncompressed(data, size, byteOrder); break;
    case FormatEpoch::DclLegacy: parsed = tryCompressed(data, size, byteOrder, true); break;
    case FormatEpoch::ZlibLegacy: parsed = tryCompressed(data, size, byteOrder, false); break;
    }
    if (!parsed) {
        throw std::invalid_argument("This file does not appear to be a Finale MUS document.");
    }
    return std::move(*parsed);
}

} // namespace container
} // namespace finale_mus_reader
