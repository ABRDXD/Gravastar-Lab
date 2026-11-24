// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Frame files for recording jobs.
// - Display frames: PNG, 8-bit RGB, no alpha, gAMA 1/2.2 (the studio's
//   diagnostic display mapping I/(1+I) then gamma 1/2.2). Lossless and
//   deterministic: identical pixels give identical bytes. Encoder: LodePNG
//   (zlib licence, third_party/lodepng, THIRD_PARTY.md).
// - Scientific output: NumPy .npy version 1.0, little-endian float64 '<f8',
//   C order, shape (height, width, channels). Lossless for doubles.
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace gravastar::recording {
struct Rgb8Image {
    int width{}, height{};
    std::vector<std::uint8_t> rgb; // row-major, 3 bytes per pixel
};
std::string encode_png(const Rgb8Image &);
// Throws on any PNG error (truncation, CRC, wrong colour type, size limits).
Rgb8Image decode_png(std::string_view bytes);
std::string encode_npy(int height, int width, int channels, std::span<const double> values);
} // namespace gravastar::recording
