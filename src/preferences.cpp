#include "preferences.hpp"
#include "color_values.hpp"
#include <map>
#include <locale>

#include <shlobj.h>

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>

namespace elite_pen::win {

namespace {

std::wstring executable_directory() {
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path().wstring();
}

std::wstring local_data_directory() {
    wchar_t path[MAX_PATH]{};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE,
                                   nullptr, SHGFP_TYPE_CURRENT, path))) {
        return (std::filesystem::path(path) / L"Power Elite Studio" / L"Elite Pen").wstring();
    }
    return (std::filesystem::path(executable_directory()) / L"data").wstring();
}

int read_int(const std::wstring& path, const wchar_t* key, int fallback) {
    return static_cast<int>(GetPrivateProfileIntW(L"ElitePen", key, fallback, path.c_str()));
}

float read_float(const std::wstring& path, const wchar_t* key, float fallback) {
    wchar_t buffer[64]{};
    GetPrivateProfileStringW(L"ElitePen", key, L"", buffer,
                             static_cast<DWORD>(std::size(buffer)), path.c_str());
    if (buffer[0] == L'\0') return fallback;
    wchar_t* end = nullptr;
    const float value = std::wcstof(buffer, &end);
    return end != buffer && std::isfinite(value) ? value : fallback;
}

Color read_color(const std::wstring& path, Color fallback) {
    wchar_t buffer[64]{};
    GetPrivateProfileStringW(L"ElitePen", L"Color", L"", buffer,
                             static_cast<DWORD>(std::size(buffer)), path.c_str());
    int red = 0;
    int green = 0;
    int blue = 0;
    if (swscanf_s(buffer, L"%d,%d,%d", &red, &green, &blue) != 3) return fallback;
    return {static_cast<std::uint8_t>(std::clamp(red, 0, 255)),
            static_cast<std::uint8_t>(std::clamp(green, 0, 255)),
            static_cast<std::uint8_t>(std::clamp(blue, 0, 255)), 255};
}

HotkeyBinding read_hotkey(const std::wstring& path, const wchar_t* key,
                          HotkeyBinding fallback) {
    wchar_t buffer[64]{};
    GetPrivateProfileStringW(L"ElitePen", key, L"", buffer,
                             static_cast<DWORD>(std::size(buffer)), path.c_str());
    unsigned int modifiers = 0;
    unsigned int virtual_key = 0;
    if (swscanf_s(buffer, L"%u,%u", &modifiers, &virtual_key) != 2) return fallback;
    constexpr UINT allowed_modifiers = MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN;
    if ((modifiers & ~allowed_modifiers) != 0 || virtual_key > 0xFEU)
        return fallback;
    if (virtual_key == 0) return modifiers == 0 ? HotkeyBinding{} : fallback;
    return {modifiers, virtual_key};
}

RectF read_focus_rect(const std::wstring& path, const std::wstring& key) {
    wchar_t buffer[160]{};
    GetPrivateProfileStringW(L"ElitePen", key.c_str(), L"", buffer,
        static_cast<DWORD>(std::size(buffer)), path.c_str());
    int left{}, top{}, width{}, height{};
    if (swscanf_s(buffer, L"%d,%d,%d,%d", &left, &top, &width, &height) != 4 ||
        std::abs(static_cast<double>(left)) > 1000000 ||
        std::abs(static_cast<double>(top)) > 1000000 ||
        width < 8 || width > 100000 || height < 8 || height > 100000) return {};
    const RectF result{static_cast<float>(left), static_cast<float>(top),
        static_cast<float>(left) + static_cast<float>(width),
        static_cast<float>(top) + static_cast<float>(height)};
    return valid_focus_rect(result) ? result : RectF{};
}

FocusScene read_focus_scene(const std::wstring& path, const std::wstring& prefix) {
    FocusScene result;
    result.area = read_focus_rect(path, prefix + L"Area");
    const int count = std::clamp(read_int(path, (prefix + L"MaskCount").c_str(), 0),
        0, static_cast<int>(kFocusMaskCount));
    for (int index = 0; index < count; ++index) {
        const auto mask = read_focus_rect(path, prefix + L"Mask" + std::to_wstring(index + 1));
        if (valid_focus_rect(mask)) result.masks[result.mask_count++] = mask;
    }
    return result;
}

