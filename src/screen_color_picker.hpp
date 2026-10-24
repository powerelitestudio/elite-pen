#pragma once
#include "color_values.hpp"
#include <windows.h>
#include <span>

namespace elite_pen::win {
// Top-down BGRA pixels in physical desktop coordinates, including negative origins.
inline std::optional<Color> desktop_pixel(std::span<const std::uint32_t> pixels,
    int width, int height, POINT origin, POINT point) {
    const auto x = static_cast<long long>(point.x) - origin.x;
    const auto y = static_cast<long long>(point.y) - origin.y;
    if (width <= 0 || height <= 0 || x < 0 || y < 0 || x >= width || y >= height) return {};
    const auto index = static_cast<std::uint64_t>(y) * static_cast<unsigned>(width) + static_cast<unsigned>(x);
    if (index >= pixels.size()) return {};
    const auto value = pixels[static_cast<std::size_t>(index)];
    return Color{static_cast<std::uint8_t>(value >> 16), static_cast<std::uint8_t>(value >> 8),
                 static_cast<std::uint8_t>(value), 255};
}
// A frozen desktop is held only in memory; cancel leaves the editor unchanged.
std::optional<Color> pick_screen_color(HWND owner, std::wstring& error);
}
