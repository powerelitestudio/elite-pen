#pragma once
#include "color_values.hpp"
#include <windows.h>
#include <vector>
#include <functional>
namespace elite_pen::win {
std::optional<Color> custom_color_dialog(HWND owner, Color initial, std::vector<Color>& favorites,
    const std::function<bool(const std::vector<Color>&)>& save);
}
