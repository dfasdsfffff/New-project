#include "ZipArchive.hpp"

#include "cppwordkit/Error.hpp"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace cppwordkit {
namespace {

constexpr std::uint32_t localFileHeaderSignature = 0x04034b50;
constexpr std::uint32_t centralDirectorySignature = 0x02014b50;
constexpr std::uint32_t endOfCentralDirectorySignature = 0x06054b50;
constexpr std::uint16_t storeMethod = 0;
constexpr std::uint16_t deflateMethod = 8;

std::uint16_t readU16(const std::vector<std::uint8_t>& data, std::size_t offset) {
    if (offset + 2 > data.size()) {
        throw PackageException("Unexpected end of zip data");
    }
    return static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
}

std::uint32_t readU32(const std::vector<std::uint8_t>& data, std::size_t offset) {
    if (offset + 4 > data.size()) {
        throw PackageException("Unexpected end of zip data");
    }
    return static_cast<std::uint32_t>(
        static_cast<std::uint32_t>(data[offset]) |
        (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
        (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
        (static_cast<std::uint32_t>(data[offset + 3]) << 24)
    );
}

void writeU16(std::vector<std::uint8_t>& data, std::uint16_t value) {
    data.push_back(static_cast<std::uint8_t>(value & 0xff));
    data.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
}

void writeU32(std::vector<std::uint8_t>& data, std::uint32_t value) {
    data.push_back(static_cast<std::uint8_t>(value & 0xff));
    data.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
    data.push_back(static_cast<std::uint8_t>((value >> 16) & 0xff));
    data.push_back(static_cast<std::uint8_t>((value >> 24) & 0xff));
}

std::vector<std::uint8_t> readFile(const Path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw PackageException("Unable to open zip file: " + path.string());
    }
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void writeFile(const Path& path, const std::vector<std::uint8_t>& data) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw PackageException("Unable to write zip file: " + path.string());
    }
    output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

std::vector<std::uint8_t> inflateRaw(
    const std::uint8_t* input,
    std::size_t compressedSize,
    std::size_t uncompressedSize
) {
    std::vector<std::uint8_t> output(uncompressedSize);
    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(input));
    stream.avail_in = static_cast<uInt>(compressedSize);
    stream.next_out = reinterpret_cast<Bytef*>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());

    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        throw PackageException("Unable to initialize zip inflater");
    }

    const int result = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    if (result != Z_STREAM_END) {
        throw PackageException("Unable to inflate zip entry");
    }
    return output;
}

std::vector<std::uint8_t> deflateRaw(const std::vector<std::uint8_t>& input) {
    z_stream stream{};
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        throw PackageException("Unable to initialize zip deflater");
    }

    const auto bound = deflateBound(&stream, static_cast<uLong>(input.size()));
    std::vector<std::uint8_t> output(bound);
    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(input.data()));
    stream.avail_in = static_cast<uInt>(input.size());
    stream.next_out = reinterpret_cast<Bytef*>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());

    const int result = deflate(&stream, Z_FINISH);
    if (result != Z_STREAM_END) {
        deflateEnd(&stream);
        throw PackageException("Unable to deflate zip entry");
    }

    output.resize(stream.total_out);
    deflateEnd(&stream);
    return output;
}

std::uint32_t checkedU32(std::size_t value, const char* label) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw PackageException(std::string(label) + " is too large for zip32");
    }
    return static_cast<std::uint32_t>(value);
}

std::uint16_t checkedU16(std::size_t value, const char* label) {
    if (value > std::numeric_limits<std::uint16_t>::max()) {
        throw PackageException(std::string(label) + " is too large for zip32");
    }
    return static_cast<std::uint16_t>(value);
}

} // namespace

ZipArchive ZipArchive::read(const Path& path) {
    const auto file = readFile(path);
    if (file.size() < 22) {
        throw PackageException("Invalid zip file: " + path.string());
    }

    const std::size_t searchStart = file.size() > 0xffff + 22 ? file.size() - (0xffff + 22) : 0;
    std::optional<std::size_t> eocdOffset;
    for (std::size_t offset = file.size() - 22; offset + 1 > searchStart; --offset) {
        if (readU32(file, offset) == endOfCentralDirectorySignature) {
            eocdOffset = offset;
            break;
        }
        if (offset == 0) {
            break;
        }
    }
    if (!eocdOffset) {
        throw PackageException("Zip end of central directory not found");
    }

    const auto entryCount = readU16(file, *eocdOffset + 10);
    const auto centralOffset = readU32(file, *eocdOffset + 16);
    std::size_t cursor = centralOffset;

    ZipArchive archive;
    for (std::uint16_t i = 0; i < entryCount; ++i) {
        if (readU32(file, cursor) != centralDirectorySignature) {
            throw PackageException("Invalid zip central directory");
        }
        const auto flags = readU16(file, cursor + 8);
        const auto method = readU16(file, cursor + 10);
        const auto compressedSize = readU32(file, cursor + 20);
        const auto uncompressedSize = readU32(file, cursor + 24);
        const auto nameLength = readU16(file, cursor + 28);
        const auto extraLength = readU16(file, cursor + 30);
        const auto commentLength = readU16(file, cursor + 32);
        const auto localOffset = readU32(file, cursor + 42);

        if ((flags & 0x1) != 0) {
            throw PackageException("Encrypted zip entries are not supported");
        }

        const std::string name(
            reinterpret_cast<const char*>(file.data() + cursor + 46),
            reinterpret_cast<const char*>(file.data() + cursor + 46 + nameLength)
        );

        if (readU32(file, localOffset) != localFileHeaderSignature) {
            throw PackageException("Invalid zip local file header");
        }
        const auto localNameLength = readU16(file, localOffset + 26);
        const auto localExtraLength = readU16(file, localOffset + 28);
        const auto dataOffset = localOffset + 30 + localNameLength + localExtraLength;
        if (dataOffset + compressedSize > file.size()) {
            throw PackageException("Zip entry data extends past end of file");
        }

        std::vector<std::uint8_t> data;
        if (method == storeMethod) {
            data.assign(file.begin() + static_cast<std::ptrdiff_t>(dataOffset),
                file.begin() + static_cast<std::ptrdiff_t>(dataOffset + compressedSize));
        } else if (method == deflateMethod) {
            data = inflateRaw(file.data() + dataOffset, compressedSize, uncompressedSize);
        } else {
            throw PackageException("Unsupported zip compression method");
        }

        archive.addOrReplace(name, std::move(data));
        cursor += 46 + nameLength + extraLength + commentLength;
    }

    return archive;
}