void write_focus_scene(std::ostream& stream, const std::string& prefix, const FocusScene& scene) {
    const auto write_rect = [&](const std::string& key, RectF rect) {
        if (!valid_focus_rect(rect)) rect = {};
        stream << key << '=' << std::lround(rect.left) << ',' << std::lround(rect.top)
            << ',' << std::lround(rect.width()) << ',' << std::lround(rect.height()) << "\r\n";
    };
    write_rect(prefix + "Area", scene.area);
    const auto count = std::min(scene.mask_count, kFocusMaskCount);
    stream << prefix << "MaskCount=" << count << "\r\n";
    for (std::size_t index = 0; index < count; ++index)
        write_rect(prefix + "Mask" + std::to_string(index + 1), scene.masks[index]);
}

}  // namespace

PreferencesStore::PreferencesStore() {
    const auto executable = std::filesystem::path(executable_directory());
    portable_ = std::filesystem::exists(executable / L"portable.flag");
    const auto directory = portable_ ? executable / L"data"
                                     : std::filesystem::path(local_data_directory());
    path_ = (directory / L"settings.ini").wstring();
}

Preferences PreferencesStore::load() const {
    Preferences result;
    if (!std::filesystem::exists(path_)) return result;
    result.hotkey_scheme_version = std::clamp(
        read_int(path_, L"HotkeySchemeVersion", 1), 1,
        kCurrentHotkeySchemeVersion);
    result.theme = static_cast<AppTheme>(
        std::clamp(read_int(path_, L"Theme", 1), 0, 1));
    result.control_mode = static_cast<ControlMode>(
        std::clamp(read_int(path_, L"ControlMode", 0), 0, 1));
    result.confirm_clear = read_int(path_, L"ConfirmClear", 0) != 0;
    result.exclude_palette_from_capture = read_int(path_, L"ExcludePaletteFromCapture", 1) != 0;
    result.start_in_interact_mode = read_int(path_, L"StartInInteractMode", 0) != 0;
    result.highlight_cursor = read_int(path_, L"HighlightCursor", 0) != 0;
    result.fade_seconds = std::clamp(read_int(path_, L"FadeSeconds", 0), 0, 60);
    result.has_palette_position = read_int(path_, L"HasPalettePosition", 0) != 0;
    result.palette_x = read_int(path_, L"PaletteX", 0);
    result.palette_y = read_int(path_, L"PaletteY", 0);
    result.palette_size = std::clamp(read_int(path_, L"PaletteSize", 1), 0,
                                    static_cast<int>(kPaletteScales.size()) - 1);
    result.palette_collapsed = read_int(path_, L"PaletteCollapsed", 0) != 0;
    result.geometry_size = std::clamp(read_int(path_, L"GeometrySize", 1), 0,
                                     static_cast<int>(kGeometryPanelScales.size()) - 1);
    result.geometry_pinned = read_int(path_, L"GeometryPinned", 0) != 0;
    result.geometry_visible = read_int(path_, L"GeometryVisible", 0) != 0;
    result.has_geometry_position = read_int(path_, L"HasGeometryPosition", 0) != 0;
    result.geometry_x = read_int(path_, L"GeometryX", 0);
    result.geometry_y = read_int(path_, L"GeometryY", 0);
    result.emoji_size = std::clamp(read_int(path_, L"EmojiSize", 1), 0,
                                  static_cast<int>(kGeometryPanelScales.size()) - 1);
    result.emoji_pinned = read_int(path_, L"EmojiPinned", 0) != 0;
    result.emoji_visible = read_int(path_, L"EmojiVisible", 0) != 0;
    result.has_emoji_position = read_int(path_, L"HasEmojiPosition", 0) != 0;
    result.emoji_x = read_int(path_, L"EmojiX", 0);
    result.emoji_y = read_int(path_, L"EmojiY", 0);
    const int selected = read_int(path_, L"SelectedEmoji", 0);
    result.selected_emoji = valid_emoji(selected) ? selected : 0;
    for (std::size_t slot = 0; slot < kEmojiFavoriteCount; ++slot) {
        const auto key = L"EmojiFavorite" + std::to_wstring(slot + 1);
        const int id = read_int(path_, key.c_str(), kDefaultEmojiFavorites[slot]);
        result.emoji_favorites[slot] = valid_emoji(id) || id == -1
            ? id : kDefaultEmojiFavorites[slot];
    }
    result.focus_last = read_focus_scene(path_, L"FocusLast");
    for (std::size_t slot = 0; slot < kFocusZoneCount; ++slot)
        result.focus_zones[slot] = read_focus_scene(path_, L"FocusZone" + std::to_wstring(slot + 1));
    result.focus_duration_ms = std::clamp(read_int(path_, L"FocusDurationMs", 280), 120, 600);
    result.focus_isolation = read_int(path_, L"FocusIsolation", 1) != 0;
    result.focus_masks_visible = read_int(path_, L"FocusMasksVisible", 1) != 0;
    result.zoom_factor = std::clamp(read_float(path_, L"ZoomFactor", 2.0F), 1.25F, 8.0F);
    result.zoom_view = std::clamp(read_int(path_, L"ZoomView", 0), 0, 2);
    result.zoom_lens_diameter = std::clamp(
        read_int(path_, L"ZoomLensDiameter", 520), 280, 960);
    result.zoom_invert = read_int(path_, L"ZoomInvert", 0) != 0;
    result.thickness = std::clamp(read_float(path_, L"Thickness", 4.0F), 1.0F, 40.0F);
    result.color = read_color(path_, kBlack);
    wchar_t colors[160]{};
    GetPrivateProfileStringW(L"ElitePen", L"CustomColors", L"", colors, 160, path_.c_str());
    std::wistringstream swatches(colors);
    std::wstring value;
    while (std::getline(swatches, value, L';') && result.custom_colors.size() < kCustomColorLimit) {
        if (auto c = parse_color_hex(value); c &&
            std::find(result.custom_colors.begin(), result.custom_colors.end(), *c) == result.custom_colors.end())
            result.custom_colors.push_back(*c);
    }
    result.board_text_size = std::clamp(read_float(path_, L"BoardTextSize", 28), 8.0F, 256.0F);
    result.board_auto_text = read_int(path_, L"BoardAutoText", 0) != 0;
    // The legacy profile API reads UTF-8 INI text as ANSI. Decode font names explicitly.
    std::ifstream font_stream{std::filesystem::path(path_)};
    std::string line;
    while (std::getline(font_stream, line)) {
        if (!line.starts_with("BoardFont=")) continue;
        auto font = line.substr(10);
        if (!font.empty() && font.back() == '\r') font.pop_back();
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, font.data(),
                                             static_cast<int>(font.size()), nullptr, 0);
        if (count > 0 && count < 128) {
            std::wstring wide(static_cast<std::size_t>(count), L'\0');
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, font.data(), static_cast<int>(font.size()), wide.data(), count);
            result.board_font = std::move(wide);
        }
    }
    constexpr std::array<const wchar_t*, kHotkeyActionCount> hotkey_keys{{
        L"HotkeyInteract", L"HotkeyVisibility", L"HotkeyWhiteboard", L"HotkeyUndo",
        L"HotkeyRedo", L"HotkeyClear", L"HotkeyZoom", L"HotkeyBlackboard",
        L"HotkeyPen", L"HotkeyHighlighter", L"HotkeyEraser", L"HotkeyLine",
        L"HotkeyRectangle", L"HotkeyEllipse", L"HotkeyArrow", L"HotkeyCurvedArrow",
        L"HotkeyText", L"HotkeyScreenshot", L"HotkeyColorPanel", L"HotkeyGeometryPanel",
        L"HotkeyToolPanel", L"HotkeySettings", L"HotkeyPaletteCollapse",
        L"HotkeyColorBlack", L"HotkeyColorYellow", L"HotkeyColorBlue",
        L"HotkeyColorRed", L"HotkeyColorGreen", L"HotkeyColorPurple",
        L"HotkeyColorPanelAlternate",
        L"HotkeyPentagon", L"HotkeyHexagon",
        L"HotkeyCross", L"HotkeyCheckmark", L"HotkeyWarning", L"HotkeyProhibition",
        L"HotkeyEmojiPanel", L"HotkeyEmojiFavorite1", L"HotkeyEmojiFavorite2",
        L"HotkeyEmojiFavorite3", L"HotkeyEmojiFavorite4", L"HotkeyEmojiFavorite5",
        L"HotkeyEmojiFavorite6", L"HotkeyEmojiFavorite7", L"HotkeyEmojiFavorite8",
        L"HotkeyFocusToggle",
        L"HotkeyFocusSelect",
        L"HotkeyFocusMask",
        L"HotkeyFocusMasksToggle",
        L"HotkeyFocusIsolation",
        L"HotkeyFocusOverview",
        L"HotkeyFocusNext",
        L"HotkeyFocusPrevious",
        L"HotkeyFocusRecall1",
        L"HotkeyFocusRecall2",
        L"HotkeyFocusRecall3",
        L"HotkeyFocusRecall4",
        L"HotkeyFocusRecall5",
        L"HotkeyFocusRecall6",
        L"HotkeyFocusRecall7",
        L"HotkeyFocusRecall8",
        L"HotkeyFocusSave1",
        L"HotkeyFocusSave2",
        L"HotkeyFocusSave3",
        L"HotkeyFocusSave4",
        L"HotkeyFocusSave5",
        L"HotkeyFocusSave6",
        L"HotkeyFocusSave7",
        L"HotkeyFocusSave8",
        L"HotkeyBoardOpen",
        L"HotkeyZoomFreeze", L"HotkeyZoomFullscreen", L"HotkeyZoomLens",
        L"HotkeyZoomDocked", L"HotkeyZoomCycleView", L"HotkeyZoomInvert",
        L"HotkeyZoomOverview", L"HotkeyZoomIn", L"HotkeyZoomOut",
        L"HotkeyZoomEdit",
        L"HotkeyBoardPen", L"HotkeyBoardHighlighter", L"HotkeyBoardEraser", L"HotkeyBoardSelect", L"HotkeyBoardHand", L"HotkeyBoardFullscreen", L"HotkeyBoardBackground", L"HotkeyBoardInkToText"
    }};
    for (std::size_t index = 0; index < hotkey_keys.size(); ++index) {
        result.hotkeys[index] = read_hotkey(path_, hotkey_keys[index],
                                             kDefaultHotkeys[index]);
    }
    if (result.hotkey_scheme_version < kCurrentHotkeySchemeVersion) {
        for (std::size_t index = 0; index < result.hotkeys.size(); ++index) {
            if (result.hotkeys[index] == kLegacyDefaultHotkeysV1[index]) {
                result.hotkeys[index] = kDefaultHotkeys[index];
            }
        }
        result.hotkey_scheme_version = kCurrentHotkeySchemeVersion;
    }
    // Missing new defaults must never displace an existing user's combination.
    for (std::size_t index = static_cast<std::size_t>(HotkeyAction::FocusToggle);
         index < static_cast<std::size_t>(HotkeyAction::ZoomFreeze); ++index) {
        wchar_t saved[64]{};
        GetPrivateProfileStringW(L"ElitePen", hotkey_keys[index], L"", saved,
            static_cast<DWORD>(std::size(saved)), path_.c_str());
        if (saved[0] || !result.hotkeys[index].virtual_key) continue;
        for (std::size_t other = 0; other < kHotkeyActionCount; ++other) {
            if (other >= static_cast<std::size_t>(HotkeyAction::FocusToggle) &&
                other < static_cast<std::size_t>(HotkeyAction::ZoomFreeze)) continue;
            if (result.hotkeys[index] == result.hotkeys[other]) {
                result.hotkeys[index] = {};
                break;
            }
        }
    }
    for (std::size_t index = kBoardHotkeyBegin; index < kHotkeyActionCount; ++index) {
        if (reserved_board_binding(result.hotkeys[index])) result.hotkeys[index] = {};
        for (std::size_t other = 0; other < index && result.hotkeys[index].virtual_key; ++other)
            if (hotkey_scopes_overlap(index, other) && result.hotkeys[index] == result.hotkeys[other])
                result.hotkeys[index] = {};
    }
    return result;
}

