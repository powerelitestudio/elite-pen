#pragma once

#include "elite_pen/core.hpp"

#include <windows.h>

#include <array>
#include <cstdint>
#include <string>

namespace elite_pen::win {

enum class AppTheme : std::uint8_t {
    Dark = 0,
    Light = 1,
};

enum class ControlMode : std::uint8_t {
    Palette = 0,
    Linear = 1,
};

enum class HotkeyAction : std::uint8_t {
    Interact,
    Visibility,
    Whiteboard,
    Undo,
    Redo,
    Clear,
    Zoom,
    Blackboard,
    Pen,
    Highlighter,
    Eraser,
    Line,
    Rectangle,
    Ellipse,
    Arrow,
    CurvedArrow,
    Text,
    Screenshot,
    ColorPanel,
    GeometryPanel,
    ToolPanel,
    Settings,
    PaletteCollapse,
    ColorBlack,
    ColorYellow,
    ColorBlue,
    ColorRed,
    ColorGreen,
    ColorPurple,
    ColorPanelAlternate,
    ZoomFreeze,
    ZoomFullscreen,
    ZoomLens,
    ZoomDocked,
    ZoomCycleView,
    ZoomInvert,
    ZoomOverview,
    ZoomIn,
    ZoomOut,
    ZoomEdit,
    Count
};

struct HotkeyBinding {
    UINT modifiers{};
    UINT virtual_key{};

    constexpr bool operator==(const HotkeyBinding&) const noexcept = default;
};

inline constexpr std::size_t kHotkeyActionCount =
    static_cast<std::size_t>(HotkeyAction::Count);
inline constexpr std::size_t kGlobalHotkeyActionCount =
    static_cast<std::size_t>(HotkeyAction::ZoomFreeze);
inline constexpr int kCurrentHotkeySchemeVersion = 2;

inline constexpr std::array<HotkeyBinding, kHotkeyActionCount> kDefaultHotkeys{{
    {MOD_CONTROL | MOD_SHIFT, 'Q'},
    {MOD_CONTROL | MOD_SHIFT, 'A'},
    {MOD_CONTROL | MOD_SHIFT, 'W'},
    {MOD_CONTROL | MOD_ALT, 'Z'},
    {MOD_CONTROL | MOD_SHIFT, 'Y'},
    {MOD_CONTROL | MOD_SHIFT, 'E'},
    {MOD_CONTROL | MOD_SHIFT, 'Z'},
    {MOD_CONTROL | MOD_SHIFT, 'B'},
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {MOD_CONTROL | MOD_SHIFT, 'T'},
    {0, 0},
    {MOD_CONTROL | MOD_SHIFT, '7'},
    {MOD_CONTROL | MOD_SHIFT, 'G'},
    {0, 0}, {0, 0},
    {MOD_CONTROL | MOD_SHIFT, 'D'},
    {MOD_CONTROL | MOD_SHIFT, '1'},
    {MOD_CONTROL | MOD_SHIFT, '2'},
    {MOD_CONTROL | MOD_SHIFT, '3'},
    {MOD_CONTROL | MOD_SHIFT, '4'},
    {MOD_CONTROL | MOD_SHIFT, '5'},
    {MOD_CONTROL | MOD_SHIFT, '6'},
    {MOD_CONTROL | MOD_SHIFT, VK_OEM_PLUS},
    {0, 'P'},
    {0, 'F'},
    {0, 'L'},
    {0, 'D'},
    {0, VK_SPACE},
    {0, 'I'},
    {0, '0'},
    {0, VK_OEM_PLUS},
    {0, VK_OEM_MINUS},
    {0, 'E'}
}};

// Defaults shipped through 2.3. Existing installations are migrated only when
// a saved binding still matches one of these values; genuine customizations win.
inline constexpr std::array<HotkeyBinding, kHotkeyActionCount> kLegacyDefaultHotkeysV1{{
    {MOD_CONTROL | MOD_SHIFT, 'P'},
    {MOD_CONTROL | MOD_SHIFT, 'H'},
    {MOD_CONTROL | MOD_SHIFT, 'W'},
    {MOD_CONTROL | MOD_SHIFT, 'Z'},
    {MOD_CONTROL | MOD_SHIFT, 'Y'},
    {MOD_CONTROL | MOD_SHIFT, 'C'},
    {MOD_CONTROL | MOD_SHIFT, 'M'},
    {MOD_CONTROL | MOD_SHIFT, 'B'},
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {MOD_CONTROL | MOD_SHIFT, 'T'},
    {0, 0}, {0, 0},
    {MOD_CONTROL | MOD_SHIFT, 'G'},
    {0, 0}, {0, 0}, {0, 0},
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {0, 'P'},
    {0, 'F'},
    {0, 'L'},
    {0, 'D'},
    {0, VK_SPACE},
    {0, 'I'},
    {0, '0'},
    {0, VK_OEM_PLUS},
    {0, VK_OEM_MINUS},
    {0, 'E'}
}};

struct Preferences {
    int hotkey_scheme_version{kCurrentHotkeySchemeVersion};
    AppTheme theme{AppTheme::Light};
    ControlMode control_mode{ControlMode::Palette};
    bool confirm_clear{false};
    bool exclude_palette_from_capture{true};
    bool start_in_interact_mode{false};
    bool highlight_cursor{false};
    int fade_seconds{0};
    bool has_palette_position{false};
    int palette_x{};
    int palette_y{};
    int palette_size{1};
    bool palette_collapsed{false};
    float zoom_factor{2.0F};
    int zoom_view{0};
    int zoom_lens_diameter{520};
    bool zoom_invert{false};
    float thickness{4.0F};
    Color color{kBlack};
    std::array<HotkeyBinding, kHotkeyActionCount> hotkeys{kDefaultHotkeys};
};

class PreferencesStore {
public:
    PreferencesStore();

    [[nodiscard]] Preferences load() const;
    bool save(const Preferences& preferences) const;
    [[nodiscard]] const std::wstring& path() const noexcept { return path_; }
    [[nodiscard]] bool portable() const noexcept { return portable_; }

private:
    std::wstring path_;
    bool portable_{};
};

}  // namespace elite_pen::win
