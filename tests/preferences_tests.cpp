#include "preferences.hpp"
#include "history_shortcuts.hpp"
#include "control_layout.hpp"

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
    HistoryShortcutRouter keys;
    check(keys.route('Z', true, MOD_CONTROL, true, false) == HistoryKeyResult::Undo,
          "Ctrl+Z undoes annotations");
    check(keys.route('Z', true, MOD_CONTROL, true, false) == HistoryKeyResult::Consume,
          "holding undo does not erase the whole history");
    check(keys.route('Z', false, 0, false, false) == HistoryKeyResult::Consume,
          "owned key-up stays consumed after leaving drawing mode");
    check(keys.route('Y', true, MOD_CONTROL, true, false) == HistoryKeyResult::Redo,
          "Ctrl+Y redoes annotations");
    keys.route('Y', false, 0, true, false);
    for (const UINT modifiers : {0U, static_cast<UINT>(MOD_SHIFT),
            static_cast<UINT>(MOD_CONTROL | MOD_SHIFT),
            static_cast<UINT>(MOD_CONTROL | MOD_ALT),
            static_cast<UINT>(MOD_CONTROL | MOD_WIN)}) {
        check(keys.route('Z', true, modifiers, true, false) == HistoryKeyResult::Pass,
              "plain typing, zoom, AltGr and other combinations pass through");
        check(keys.route('Z', false, modifiers, true, false) == HistoryKeyResult::Pass,
              "unowned key-up passes through");
    }
    check(keys.route('Z', true, MOD_CONTROL, false, false) == HistoryKeyResult::Pass,
          "interact, live zoom and text editing keep their own undo");
    check(keys.route('Z', true, MOD_CONTROL, true, false) == HistoryKeyResult::Pass,
          "a key already held before annotation is not stolen on repeat");
    keys.route('Z', false, 0, true, false);
    check(keys.route('Z', true, MOD_CONTROL, true, true) == HistoryKeyResult::Pass,
          "custom shortcuts take precedence without double dispatch");
    keys.route('Z', false, 0, true, false);
    check(keys.route('V', true, MOD_CONTROL, true, false) == HistoryKeyResult::Pass,
          "unrelated shortcuts remain untouched");

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
    const Preferences fresh = store.load();
    check(fresh.geometry_size == 1 && !fresh.geometry_pinned &&
          !fresh.geometry_visible && !fresh.has_geometry_position,
          "figures start at 100 percent as an optional transient panel");
    check(kHotkeyActionCount == 46 && kGlobalHotkeyActionCount == 36,
          "symbols have four global actions while all ten zoom actions stay contextual");
    for (auto action : {HotkeyAction::Cross, HotkeyAction::Checkmark,
                        HotkeyAction::Warning, HotkeyAction::Prohibition}) {
        check(fresh.hotkeys[static_cast<std::size_t>(action)] == HotkeyBinding{},
              "symbols do not occupy a new global shortcut until assigned");
    }
    check(fresh.theme == AppTheme::Light,
          "fresh installations start with the light appearance");
    check(fresh.hotkeys[static_cast<std::size_t>(HotkeyAction::ZoomEdit)] ==
              HotkeyBinding{0, 'E'},
          "zoom edit ships with the contextual E shortcut");
    check(fresh.hotkeys[static_cast<std::size_t>(HotkeyAction::Pentagon)] ==
              HotkeyBinding{} &&
          fresh.hotkeys[static_cast<std::size_t>(HotkeyAction::Hexagon)] ==
              HotkeyBinding{},
          "polygon tools ship without preassigned shortcuts");
    check(fresh.zoom_lens_diameter == 520,
          "lens mode starts with the standard circular diameter");

    std::filesystem::create_directories(data, error);
    {
        std::ofstream legacy(std::filesystem::path(store.path()),
                             std::ios::binary | std::ios::trunc);
        legacy << "[ElitePen]\r\n"
               << "HotkeyInteract=6,80\r\n"
               << "HotkeyVisibility=6,72\r\n"
               << "HotkeyWhiteboard=3,88\r\n"
               << "HotkeyUndo=6,90\r\n"
               << "HotkeyClear=6,67\r\n"
               << "HotkeyZoom=6,77\r\n"
               << "HotkeyColorPanel=0,0\r\n"
               << "HotkeyPaletteCollapse=0,0\r\n";
    }
    const Preferences migrated = store.load();
    check(migrated.theme == AppTheme::Light,
          "settings without an appearance choice inherit the light default");
    check(migrated.hotkey_scheme_version == kCurrentHotkeySchemeVersion,
          "legacy shortcuts migrate to the current scheme");
    check(migrated.hotkeys[static_cast<std::size_t>(HotkeyAction::Interact)] ==
              HotkeyBinding{MOD_CONTROL | MOD_SHIFT, 'Q'} &&
          migrated.hotkeys[static_cast<std::size_t>(HotkeyAction::Visibility)] ==
              HotkeyBinding{MOD_CONTROL | MOD_SHIFT, 'A'} &&
          migrated.hotkeys[static_cast<std::size_t>(HotkeyAction::Undo)] ==
              HotkeyBinding{MOD_CONTROL | MOD_ALT, 'Z'} &&
          migrated.hotkeys[static_cast<std::size_t>(HotkeyAction::Clear)] ==
              HotkeyBinding{MOD_CONTROL | MOD_SHIFT, 'E'} &&
          migrated.hotkeys[static_cast<std::size_t>(HotkeyAction::Zoom)] ==
              HotkeyBinding{MOD_CONTROL | MOD_SHIFT, 'Z'},
          "legacy primary shortcuts receive the new defaults");
    check(migrated.hotkeys[static_cast<std::size_t>(HotkeyAction::ColorPanel)] ==
              HotkeyBinding{MOD_CONTROL | MOD_SHIFT, '7'} &&
          migrated.hotkeys[static_cast<std::size_t>(HotkeyAction::PaletteCollapse)] ==
              HotkeyBinding{MOD_CONTROL | MOD_SHIFT, 'D'} &&
          migrated.hotkeys[static_cast<std::size_t>(HotkeyAction::ColorBlack)] ==
              HotkeyBinding{MOD_CONTROL | MOD_SHIFT, '1'},
          "legacy settings gain palette and direct color shortcuts");
    check(migrated.hotkeys[static_cast<std::size_t>(HotkeyAction::Whiteboard)] ==
              HotkeyBinding{MOD_CONTROL | MOD_ALT, 'X'},
          "migration preserves genuinely customized shortcuts");
    check(migrated.hotkeys[static_cast<std::size_t>(HotkeyAction::ZoomEdit)] ==
              HotkeyBinding{0, 'E'},
          "legacy settings gain editable zoom without shifting saved actions");

    {
        std::ofstream previous(std::filesystem::path(store.path()),
                               std::ios::binary | std::ios::trunc);
        previous << "[ElitePen]\r\nHotkeySchemeVersion=2\r\n"
                    "HotkeyPentagon=3,119\r\nHotkeyHexagon=6,120\r\n"
                    "HotkeyZoomFreeze=1,80\r\nHotkeyZoomEdit=1,69\r\n";
    }
    const Preferences previous_version = store.load();
    check(previous_version.hotkeys[static_cast<std::size_t>(HotkeyAction::Pentagon)] ==
              HotkeyBinding{3, 119} &&
          previous_version.hotkeys[static_cast<std::size_t>(HotkeyAction::Hexagon)] ==
              HotkeyBinding{6, 120} &&
          previous_version.hotkeys[static_cast<std::size_t>(HotkeyAction::ZoomFreeze)] ==
              HotkeyBinding{MOD_ALT, 'P'} &&
          previous_version.hotkeys[static_cast<std::size_t>(HotkeyAction::ZoomEdit)] ==
              HotkeyBinding{MOD_ALT, 'E'},
          "adding symbols preserves existing polygon and contextual zoom keys by name");
    check(previous_version.hotkeys[static_cast<std::size_t>(HotkeyAction::Cross)] ==
              HotkeyBinding{}, "old settings gain symbols without overwriting user bindings");

    Preferences expected;
    expected.theme = AppTheme::Light;
    expected.control_mode = ControlMode::Linear;
    expected.confirm_clear = true;
    expected.exclude_palette_from_capture = false;
    expected.has_palette_position = true;
    expected.palette_x = -420;
    expected.palette_y = 73;
    expected.palette_size = 3;
    expected.palette_collapsed = true;
    expected.geometry_size = 5;
    expected.geometry_pinned = true;
    expected.geometry_visible = true;
    expected.has_geometry_position = true;
    expected.geometry_x = -2880;
    expected.geometry_y = 260;
    expected.zoom_factor = 6.0F;
    expected.zoom_view = 2;
    expected.zoom_lens_diameter = 760;
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
    expected.hotkeys[static_cast<std::size_t>(HotkeyAction::ZoomEdit)] =
        {MOD_ALT, 'E'};
    for (std::size_t offset = 0; offset < 4; ++offset) {
        expected.hotkeys[static_cast<std::size_t>(HotkeyAction::Cross) + offset] =
            {0, VK_F13 + static_cast<UINT>(offset)};
    }

    check(store.save(expected), "preferences save atomically");
    const Preferences actual = store.load();
    check(actual.theme == expected.theme, "appearance theme round trip");
    check(actual.control_mode == expected.control_mode,
          "control presentation mode round trip");
    check(actual.confirm_clear == expected.confirm_clear, "boolean preference round trip");
    check(actual.has_palette_position && actual.palette_x == expected.palette_x &&
          actual.palette_y == expected.palette_y, "negative monitor coordinates round trip");
    check(actual.palette_size == expected.palette_size && actual.palette_collapsed,
          "palette size and hibernation round trip");
    check(actual.geometry_size == 5 && actual.geometry_pinned && actual.geometry_visible &&
          actual.has_geometry_position && actual.geometry_x == -2880 && actual.geometry_y == 260,
          "pinned figures retain an independent size and negative monitor position");
    check(actual.zoom_factor == expected.zoom_factor && actual.zoom_view == expected.zoom_view &&
          actual.zoom_lens_diameter == expected.zoom_lens_diameter && actual.zoom_invert,
          "zoom preferences round trip");
    check(actual.thickness == expected.thickness && actual.color == expected.color,
          "drawing preferences round trip");
    check(actual.hotkeys == expected.hotkeys, "custom hotkeys round trip");
    check(!std::filesystem::exists(std::filesystem::path(store.path()).wstring() + L".tmp"),
          "atomic temporary file is removed after save");

    const std::array<int, 6> palette_widths{139, 174, 218, 261, 348, 522};
    const std::array<int, 6> palette_heights{134, 168, 210, 252, 336, 504};
    for (std::size_t index = 0; index < kPaletteScales.size(); ++index) {
        expected.palette_size = static_cast<int>(index);
        check(store.save(expected), "each control size saves successfully");
        check(store.load().palette_size == expected.palette_size,
              "all six size indices survive restart without changing legacy indices");
        check(control_pixel_width(ControlMode::Palette, kPaletteScales[index], false) ==
                  palette_widths[index] &&
              control_pixel_height(ControlMode::Palette, kPaletteScales[index], false) ==
                  palette_heights[index],
              "palette dimensions preserve standard-relative percentages");
    }
    check(control_pixel_width(ControlMode::Linear, kPaletteScales[4], false) == 91 &&
          control_pixel_height(ControlMode::Linear, kPaletteScales[4], false) == 811 &&
          control_pixel_width(ControlMode::Linear, kPaletteScales[5], false) == 137 &&
          control_pixel_height(ControlMode::Linear, kPaletteScales[5], false) == 1217,
          "extra large and giant scale the linear presentation too");
    check(control_pixel_width(ControlMode::Palette, kPaletteScales[5], true) == 157 &&
          control_pixel_height(ControlMode::Palette, kPaletteScales[5], true) == 151 &&
          control_pixel_height(ControlMode::Linear, kPaletteScales[5], true) == 148,
          "giant controls keep their scaled collapsed dimensions");
    const RECT negative_monitor{-1280, -720, 0, 0};
    const POINT giant_position = control_position_in_work_area(
        {-30, -30}, negative_monitor, 137, 1217);
    check(giant_position.x == -137 && giant_position.y == -720,
          "an oversized bar keeps its origin reachable on negative-coordinate monitors");
    const POINT oversized_position = control_position_in_work_area(
        {900, 600}, {0, 0, 320, 240}, 522, 504);
    check(oversized_position.x == 0 && oversized_position.y == 0,
          "oversized controls never use inverted position bounds");

    expected.palette_size = 99;
    check(store.save(expected) && store.load().palette_size == 5,
          "unknown future sizes clamp to the largest supported size");

    const std::array<LONG, 6> figure_widths{293, 366, 458, 549, 732, 1098};
    const std::array<LONG, 6> figure_heights{186, 232, 290, 348, 464, 696};
    expected.palette_size = 3;
    for (std::size_t index = 0; index < kGeometryPanelScales.size(); ++index) {
        expected.geometry_size = static_cast<int>(index);
        check(store.save(expected) && store.load().geometry_size == static_cast<int>(index) &&
              store.load().palette_size == 3,
              "each figures size persists independently of the painter palette");
        const auto size = geometry_panel_pixel_size(static_cast<int>(index));
        check(size.cx == figure_widths[index] && size.cy == figure_heights[index],
              "all six figures sizes scale the entire panel");
        const float scale = kGeometryPanelScales[index];
        const auto point = geometry_panel_logical_point(
            {static_cast<LONG>(std::lround(260 * scale)),
             static_cast<LONG>(std::lround(22 * scale))}, static_cast<int>(index));
        check(point_in_rect(point, kGeometryPinBounds),
              "scaled pin hit testing uses the same logical layout as rendering");
    }
    expected.geometry_size = 99;
    check(store.save(expected) && store.load().geometry_size == 5,
          "unknown future figures sizes clamp to giant");
    expected.geometry_size = -1;
    check(store.save(expected) && store.load().geometry_size == 0,
          "negative figures sizes clamp to compact");

    if (failures == 0) std::cout << "Elite Pen preferences: all tests passed\n";
    return failures == 0 ? 0 : 1;
}
