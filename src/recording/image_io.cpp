// SPDX-License-Identifier: GPL-3.0-or-later
#include "gravastar/recording/image_io.hpp"
#include <bit>
#include <cstring>
#include <lodepng.h>
#include <stdexcept>

namespace gravastar::recording {
namespace {
void check_size(int width, int height) {
    if (width < 1 || height < 1 || width > 16384 || height > 16384)
        throw std::invalid_argument("frame size outside 1..16384");
}
} // namespace

std::string encode_png(const Rgb8Image &image) {
    check_size(image.width, image.height);
    if (image.rgb.size() != std::size_t(image.width) * image.height * 3)
        throw std::invalid_argument("RGB buffer does not match the frame size");
    lodepng::State state;
    state.info_raw.colortype = LCT_RGB;
    state.info_raw.bitdepth = 8;
    state.info_png.color.colortype = LCT_RGB;
    state.info_png.color.bitdepth = 8;
    state.encoder.auto_convert = 0; // keep RGB8 exactly; no palette guessing
    state.info_png.gama_defined = 1;
    state.info_png.gama_gamma = 45455; // 1/2.2 in PNG units of 1e-5
    std::vector<unsigned char> out;
    const unsigned error = lodepng::encode(out, image.rgb.data(), unsigned(image.width),
                                           unsigned(image.height), state);
    if (error)
        throw std::runtime_error(std::string("PNG encoding failed: ") + lodepng_error_text(error));
    return {reinterpret_cast<const char *>(out.data()), out.size()};
}

Rgb8Image decode_png(std::string_view bytes) {
    lodepng::State state;
    state.info_raw.colortype = LCT_RGB;
    state.info_raw.bitdepth = 8;
    std::vector<unsigned char> pixels;
    unsigned width = 0, height = 0;
    const unsigned error =
        lodepng::decode(pixels, width, height, state,
                        reinterpret_cast<const unsigned char *>(bytes.data()), bytes.size());
    if (error)
        throw std::runtime_error(std::string("PNG decoding failed: ") + lodepng_error_text(error));
    if (state.info_png.color.colortype != LCT_RGB || state.info_png.color.bitdepth != 8)
        throw std::runtime_error("frame is not an 8-bit RGB PNG");
    Rgb8Image out;
    out.width = int(width);
    out.height = int(height);
    check_size(out.width, out.height);
    out.rgb.assign(pixels.begin(), pixels.end());
    return out;
}

std::string encode_npy(int height, int width, int channels, std::span<const double> values) {
    check_size(width, height);
    if (channels < 1 || channels > 64 ||
        values.size() != std::size_t(width) * height * channels)
        throw std::invalid_argument("radiance values do not match the shape");
    static_assert(std::endian::native == std::endian::little, "npy writer assumes little-endian");
    std::string header = "{'descr': '<f8', 'fortran_order': False, 'shape': (" +
                         std::to_string(height) + ", " + std::to_string(width) + ", " +
                         std::to_string(channels) + "), }";
    // Magic (6) + version (2) + length (2) + header + '\n', padded to 64.
    const std::size_t total = 10 + header.size() + 1;
    header.append((64 - total % 64) % 64, ' ');
    header.push_back('\n');
    std::string out("\x93NUMPY\x01\x00", 8);
    const auto length = static_cast<std::uint16_t>(header.size());
    out.push_back(char(length & 0xff));
    out.push_back(char(length >> 8));
    out += header;
    out.append(reinterpret_cast<const char *>(values.data()), values.size_bytes());
    return out;
}
} // namespace gravastar::recording
