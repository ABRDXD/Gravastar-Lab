// SPDX-License-Identifier: GPL-3.0-or-later
// PNG frame and .npy radiance files: exact round trip, determinism, error
// detection, and the .npy 1.0 layout.
#include "gravastar/recording/image_io.hpp"
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>

namespace r = gravastar::recording;
int main() {
    int failures = 0, checks = 0;
    auto check = [&](bool pass, const char *name) {
        ++checks;
        if (!pass) {
            ++failures;
            std::cerr << "FAIL " << name << '\n';
        }
    };
    auto throws = [](auto action) {
        try {
            action();
        } catch (const std::exception &) {
            return true;
        }
        return false;
    };
    try {
        std::mt19937 random(7);
        r::Rgb8Image image{37, 23, {}};
        for (int i = 0; i < 37 * 23 * 3; ++i)
            image.rgb.push_back(static_cast<std::uint8_t>(random()));
        const auto png = r::encode_png(image);
        const auto back = r::decode_png(png);
        check(back.width == 37 && back.height == 23 && back.rgb == image.rgb,
              "PNG round trip is exact");
        check(r::encode_png(image) == png, "PNG encoding is deterministic");
        check(png.find("gAMA") != std::string::npos, "PNG records gamma 1/2.2");
        check(png.find("tEXt") == std::string::npos, "PNG carries no version text");
        check(throws([&] { r::decode_png(png.substr(0, png.size() / 2)); }),
              "truncated PNG is rejected");
        auto corrupt = png;
        corrupt[png.size() / 2] ^= 0x40;
        check(throws([&] { r::decode_png(corrupt); }), "corrupted PNG is rejected");
        check(throws([&] { r::encode_png({2, 2, {1, 2, 3}}); }), "wrong buffer size is rejected");

        const std::vector<double> values{0.5, -1.0, 1e300, 0.0, 2.0, 3.0};
        const auto npy = r::encode_npy(1, 2, 3, values);
        check(npy.compare(0, 8, std::string("\x93NUMPY\x01\x00", 8)) == 0, "npy magic and version");
        const std::size_t header = std::size_t(std::uint8_t(npy[8])) | std::size_t(std::uint8_t(npy[9])) << 8;
        check((10 + header) % 64 == 0 && npy[10 + header - 1] == '\n', "npy header padded to 64");
        check(npy.find("'descr': '<f8'") != std::string::npos &&
                  npy.find("'shape': (1, 2, 3)") != std::string::npos,
              "npy dtype and shape");
        check(npy.size() == 10 + header + values.size() * 8 &&
                  std::memcmp(npy.data() + 10 + header, values.data(), values.size() * 8) == 0,
              "npy data bytes are the doubles exactly");
        check(throws([&] { r::encode_npy(2, 2, 3, values); }), "npy shape mismatch rejected");
    } catch (const std::exception &e) {
        std::cerr << "image io: " << e.what() << '\n';
        return 1;
    }
    std::cout << (failures ? "FAIL " : "PASS ") << checks << " image I/O checks\n";
    return failures ? 1 : 0;
}
