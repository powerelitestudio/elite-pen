#pragma once
#include "elite_pen/core.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace elite_pen::win {
inline constexpr std::size_t kCustomColorLimit = 16;
inline std::wstring color_hex(Color c) {
    constexpr wchar_t digits[] = L"0123456789ABCDEF";
    std::wstring s = L"#000000";
    const unsigned channels[]{c.r, c.g, c.b};
    for (std::size_t i = 0; i < 3; ++i) {
        s[1 + i * 2] = digits[channels[i] >> 4];
        s[2 + i * 2] = digits[channels[i] & 15];
    }
    return s;
}
inline std::optional<Color> parse_color_hex(std::wstring_view s) {
    if (!s.empty() && s.front() == L'#') s.remove_prefix(1);
    if (s.size() != 6) return {};
    unsigned value = 0;
    for (auto c : s) {
        unsigned digit;
        if (c >= L'0' && c <= L'9') digit = static_cast<unsigned>(c - L'0');
        else if (c >= L'a' && c <= L'f') digit = 10U + static_cast<unsigned>(c - L'a');
        else if (c >= L'A' && c <= L'F') digit = 10U + static_cast<unsigned>(c - L'A');
        else return {};
        value = value * 16 + digit;
    }
    return Color{static_cast<std::uint8_t>(value >> 16), static_cast<std::uint8_t>(value >> 8),
                 static_cast<std::uint8_t>(value), 255};
}
inline std::optional<unsigned> parse_color_channel(std::wstring_view s) {
    if (s.empty() || s.size() > 3) return {};
    unsigned value = 0;
    for (auto c : s) {
        if (c < L'0' || c > L'9') return {};
        value = value * 10 + static_cast<unsigned>(c - L'0');
    }
    return value <= 255 ? std::optional<unsigned>(value) : std::nullopt;
}
} // namespace elite_pen::win
