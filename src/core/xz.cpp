#include "core/xz.hpp"

#include <lzma.h>

#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mtg {

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open file: " + path.string());
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

std::string decompress_xz_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open xz file: " + path.string());

    lzma_stream stream = LZMA_STREAM_INIT;
    const lzma_ret init = lzma_stream_decoder(&stream, UINT64_MAX, LZMA_CONCATENATED);
    if (init != LZMA_OK) throw std::runtime_error("lzma_stream_decoder failed");

    struct Cleanup {
        lzma_stream* stream;
        ~Cleanup() { lzma_end(stream); }
    } cleanup{&stream};

    std::array<std::uint8_t, 64 * 1024> input{};
    std::array<std::uint8_t, 64 * 1024> output{};
    std::string result;

    bool eof = false;
    lzma_ret ret = LZMA_OK;
    while (ret != LZMA_STREAM_END) {
        if (stream.avail_in == 0 && !eof) {
            in.read(reinterpret_cast<char*>(input.data()), static_cast<std::streamsize>(input.size()));
            const auto got = in.gcount();
            if (got < 0) throw std::runtime_error("failed while reading xz file");
            stream.next_in = input.data();
            stream.avail_in = static_cast<std::size_t>(got);
            eof = in.eof();
        }

        stream.next_out = output.data();
        stream.avail_out = output.size();
        ret = lzma_code(&stream, eof ? LZMA_FINISH : LZMA_RUN);

        const auto produced = output.size() - stream.avail_out;
        result.append(reinterpret_cast<const char*>(output.data()), produced);

        if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
            throw std::runtime_error("xz decompression failed, liblzma code " + std::to_string(ret));
        }

        if (eof && stream.avail_in == 0 && produced == 0 && ret == LZMA_OK) {
            throw std::runtime_error("unexpected end of xz stream");
        }
    }

    return result;
}

} // namespace mtg
