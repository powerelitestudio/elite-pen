#include "preferences.hpp"

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
    return end != buffer ? value : fallback;
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
    result.confirm_clear = read_int(path_, L"ConfirmClear", 0) != 0;
    result.exclude_palette_from_capture = read_int(path_, L"ExcludePaletteFromCapture", 1) != 0;
    result.start_in_interact_mode = read_int(path_, L"StartInInteractMode", 0) != 0;
    result.highlight_cursor = read_int(path_, L"HighlightCursor", 0) != 0;
    result.fade_seconds = std::clamp(read_int(path_, L"FadeSeconds", 0), 0, 60);
    result.has_palette_position = read_int(path_, L"HasPalettePosition", 0) != 0;
    result.palette_x = read_int(path_, L"PaletteX", 0);
    result.palette_y = read_int(path_, L"PaletteY", 0);
    result.zoom_factor = std::clamp(read_float(path_, L"ZoomFactor", 2.0F), 1.25F, 8.0F);
    result.zoom_view = std::clamp(read_int(path_, L"ZoomView", 0), 0, 2);
    result.zoom_invert = read_int(path_, L"ZoomInvert", 0) != 0;
    result.thickness = std::clamp(read_float(path_, L"Thickness", 7.0F), 1.0F, 40.0F);
    result.color = read_color(path_, kBlack);
    return result;
}

bool PreferencesStore::save(const Preferences& preferences) const {
    const std::filesystem::path target(path_);
    std::error_code error;
    std::filesystem::create_directories(target.parent_path(), error);
    if (error) return false;

    std::ostringstream content;
    content << "[ElitePen]\r\n";
    content << "ConfirmClear=" << (preferences.confirm_clear ? 1 : 0) << "\r\n";
    content << "ExcludePaletteFromCapture="
            << (preferences.exclude_palette_from_capture ? 1 : 0) << "\r\n";
    content << "StartInInteractMode=" << (preferences.start_in_interact_mode ? 1 : 0) << "\r\n";
    content << "HighlightCursor=" << (preferences.highlight_cursor ? 1 : 0) << "\r\n";
    content << "FadeSeconds=" << preferences.fade_seconds << "\r\n";
    content << "HasPalettePosition=" << (preferences.has_palette_position ? 1 : 0) << "\r\n";
    content << "PaletteX=" << preferences.palette_x << "\r\n";
    content << "PaletteY=" << preferences.palette_y << "\r\n";
    content << "ZoomFactor=" << preferences.zoom_factor << "\r\n";
    content << "ZoomView=" << preferences.zoom_view << "\r\n";
    content << "ZoomInvert=" << (preferences.zoom_invert ? 1 : 0) << "\r\n";
    content << "Thickness=" << preferences.thickness << "\r\n";
    content << "Color=" << static_cast<int>(preferences.color.r) << ','
            << static_cast<int>(preferences.color.g) << ','
            << static_cast<int>(preferences.color.b) << "\r\n";

    const std::filesystem::path temporary = target.wstring() + L".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) return false;
        stream << content.str();
        stream.flush();
        if (!stream) return false;
    }
    return MoveFileExW(temporary.c_str(), target.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

}  // namespace elite_pen::win