namespace {
std::string preferences_ini(const Preferences& preferences) {
    std::ostringstream content;
    content.imbue(std::locale::classic());
    content << "[ElitePen]\r\n";
    content << "HotkeySchemeVersion=" << preferences.hotkey_scheme_version << "\r\n";
    content << "Theme=" << static_cast<int>(preferences.theme) << "\r\n";
    content << "ControlMode=" << static_cast<int>(preferences.control_mode) << "\r\n";
    content << "ConfirmClear=" << (preferences.confirm_clear ? 1 : 0) << "\r\n";
    content << "ExcludePaletteFromCapture="
            << (preferences.exclude_palette_from_capture ? 1 : 0) << "\r\n";
    content << "StartInInteractMode=" << (preferences.start_in_interact_mode ? 1 : 0) << "\r\n";
    content << "HighlightCursor=" << (preferences.highlight_cursor ? 1 : 0) << "\r\n";
    content << "FadeSeconds=" << preferences.fade_seconds << "\r\n";
    content << "HasPalettePosition=" << (preferences.has_palette_position ? 1 : 0) << "\r\n";
    content << "PaletteX=" << preferences.palette_x << "\r\n";
    content << "PaletteY=" << preferences.palette_y << "\r\n";
    content << "PaletteSize=" << preferences.palette_size << "\r\n";
    content << "PaletteCollapsed=" << (preferences.palette_collapsed ? 1 : 0) << "\r\n";
    content << "GeometrySize=" << preferences.geometry_size << "\r\n";
    content << "GeometryPinned=" << (preferences.geometry_pinned ? 1 : 0) << "\r\n";
    content << "GeometryVisible=" << (preferences.geometry_visible ? 1 : 0) << "\r\n";
    content << "HasGeometryPosition=" << (preferences.has_geometry_position ? 1 : 0) << "\r\n";
    content << "GeometryX=" << preferences.geometry_x << "\r\n";
    content << "GeometryY=" << preferences.geometry_y << "\r\n";
    content << "EmojiSize=" << preferences.emoji_size << "\r\n";
    content << "EmojiPinned=" << (preferences.emoji_pinned ? 1 : 0) << "\r\n";
    content << "EmojiVisible=" << (preferences.emoji_visible ? 1 : 0) << "\r\n";
    content << "HasEmojiPosition=" << (preferences.has_emoji_position ? 1 : 0) << "\r\n";
    content << "EmojiX=" << preferences.emoji_x << "\r\n";
    content << "EmojiY=" << preferences.emoji_y << "\r\n";
    content << "SelectedEmoji=" << preferences.selected_emoji << "\r\n";
    for (std::size_t slot = 0; slot < kEmojiFavoriteCount; ++slot)
        content << "EmojiFavorite" << slot + 1 << '=' << preferences.emoji_favorites[slot] << "\r\n";
    write_focus_scene(content, "FocusLast", preferences.focus_last);
    for (std::size_t slot = 0; slot < kFocusZoneCount; ++slot)
        write_focus_scene(content, "FocusZone" + std::to_string(slot + 1), preferences.focus_zones[slot]);
    content << "FocusDurationMs=" << preferences.focus_duration_ms << "\r\n";
    content << "FocusIsolation=" << (preferences.focus_isolation ? 1 : 0) << "\r\n";
    content << "FocusMasksVisible=" << (preferences.focus_masks_visible ? 1 : 0) << "\r\n";
    content << "ZoomFactor=" << preferences.zoom_factor << "\r\n";
    content << "ZoomView=" << preferences.zoom_view << "\r\n";
    content << "ZoomLensDiameter=" << preferences.zoom_lens_diameter << "\r\n";
    content << "ZoomInvert=" << (preferences.zoom_invert ? 1 : 0) << "\r\n";
    content << "Thickness=" << preferences.thickness << "\r\n";
    content << "Color=" << static_cast<int>(preferences.color.r) << ','
            << static_cast<int>(preferences.color.g) << ','
            << static_cast<int>(preferences.color.b) << "\r\n";
    content << "CustomColors=";
    for (std::size_t n = 0; n < std::min(preferences.custom_colors.size(), kCustomColorLimit); ++n) {
        auto hex = color_hex(preferences.custom_colors[n]);
        content << (n ? ";" : "") << std::string(hex.begin(), hex.end());
    }
    content << "\r\nBoardTextSize=" << preferences.board_text_size
            << "\r\nBoardAutoText=" << (preferences.board_auto_text ? 1 : 0) << "\r\nBoardFont=";
    std::wstring font = preferences.board_font;
    if (font.empty() || font.size() >= 128 || font.find_first_of(L"\r\n") != std::wstring::npos) font = L"Palitoon";
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, font.data(), static_cast<int>(font.size()), nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<std::size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, font.data(), static_cast<int>(font.size()), utf8.data(), bytes, nullptr, nullptr);
    content << utf8 << "\r\n";
    constexpr std::array<const char*, kHotkeyActionCount> hotkey_keys{{
        "HotkeyInteract", "HotkeyVisibility", "HotkeyWhiteboard", "HotkeyUndo",
        "HotkeyRedo", "HotkeyClear", "HotkeyZoom", "HotkeyBlackboard",
        "HotkeyPen", "HotkeyHighlighter", "HotkeyEraser", "HotkeyLine",
        "HotkeyRectangle", "HotkeyEllipse", "HotkeyArrow", "HotkeyCurvedArrow",
        "HotkeyText", "HotkeyScreenshot", "HotkeyColorPanel", "HotkeyGeometryPanel",
        "HotkeyToolPanel", "HotkeySettings", "HotkeyPaletteCollapse",
        "HotkeyColorBlack", "HotkeyColorYellow", "HotkeyColorBlue",
        "HotkeyColorRed", "HotkeyColorGreen", "HotkeyColorPurple",
        "HotkeyColorPanelAlternate",
        "HotkeyPentagon", "HotkeyHexagon",
        "HotkeyCross", "HotkeyCheckmark", "HotkeyWarning", "HotkeyProhibition",
        "HotkeyEmojiPanel", "HotkeyEmojiFavorite1", "HotkeyEmojiFavorite2",
        "HotkeyEmojiFavorite3", "HotkeyEmojiFavorite4", "HotkeyEmojiFavorite5",
        "HotkeyEmojiFavorite6", "HotkeyEmojiFavorite7", "HotkeyEmojiFavorite8",
        "HotkeyFocusToggle",
        "HotkeyFocusSelect",
        "HotkeyFocusMask",
        "HotkeyFocusMasksToggle",
        "HotkeyFocusIsolation",
        "HotkeyFocusOverview",
        "HotkeyFocusNext",
        "HotkeyFocusPrevious",
        "HotkeyFocusRecall1",
        "HotkeyFocusRecall2",
        "HotkeyFocusRecall3",
        "HotkeyFocusRecall4",
        "HotkeyFocusRecall5",
        "HotkeyFocusRecall6",
        "HotkeyFocusRecall7",
        "HotkeyFocusRecall8",
        "HotkeyFocusSave1",
        "HotkeyFocusSave2",
        "HotkeyFocusSave3",
        "HotkeyFocusSave4",
        "HotkeyFocusSave5",
        "HotkeyFocusSave6",
        "HotkeyFocusSave7",
        "HotkeyFocusSave8",
        "HotkeyBoardOpen",
        "HotkeyZoomFreeze", "HotkeyZoomFullscreen", "HotkeyZoomLens",
        "HotkeyZoomDocked", "HotkeyZoomCycleView", "HotkeyZoomInvert",
        "HotkeyZoomOverview", "HotkeyZoomIn", "HotkeyZoomOut",
        "HotkeyZoomEdit",
        "HotkeyBoardPen", "HotkeyBoardHighlighter", "HotkeyBoardEraser", "HotkeyBoardSelect", "HotkeyBoardHand", "HotkeyBoardFullscreen", "HotkeyBoardBackground", "HotkeyBoardInkToText"
    }};
    for (std::size_t index = 0; index < hotkey_keys.size(); ++index) {
        content << hotkey_keys[index] << '=' << preferences.hotkeys[index].modifiers
                << ',' << preferences.hotkeys[index].virtual_key << "\r\n";
    }

    return content.str();
}
bool write_settings_atomic(const std::wstring& path, const std::string& content) {
    const std::filesystem::path target(path);
    std::error_code error;
    if (!target.parent_path().empty()) std::filesystem::create_directories(target.parent_path(), error);
    if (error) return false;
    const std::filesystem::path temporary = target.wstring() + L".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) return false;
        stream << content;
        stream.flush();
        if (!stream) return false;
    }
    return MoveFileExW(temporary.c_str(), target.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

} // namespace
bool PreferencesStore::save(const Preferences& preferences) const {
    return write_settings_atomic(path_, preferences_ini(preferences));
}
#include "preferences_transfer.inl"
}  // namespace elite_pen::win
