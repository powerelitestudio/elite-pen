#include "preferences.hpp"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace elite_pen;
using namespace elite_pen::win;

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

std::filesystem::path executable_directory() {
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(),
                                            static_cast<DWORD>(path.size()));
    path.resize(length);
    return std::filesystem::path(path).parent_path();
}

}  // namespace

int main() {
    const auto root = executable_directory();
    const auto data = root / L"data";
    std::error_code error;
    std::filesystem::remove_all(data, error);
    error.clear();
    {
        std::ofstream flag(root / L"portable.flag", std::ios::binary | std::ios::trunc);
        flag << "Elite Pen preferences QA";
    }

    PreferencesStore store;
    check(store.portable(), "portable.flag selects portable preferences");
    check(std::filesystem::path(store.path()).parent_path() == data,
          "portable settings stay beside the executable");

    Preferences expected;
    expected.confirm_clear = true;
    expected.exclude_palette_from_capture = false;
    expected.has_palette_position = true;
    expected.palette_x = -420;
    expected.palette_y = 73;
    expected.palette_size = 3;
    expected.palette_collapsed = true;
    expected.zoom_factor = 6.0F;
    expected.zoom_view = 2;
    expected.zoom_invert = true;
    expected.thickness = 12.0F;
    expected.color = kPurple;
    expected.hotkeys[static_cast<std::size_t>(HotkeyAction::Interact)] =
        {MOD_CONTROL | MOD_ALT, 'D'};
    expected.hotkeys[static_cast<std::size_t>(HotkeyAction::Zoom)] =
        {MOD_WIN | MOD_SHIFT, VK_F8};
    expected.hotkeys[static_cast<std::size_t>(HotkeyAction::Pen)] =
        {MOD_CONTROL | MOD_ALT, 'K'};
    expected.hotkeys[static_cast<std::size_t>(HotkeyAction::Settings)] =
        {MOD_CONTROL | MOD_SHIFT, 'S'};
    expected.hotkeys[static_cast<std::size_t>(HotkeyAction::ZoomFreeze)] =
        {MOD_ALT, 'P'};
    expected.hotkeys[static_cast<std::size_t>(HotkeyAction::ZoomIn)] = {};

    check(store.save(expected), "preferences save atomically");
    const Preferences actual = store.load();
    check(actual.confirm_clear == expected.confirm_clear, "boolean preference round trip");
    check(actual.has_palette_position && actual.palette_x == expected.palette_x &&
          actual.palette_y == expected.palette_y, "negative monitor coordinates round trip");
    check(actual.palette_size == expected.palette_size && actual.palette_collapsed,
          "palette size and hibernation round trip");
    check(actual.zoom_factor == expected.zoom_factor && actual.zoom_view == expected.zoom_view &&
          actual.zoom_invert, "zoom preferences round trip");
    check(actual.thickness == expected.thickness && actual.color == expected.color,
          "drawing preferences round trip");
    check(actual.hotkeys == expected.hotkeys, "custom hotkeys round trip");
    check(!std::filesystem::exists(std::filesystem::path(store.path()).wstring() + L".tmp"),
          "atomic temporary file is removed after save");

    if (failures == 0) std::cout << "Elite Pen preferences: all tests passed\n";
    return failures == 0 ? 0 : 1;
}