void ZipArchive::write(const Path& path) const {
    struct CentralRecord {
        std::string name;
        std::uint32_t crc{};
        std::uint32_t compressedSize{};
        std::uint32_t uncompressedSize{};
        std::uint32_t localOffset{};
    };

    std::vector<std::uint8_t> output;
    std::vector<CentralRecord> central;
    central.reserve(entries_.size());

    for (const auto& entry : entries_) {
        const auto compressed = deflateRaw(entry.data);
        const auto crc = crc32(0L, reinterpret_cast<const Bytef*>(entry.data.data()), static_cast<uInt>(entry.data.size()));
        const auto nameLength = checkedU16(entry.name.size(), "zip entry name");
        const auto localOffset = checkedU32(output.size(), "zip local offset");
        const auto compressedSize = checkedU32(compressed.size(), "zip compressed entry");
        const auto uncompressedSize = checkedU32(entry.data.size(), "zip uncompressed entry");

        writeU32(output, localFileHeaderSignature);
        writeU16(output, 20);
        writeU16(output, 0);
        writeU16(output, deflateMethod);
        writeU16(output, 0);
        writeU16(output, 0);
        writeU32(output, crc);
        writeU32(output, compressedSize);
        writeU32(output, uncompressedSize);
        writeU16(output, nameLength);
        writeU16(output, 0);
        output.insert(output.end(), entry.name.begin(), entry.name.end());
        output.insert(output.end(), compressed.begin(), compressed.end());

        central.push_back({entry.name, crc, compressedSize, uncompressedSize, localOffset});
    }

    const auto centralOffset = checkedU32(output.size(), "zip central directory offset");
    for (const auto& record : central) {
        const auto nameLength = checkedU16(record.name.size(), "zip entry name");
        writeU32(output, centralDirectorySignature);
        writeU16(output, 20);
        writeU16(output, 20);
        writeU16(output, 0);
        writeU16(output, deflateMethod);
        writeU16(output, 0);
        writeU16(output, 0);
        writeU32(output, record.crc);
        writeU32(output, record.compressedSize);
        writeU32(output, record.uncompressedSize);
        writeU16(output, nameLength);
        writeU16(output, 0);
        writeU16(output, 0);
        writeU16(output, 0);
        writeU16(output, 0);
        writeU32(output, 0);
        writeU32(output, record.localOffset);
        output.insert(output.end(), record.name.begin(), record.name.end());
    }

    const auto centralSize = checkedU32(output.size() - centralOffset, "zip central directory");
    const auto entryCount = checkedU16(central.size(), "zip entry count");
    writeU32(output, endOfCentralDirectorySignature);
    writeU16(output, 0);
    writeU16(output, 0);
    writeU16(output, entryCount);
    writeU16(output, entryCount);
    writeU32(output, centralSize);
    writeU32(output, centralOffset);
    writeU16(output, 0);

    writeFile(path, output);
}

bool ZipArchive::contains(const std::string& name) const {
    return index_.contains(name);
}

std::string ZipArchive::text(const std::string& name) const {
    const auto data = bytes(name);
    return std::string(data.begin(), data.end());
}

std::vector<std::uint8_t> ZipArchive::bytes(const std::string& name) const {
    const auto it = index_.find(name);
    if (it == index_.end()) {
        throw PackageException("Zip entry not found: " + name);
    }
    return entries_[it->second].data;
}

std::vector<std::string> ZipArchive::names() const {
    std::vector<std::string> names;
    names.reserve(entries_.size());
    for (const auto& entry : entries_) {
        names.push_back(entry.name);
    }
    return names;
}

void ZipArchive::setText(const std::string& name, const std::string& content) {
    setBytes(name, {content.begin(), content.end()});
}

void ZipArchive::setBytes(const std::string& name, std::vector<std::uint8_t> content) {
    addOrReplace(name, std::move(content));
}

void ZipArchive::addOrReplace(std::string name, std::vector<std::uint8_t> data) {
    const auto it = index_.find(name);
    if (it != index_.end()) {
        entries_[it->second].data = std::move(data);
        return;
    }
    index_[name] = entries_.size();
    entries_.push_back({std::move(name), std::move(data)});
}

} // namespace cppwordkit
