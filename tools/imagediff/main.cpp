#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "nopeui/graphics/image.h"

namespace {

void PrintUsage() {
    std::fprintf(
        stderr,
        "usage: nopeui_image_diff <expected.png> <actual.png> [--diff <out.png>] "
        "[--tolerance 0.005] [--channel-tolerance 2]\n");
}

bool ParseDouble(const char* text, double* value) {
    char* end = nullptr;
    const double parsed = std::strtod(text, &end);
    if (end == nullptr || *end != '\0' || parsed < 0.0 || parsed > 1.0) {
        return false;
    }
    *value = parsed;
    return true;
}

bool ParseInt(const char* text, int* value) {
    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (end == nullptr || *end != '\0' || parsed < 0 || parsed > 255) {
        return false;
    }
    *value = static_cast<int>(parsed);
    return true;
}

std::uint8_t Channel(std::uint32_t pixel, int shift) {
    return static_cast<std::uint8_t>((pixel >> shift) & 0xFFU);
}

}  // namespace

int main(int argc, char** argv) {
    std::string expected;
    std::string actual;
    std::string diff_path;
    double tolerance = 0.005;
    int channel_tolerance = 2;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--diff" && index + 1 < argc) {
            diff_path = argv[++index];
        } else if (argument == "--tolerance" && index + 1 < argc) {
            if (!ParseDouble(argv[++index], &tolerance)) {
                PrintUsage();
                return 2;
            }
        } else if (argument == "--channel-tolerance" && index + 1 < argc) {
            if (!ParseInt(argv[++index], &channel_tolerance)) {
                PrintUsage();
                return 2;
            }
        } else if (argument.rfind("--", 0) == 0) {
            PrintUsage();
            return 2;
        } else if (expected.empty()) {
            expected = argument;
        } else if (actual.empty()) {
            actual = argument;
        } else {
            PrintUsage();
            return 2;
        }
    }

    if (expected.empty() || actual.empty()) {
        PrintUsage();
        return 2;
    }

    auto expected_result = nopeui::graphics::LoadPng(expected);
    if (!expected_result.ok()) {
        std::fprintf(stderr, "expected: %s\n", expected_result.error().message.c_str());
        return 3;
    }
    auto actual_result = nopeui::graphics::LoadPng(actual);
    if (!actual_result.ok()) {
        std::fprintf(stderr, "actual: %s\n", actual_result.error().message.c_str());
        return 3;
    }

    const auto& left = expected_result.value();
    const auto& right = actual_result.value();
    if (left.width != right.width || left.height != right.height) {
        std::fprintf(
            stderr,
            "size mismatch: %dx%d vs %dx%d\n",
            left.width,
            left.height,
            right.width,
            right.height);
        return 1;
    }

    const std::size_t total = left.pixels.size();
    std::size_t changed = 0;
    int max_delta = 0;
    std::vector<std::uint32_t> diff_pixels;
    if (!diff_path.empty()) {
        diff_pixels = right.pixels;
    }

    for (std::size_t index = 0; index < total; ++index) {
        const std::uint32_t left_pixel = left.pixels[index];
        const std::uint32_t right_pixel = right.pixels[index];
        const int delta = std::max(
            std::max(
                std::abs(static_cast<int>(Channel(left_pixel, 0)) -
                         static_cast<int>(Channel(right_pixel, 0))),
                std::abs(static_cast<int>(Channel(left_pixel, 8)) -
                         static_cast<int>(Channel(right_pixel, 8)))),
            std::max(
                std::abs(static_cast<int>(Channel(left_pixel, 16)) -
                         static_cast<int>(Channel(right_pixel, 16))),
                std::abs(static_cast<int>(Channel(left_pixel, 24)) -
                         static_cast<int>(Channel(right_pixel, 24)))));
        max_delta = std::max(max_delta, delta);
        if (delta > channel_tolerance) {
            ++changed;
            if (!diff_path.empty()) {
                diff_pixels[index] = 0xFFFF0000U;
            }
        }
    }

    const double ratio = total == 0
        ? 1.0
        : static_cast<double>(changed) / static_cast<double>(total);

    if (!diff_path.empty()) {
        auto saved = nopeui::graphics::SavePng(
            diff_path,
            left.width,
            left.height,
            diff_pixels);
        if (!saved.ok()) {
            std::fprintf(stderr, "diff: %s\n", saved.error().message.c_str());
            return 4;
        }
    }

    std::printf(
        "image diff: %zu/%zu changed (%.3f%%), max channel delta %d, "
        "tolerance %.2f%%\n",
        changed,
        total,
        ratio * 100.0,
        max_delta,
        tolerance * 100.0);

    return ratio > tolerance ? 1 : 0;
}
