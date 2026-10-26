#include "nutmeg/sim/input_stream.h"

#include "nutmeg/core/binary.h"
#include "nutmeg/core/file_io.h"

namespace nutmeg {

namespace {

constexpr std::uint32_t kMagic = 0x5349464Eu;

}

std::vector<std::uint8_t> InputStream::toBytes(std::string_view version) const
{
    BinaryWriter writer;
    writer.write(kMagic);
    writer.write(std::string(version));
    writer.write(frames_);
    return writer.take();
}

bool InputStream::fromBytes(std::span<const std::uint8_t> bytes, InputStream& stream, std::string& error, std::string_view version)
{
    stream.frames_.clear();
    BinaryReader reader(bytes);

    std::uint32_t magic = 0;
    if (!reader.read(magic) || magic != kMagic) {
        error = "not an input stream file";
        return false;
    }
    std::string stamped;
    if (!reader.read(stamped)) {
        error = "input stream file is truncated in its header";
        return false;
    }
    if (stamped != version) {
        error = "input stream was recorded by engine version " + stamped + ", this is " + std::string(version);
        return false;
    }
    std::vector<InputFrame> frames;
    if (!reader.read(frames)) {
        error = "input stream file is truncated";
        return false;
    }
    if (!reader.atEnd()) {
        error = "input stream file has trailing data";
        return false;
    }
    stream.frames_ = std::move(frames);
    return true;
}

bool InputStream::save(const std::filesystem::path& path) const
{
    return writeFileAtomically(path, toBytes());
}

bool InputStream::load(const std::filesystem::path& path, InputStream& stream, std::string& error)
{
    std::vector<std::uint8_t> bytes;
    if (!readFileBytes(path, bytes)) {
        stream.frames_.clear();
        error = "cannot read " + path.string();
        return false;
    }
    return fromBytes(bytes, stream, error);
}

}
