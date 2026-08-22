#include "application.hpp"

#include "elite_pen/core.hpp"
#include "control_layout.hpp"
#include "graphics.hpp"
#include "preferences.hpp"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <magnification.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <uxtheme.h>
#include <wincodec.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace elite_pen::win {

namespace {

constexpr UINT kTrayMessage = WM_APP + 10;
constexpr UINT kExitMessage = WM_APP + 12;
constexpr UINT kQaQueryToolMessage = WM_APP + 90;
constexpr UINT kQaQueryColorMessage = WM_APP + 91;
constexpr UINT kQaQueryThicknessMessage = WM_APP + 92;
constexpr UINT kQaCommitInlineTextMessage = WM_APP + 93;
constexpr UINT kQaQueryDocumentCountMessage = WM_APP + 94;
constexpr UINT kQaQueryDrawingCursorMessage = WM_APP + 95;
constexpr UINT kQaQueryPaletteCollapsedMessage = WM_APP + 96;
constexpr UINT kQaQueryZoomFrozenMessage = WM_APP + 97;
constexpr UINT kQaQueryZoomDocumentCountMessage = WM_APP + 98;
constexpr UINT kQaQueryBoardModeMessage = WM_APP + 99;
constexpr UINT kQaQueryZoomSnapshotMessage = WM_APP + 100;
constexpr UINT kZoomClickFreezeMessage = WM_APP + 101;
constexpr UINT kQaQueryZoomSourceFocusXMessage = WM_APP + 102;
constexpr UINT kQaQueryZoomSourceFocusYMessage = WM_APP + 103;
constexpr UINT kQaQueryThemeMessage = WM_APP + 104;
constexpr UINT kQaPopulateStressDocumentMessage = WM_APP + 105;
constexpr UINT kThicknessWheelMessage = WM_APP + 106;
constexpr UINT kQaQueryThicknessWheelRouteMessage = WM_APP + 107;
constexpr UINT kQaQueryGlobalHotkeysMessage = WM_APP + 108;
constexpr UINT kQaQueryZoomViewMessage = WM_APP + 109;
constexpr UINT kQaQueryZoomGeometryWidthMessage = WM_APP + 110;
constexpr UINT kQaToggleZoomFreezeMessage = WM_APP + 111;
constexpr UINT kQaQueryControlModeMessage = WM_APP + 112;
constexpr UINT kQaSetControlModeMessage = WM_APP + 113;
constexpr UINT kQaQueryZoomEditStateMessage = WM_APP + 114;
constexpr UINT kQaQueryZoomEditDocumentCountMessage = WM_APP + 115;
constexpr UINT kQaSetZoomEditStateMessage = WM_APP + 116;
constexpr UINT kQaPopulateZoomEditDocumentMessage = WM_APP + 117;
constexpr UINT kQaQueryZoomEditFirstViewXMessage = WM_APP + 118;
constexpr UINT kQaQueryZoomEditFirstViewYMessage = WM_APP + 119;
constexpr UINT kQaNudgeZoomEditSourceMessage = WM_APP + 120;
constexpr UINT kQaQueryZoomFactorMessage = WM_APP + 121;
constexpr UINT kQaQueryZoomPresentedFactorMessage = WM_APP + 122;
constexpr UINT kQaQueryZoomEntryAnimationMessage = WM_APP + 123;
constexpr UINT kQaQueryZoomGeometryHeightMessage = WM_APP + 124;
constexpr UINT kQaQueryZoomLensDiameterMessage = WM_APP + 125;
constexpr UINT_PTR kTrayId = 1;
constexpr std::array<float, 4> kPaletteScales{0.48F, 0.60F, 0.75F, 0.90F};
constexpr std::array<float, 5> kThicknessSteps{2.0F, 4.0F, 7.0F, 12.0F, 20.0F};
constexpr std::array<int, 5> kZoomLensDiameters{360, 440, 520, 640, 760};

struct UiTheme {
    bool light{};
    std::uint32_t background{};
    std::uint32_t surface_1{};
    std::uint32_t surface_2{};
    std::uint32_t surface_3{};
    std::uint32_t surface_4{};
    std::uint32_t text{};
    std::uint32_t text_soft{};
    std::uint32_t text_muted{};
    std::uint32_t violet{};
    std::uint32_t violet_strong{};
    std::uint32_t mint{};
    std::uint32_t danger{};
    std::uint32_t warning{};
    std::uint32_t line{};
    std::uint32_t shadow{};
};

constexpr UiTheme kDarkTheme{
    false, 0x080A10, 0x0C0F17, 0x11151F, 0x171C28, 0x202636,
    0xF4F5F8, 0xA9B0BF, 0x6E7688, 0x917CFF, 0x7B64F7, 0x55DEC0,
    0xFF6F87, 0xF1B86A, 0x363D4D, 0x000000};
constexpr UiTheme kLightTheme{
    true, 0xEDF0F6, 0xF8F9FC, 0xFFFFFF, 0xE9EDF5, 0xDFE4EE,
    0x171B28, 0x4F586B, 0x7A8498, 0x6E55DF, 0x5E46CE, 0x0D9F85,
    0xD94B65, 0xB77719, 0xC7CDD9, 0x212A3E};

AppTheme g_ui_theme = AppTheme::Light;

const UiTheme& ui_theme(AppTheme theme) noexcept {
    return theme == AppTheme::Light ? kLightTheme : kDarkTheme;
}

const UiTheme& current_ui_theme() noexcept {
    return ui_theme(g_ui_theme);
}

bool process_is_running(const wchar_t* executable_name) {
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, executable_name) == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return found;
}

bool obs_is_running() {
    return process_is_running(L"obs64.exe") || process_is_running(L"obs32.exe");
}

D2D1_COLOR_F theme_color(std::uint32_t rgb, float alpha = 1.0F) noexcept {
    return D2D1::ColorF(
        static_cast<float>((rgb >> 16U) & 0xFFU) / 255.0F,
        static_cast<float>((rgb >> 8U) & 0xFFU) / 255.0F,
        static_cast<float>(rgb & 0xFFU) / 255.0F, alpha);
}

COLORREF theme_colorref(std::uint32_t rgb) noexcept {
    return RGB((rgb >> 16U) & 0xFFU, (rgb >> 8U) & 0xFFU, rgb & 0xFFU);
}

float palette_scale_for_size(int size) noexcept {
    return kPaletteScales[static_cast<std::size_t>(
        std::clamp(size, 0, static_cast<int>(kPaletteScales.size()) - 1))];
}

struct CursorPoint {
    float x{};
    float y{};
};

struct CursorColor {
    float r{};
    float g{};
    float b{};
    float a{};
};

CursorColor cursor_theme_color(std::uint32_t rgb, float alpha = 1.0F) noexcept {
    return {static_cast<float>((rgb >> 16U) & 0xFFU) / 255.0F,
            static_cast<float>((rgb >> 8U) & 0xFFU) / 255.0F,
            static_cast<float>(rgb & 0xFFU) / 255.0F, alpha};
}

bool cursor_point_in_polygon(CursorPoint point, const std::vector<CursorPoint>& polygon) {
    bool inside = false;
    for (std::size_t current = 0, previous = polygon.size() - 1;
         current < polygon.size(); previous = current++) {
        const CursorPoint a = polygon[current];
        const CursorPoint b = polygon[previous];
        const bool crosses = ((a.y > point.y) != (b.y > point.y)) &&
            (point.x < (b.x - a.x) * (point.y - a.y) /
                ((b.y - a.y) == 0.0F ? 0.0001F : (b.y - a.y)) + a.x);
        if (crosses) inside = !inside;
    }
    return inside;
}

std::vector<CursorPoint> cursor_rectangle(CursorPoint start, CursorPoint finish,
                                          CursorPoint normal, float half_width) {
    return {
        {start.x - normal.x * half_width, start.y - normal.y * half_width},
        {finish.x - normal.x * half_width, finish.y - normal.y * half_width},
        {finish.x + normal.x * half_width, finish.y + normal.y * half_width},
        {start.x + normal.x * half_width, start.y + normal.y * half_width}
    };
}

std::vector<CursorPoint> cursor_tip(CursorPoint tip, CursorPoint base,
                                    CursorPoint normal, float half_width) {
    return {
        tip,
        {base.x - normal.x * half_width, base.y - normal.y * half_width},
        {base.x + normal.x * half_width, base.y + normal.y * half_width}
    };
}

HCURSOR create_pencil_cursor(UINT dpi) {
    using GetSystemMetricsForDpiFunction = int(WINAPI*)(int, UINT);
    const auto metrics_for_dpi = reinterpret_cast<GetSystemMetricsForDpiFunction>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetSystemMetricsForDpi"));
    const auto metric = [metrics_for_dpi, dpi](int index) {
        if (metrics_for_dpi) return metrics_for_dpi(index, dpi);
        return MulDiv(GetSystemMetrics(index), static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
    };
    const int width = std::clamp(metric(SM_CXCURSOR), 32, 96);
    const int height = std::clamp(metric(SM_CYCURSOR), 32, 96);
    constexpr int sample_grid = 4;
    constexpr float logical_size = 32.0F;
    constexpr float diagonal = 0.70710678F;
    // Screen Y grows downwards. Extending the pencil from its graphite tip in
    // (+X, +Y) therefore presents it at -45 degrees in Cartesian coordinates:
    // the body stays below the insertion point instead of covering the text
    // immediately above a left-to-right underline.
    const CursorPoint direction{diagonal, diagonal};
    const CursorPoint normal{-diagonal, diagonal};
    const CursorPoint tip{2.5F, 2.5F};
    const CursorPoint wood_base{tip.x + direction.x * 8.5F,
                                tip.y + direction.y * 8.5F};
    const CursorPoint finish{27.0F, 27.0F};
    const auto along = [direction](CursorPoint origin, float amount) {
        return CursorPoint{origin.x + direction.x * amount,
                           origin.y + direction.y * amount};
    };
    const CursorPoint metal_start = along(finish, -7.0F);
    const CursorPoint eraser_start = along(finish, -4.0F);

    const auto outer_shaft = cursor_rectangle(wood_base, finish, normal, 5.0F);
    const auto outer_tip = cursor_tip(tip, wood_base, normal, 5.0F);
    const auto outline_shaft = cursor_rectangle(wood_base, finish, normal, 4.25F);
    const auto outline_tip = cursor_tip(tip, wood_base, normal, 4.25F);
    const auto wood = cursor_tip(tip, wood_base, normal, 3.35F);
    const CursorPoint graphite_base = along(tip, 3.6F);
    const auto graphite = cursor_tip(tip, graphite_base, normal, 1.45F);
    const auto body = cursor_rectangle(wood_base, metal_start, normal, 3.35F);
    const auto metal = cursor_rectangle(metal_start, eraser_start, normal, 3.35F);
    const auto eraser = cursor_rectangle(eraser_start, finish, normal, 3.35F);
    const CursorPoint highlight_start{
        wood_base.x - normal.x * 1.75F + direction.x * 0.9F,
        wood_base.y - normal.y * 1.75F + direction.y * 0.9F};
    const CursorPoint highlight_finish{
        metal_start.x - normal.x * 1.75F - direction.x * 0.7F,
        metal_start.y - normal.y * 1.75F - direction.y * 0.7F};
    const auto highlight = cursor_rectangle(
        highlight_start, highlight_finish, normal, 0.55F);

    constexpr CursorColor transparent{};
    constexpr CursorColor halo{0.98F, 0.98F, 0.99F, 0.96F};
    constexpr CursorColor outline{0.07F, 0.08F, 0.10F, 1.0F};
    constexpr CursorColor wood_color{0.95F, 0.80F, 0.59F, 1.0F};
    constexpr CursorColor graphite_color{0.08F, 0.09F, 0.11F, 1.0F};
    constexpr CursorColor body_color{0.96F, 0.66F, 0.18F, 1.0F};
    constexpr CursorColor highlight_color{1.0F, 0.86F, 0.38F, 1.0F};
    constexpr CursorColor metal_color{0.72F, 0.76F, 0.82F, 1.0F};
    constexpr CursorColor eraser_color{0.94F, 0.34F, 0.42F, 1.0F};

    const auto sample_color = [&](CursorPoint point) {
        CursorColor color = transparent;
        if (cursor_point_in_polygon(point, outer_shaft) ||
            cursor_point_in_polygon(point, outer_tip)) color = halo;
        if (cursor_point_in_polygon(point, outline_shaft) ||
            cursor_point_in_polygon(point, outline_tip)) color = outline;
        if (cursor_point_in_polygon(point, wood)) color = wood_color;
        if (cursor_point_in_polygon(point, graphite)) color = graphite_color;
        if (cursor_point_in_polygon(point, body)) color = body_color;
        if (cursor_point_in_polygon(point, highlight)) color = highlight_color;
        if (cursor_point_in_polygon(point, metal)) color = metal_color;
        if (cursor_point_in_polygon(point, eraser)) color = eraser_color;
        return color;
    };

    std::vector<unsigned char> pixels(
        static_cast<std::size_t>(width * height * 4), 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float red{};
            float green{};
            float blue{};
            float alpha{};
            for (int sample_y = 0; sample_y < sample_grid; ++sample_y) {
                for (int sample_x = 0; sample_x < sample_grid; ++sample_x) {
                    const CursorPoint point{
                        (static_cast<float>(x) +
                         (static_cast<float>(sample_x) + 0.5F) / sample_grid) *
                            logical_size / static_cast<float>(width),
                        (static_cast<float>(y) +
                         (static_cast<float>(sample_y) + 0.5F) / sample_grid) *
                            logical_size / static_cast<float>(height)};
                    const CursorColor color = sample_color(point);
                    red += color.r * color.a;
                    green += color.g * color.a;
                    blue += color.b * color.a;
                    alpha += color.a;
                }
            }
            constexpr float samples = static_cast<float>(sample_grid * sample_grid);
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<unsigned char>(
                std::clamp(std::lround(blue / samples * 255.0F), 0L, 255L));
            pixels[offset + 1] = static_cast<unsigned char>(
                std::clamp(std::lround(green / samples * 255.0F), 0L, 255L));
            pixels[offset + 2] = static_cast<unsigned char>(
                std::clamp(std::lround(red / samples * 255.0F), 0L, 255L));
            pixels[offset + 3] = static_cast<unsigned char>(
                std::clamp(std::lround(alpha / samples * 255.0F), 0L, 255L));
        }
    }

    BITMAPV5HEADER header{};
    header.bV5Size = sizeof(header);
    header.bV5Width = width;
    header.bV5Height = -height;
    header.bV5Planes = 1;
    header.bV5BitCount = 32;
    header.bV5Compression = BI_BITFIELDS;
    header.bV5RedMask = 0x00ff0000;
    header.bV5GreenMask = 0x0000ff00;
    header.bV5BlueMask = 0x000000ff;
    header.bV5AlphaMask = 0xff000000;
    void* bitmap_bits{};
    HDC screen = GetDC(nullptr);
    HBITMAP color_bitmap = CreateDIBSection(screen,
        reinterpret_cast<const BITMAPINFO*>(&header), DIB_RGB_COLORS,
        &bitmap_bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!color_bitmap || !bitmap_bits) {
        if (color_bitmap) DeleteObject(color_bitmap);
        return nullptr;
    }
    std::memcpy(bitmap_bits, pixels.data(), pixels.size());
    HBITMAP mask_bitmap = CreateBitmap(width, height, 1, 1, nullptr);
    if (!mask_bitmap) {
        DeleteObject(color_bitmap);
        return nullptr;
    }
    ICONINFO information{};
    information.fIcon = FALSE;
    information.xHotspot = static_cast<DWORD>(std::clamp(
        std::lround(tip.x * static_cast<float>(width) / logical_size), 0L,
        static_cast<long>(width - 1)));
    information.yHotspot = static_cast<DWORD>(std::clamp(
        std::lround(tip.y * static_cast<float>(height) / logical_size), 0L,
        static_cast<long>(height - 1)));
    information.hbmMask = mask_bitmap;
    information.hbmColor = color_bitmap;
    HCURSOR cursor = static_cast<HCURSOR>(CreateIconIndirect(&information));
    DeleteObject(mask_bitmap);
    DeleteObject(color_bitmap);
    return cursor;
}

float cursor_distance_to_segment(CursorPoint point, CursorPoint start,
                                 CursorPoint finish) noexcept {
    const float dx = finish.x - start.x;
    const float dy = finish.y - start.y;
    const float length_squared = dx * dx + dy * dy;
    if (length_squared <= 0.0001F) {
        return std::hypot(point.x - start.x, point.y - start.y);
    }
    const float projection = std::clamp(
        ((point.x - start.x) * dx + (point.y - start.y) * dy) / length_squared,
        0.0F, 1.0F);
    return std::hypot(point.x - (start.x + projection * dx),
                      point.y - (start.y + projection * dy));
}

HCURSOR create_zoom_lens_cursor(UINT dpi) {
    const auto& theme = current_ui_theme();
    using GetSystemMetricsForDpiFunction = int(WINAPI*)(int, UINT);
    const auto metrics_for_dpi = reinterpret_cast<GetSystemMetricsForDpiFunction>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetSystemMetricsForDpi"));
    const auto metric = [metrics_for_dpi, dpi](int index) {
        if (metrics_for_dpi) return metrics_for_dpi(index, dpi);
        return MulDiv(GetSystemMetrics(index), static_cast<int>(dpi),
                      USER_DEFAULT_SCREEN_DPI);
    };
    const int width = std::clamp(metric(SM_CXCURSOR), 40, 96);
    const int height = std::clamp(metric(SM_CYCURSOR), 40, 96);
    constexpr int sample_grid = 4;
    constexpr float logical_size = 40.0F;
    constexpr CursorPoint center{16.0F, 16.0F};
    constexpr CursorPoint handle_start{24.0F, 24.0F};
    constexpr CursorPoint handle_finish{35.0F, 35.0F};
    constexpr float ring_radius = 11.0F;
    constexpr CursorColor transparent{};
    constexpr CursorColor halo{0.98F, 0.98F, 0.99F, 0.94F};
    constexpr CursorColor outline{0.05F, 0.07F, 0.10F, 1.0F};
    const CursorColor accent = cursor_theme_color(theme.violet);
    const CursorColor focus = cursor_theme_color(theme.mint);
    const CursorColor glass = cursor_theme_color(theme.violet, 0.16F);

    const auto sample_color = [&](CursorPoint point) {
        const float radial = std::hypot(point.x - center.x, point.y - center.y);
        const float ring = std::abs(radial - ring_radius);
        const float handle = cursor_distance_to_segment(
            point, handle_start, handle_finish);
        CursorColor color = radial < ring_radius - 1.8F ? glass : transparent;
        if (ring <= 2.5F || handle <= 3.5F) color = halo;
        if (ring <= 1.8F || handle <= 2.7F) color = outline;
        if (ring <= 1.0F || handle <= 1.65F) color = accent;
        const bool focus_cross =
            (std::abs(point.x - center.x) <= 0.8F &&
             std::abs(point.y - center.y) <= 4.0F) ||
            (std::abs(point.y - center.y) <= 0.8F &&
             std::abs(point.x - center.x) <= 4.0F);
        if (focus_cross) color = focus;
        return color;
    };

    std::vector<unsigned char> pixels(
        static_cast<std::size_t>(width * height * 4), 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float red{};
            float green{};
            float blue{};
            float alpha{};
            for (int sample_y = 0; sample_y < sample_grid; ++sample_y) {
                for (int sample_x = 0; sample_x < sample_grid; ++sample_x) {
                    const CursorPoint point{
                        (static_cast<float>(x) +
                         (static_cast<float>(sample_x) + 0.5F) / sample_grid) *
                            logical_size / static_cast<float>(width),
                        (static_cast<float>(y) +
                         (static_cast<float>(sample_y) + 0.5F) / sample_grid) *
                            logical_size / static_cast<float>(height)};
                    const CursorColor color = sample_color(point);
                    red += color.r * color.a;
                    green += color.g * color.a;
                    blue += color.b * color.a;
                    alpha += color.a;
                }
            }
            constexpr float samples = static_cast<float>(sample_grid * sample_grid);
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset] = static_cast<unsigned char>(
                std::clamp(std::lround(blue / samples * 255.0F), 0L, 255L));
            pixels[offset + 1] = static_cast<unsigned char>(
                std::clamp(std::lround(green / samples * 255.0F), 0L, 255L));
            pixels[offset + 2] = static_cast<unsigned char>(
                std::clamp(std::lround(red / samples * 255.0F), 0L, 255L));
            pixels[offset + 3] = static_cast<unsigned char>(
                std::clamp(std::lround(alpha / samples * 255.0F), 0L, 255L));
        }
    }

    BITMAPV5HEADER header{};
    header.bV5Size = sizeof(header);
    header.bV5Width = width;
    header.bV5Height = -height;
    header.bV5Planes = 1;
    header.bV5BitCount = 32;
    header.bV5Compression = BI_BITFIELDS;
    header.bV5RedMask = 0x00ff0000;
    header.bV5GreenMask = 0x0000ff00;
    header.bV5BlueMask = 0x000000ff;
    header.bV5AlphaMask = 0xff000000;
    void* bitmap_bits{};
    HDC screen = GetDC(nullptr);
    HBITMAP color_bitmap = CreateDIBSection(
        screen, reinterpret_cast<const BITMAPINFO*>(&header), DIB_RGB_COLORS,
        &bitmap_bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!color_bitmap || !bitmap_bits) {
        if (color_bitmap) DeleteObject(color_bitmap);
        return nullptr;
    }
    std::memcpy(bitmap_bits, pixels.data(), pixels.size());
    HBITMAP mask_bitmap = CreateBitmap(width, height, 1, 1, nullptr);
    if (!mask_bitmap) {
        DeleteObject(color_bitmap);
        return nullptr;
    }
    ICONINFO information{};
    information.fIcon = FALSE;
    information.xHotspot = static_cast<DWORD>(std::clamp(
        std::lround(center.x * static_cast<float>(width) / logical_size), 0L,
        static_cast<long>(width - 1)));
    information.yHotspot = static_cast<DWORD>(std::clamp(
        std::lround(center.y * static_cast<float>(height) / logical_size), 0L,
        static_cast<long>(height - 1)));
    information.hbmMask = mask_bitmap;
    information.hbmColor = color_bitmap;
    HCURSOR cursor = static_cast<HCURSOR>(CreateIconIndirect(&information));
    DeleteObject(mask_bitmap);
    DeleteObject(color_bitmap);
    return cursor;
}

enum class ZoomView : int { Fullscreen = 0, Lens = 1, Docked = 2 };
enum class ZoomEditState : int { Off = 0, Navigate = 1, Annotate = 2 };
constexpr auto kZoomEntryDuration = std::chrono::milliseconds{180};

Tool current_gesture_tool(Tool selected) noexcept {
    return gesture_tool(
        selected,
        (GetKeyState(VK_SHIFT) & 0x8000) != 0,
        (GetKeyState(VK_CONTROL) & 0x8000) != 0,
        (GetKeyState(VK_MENU) & 0x8000) != 0,
        (GetKeyState(VK_TAB) & 0x8000) != 0);
}

std::wstring hotkey_text(HotkeyBinding binding) {
    if (binding.virtual_key == 0) return L"Sin asignar";
    std::wstring result;
    const auto append = [&result](const wchar_t* value) {
        if (!result.empty()) result += L" + ";
        result += value;
    };
    if ((binding.modifiers & MOD_CONTROL) != 0) append(L"Ctrl");
    if ((binding.modifiers & MOD_ALT) != 0) append(L"Alt");
    if ((binding.modifiers & MOD_SHIFT) != 0) append(L"Shift");
    if ((binding.modifiers & MOD_WIN) != 0) append(L"Win");
    wchar_t key_name[64]{};
    if (binding.virtual_key >= 'A' && binding.virtual_key <= 'Z') {
        key_name[0] = static_cast<wchar_t>(binding.virtual_key);
        key_name[1] = L'\0';
    } else if (binding.virtual_key >= '0' && binding.virtual_key <= '9') {
        key_name[0] = static_cast<wchar_t>(binding.virtual_key);
        key_name[1] = L'\0';
    } else {
        switch (binding.virtual_key) {
            case VK_SPACE: wcscpy_s(key_name, L"Espacio"); break;
            case VK_TAB: wcscpy_s(key_name, L"Tab"); break;
            case VK_RETURN: wcscpy_s(key_name, L"Enter"); break;
            case VK_BACK: wcscpy_s(key_name, L"Retroceso"); break;
            case VK_DELETE: wcscpy_s(key_name, L"Supr"); break;
            default: {
                const UINT scan_code = MapVirtualKeyW(binding.virtual_key, MAPVK_VK_TO_VSC);
                GetKeyNameTextW(static_cast<LONG>(scan_code << 16U), key_name,
                                static_cast<int>(std::size(key_name)));
                break;
            }
        }
    }
    append(key_name[0] ? key_name : L"Tecla");
    return result;
}

struct HotkeyInfo {
    const wchar_t* title;
    const wchar_t* description;
};

constexpr std::array<HotkeyInfo, kHotkeyActionCount> kHotkeyInfo{{
    {L"Cursor / lapiz", L"Alterna interaccion y dibujo"},
    {L"Visibilidad", L"Oculta o muestra anotaciones"},
    {L"Pizarra blanca", L"Abre o cierra el lienzo blanco"},
    {L"Deshacer", L"Revierte en el contexto actual"},
    {L"Rehacer", L"Recupera en el contexto actual"},
    {L"Limpiar", L"Limpia el contexto actual"},
    {L"Zoom", L"Abre o cierra la ampliacion"},
    {L"Pizarra negra", L"Abre o cierra el lienzo negro"},
    {L"Lapiz", L"Activa dibujo libre"},
    {L"Resaltador", L"Activa tinta translucida"},
    {L"Borrador", L"Activa borrado selectivo"},
    {L"Linea", L"Activa linea recta"},
    {L"Rectangulo", L"Activa rectangulo"},
    {L"Elipse", L"Activa circulo o elipse"},
    {L"Flecha", L"Activa flecha recta"},
    {L"Flecha curva", L"Activa flecha Bezier"},
    {L"Texto", L"Escribe directamente en pantalla"},
    {L"Captura", L"Activa seleccion de captura"},
    {L"Colores", L"Abre el selector completo"},
    {L"Figuras", L"Abre geometria directamente"},
    {L"Herramientas", L"Abre todas las herramientas"},
    {L"Configuracion", L"Abre preferencias y atajos"},
    {L"Contraer paleta", L"Contrae o expande Elite Pen"},
    {L"Color: negro", L"Selecciona negro directamente"},
    {L"Color: amarillo", L"Selecciona amarillo directamente"},
    {L"Color: azul", L"Selecciona azul directamente"},
    {L"Color: rojo", L"Selecciona rojo directamente"},
    {L"Color: verde", L"Selecciona verde directamente"},
    {L"Color: morado", L"Selecciona morado directamente"},
    {L"Colores (+)", L"Alternativa para abrir el selector completo"},
    {L"Zoom: pausar", L"Congela o reanuda para anotar"},
    {L"Zoom: completa", L"Cambia a pantalla completa"},
    {L"Zoom: lente", L"Cambia a lente movil"},
    {L"Zoom: acoplada", L"Acopla la ampliacion arriba"},
    {L"Zoom: vista", L"Recorre los tres modos"},
    {L"Zoom: invertir", L"Invierte o restaura colores"},
    {L"Zoom: vision general", L"Alterna ampliacion 1x"},
    {L"Zoom: acercar", L"Aumenta el nivel de zoom"},
    {L"Zoom: alejar", L"Reduce el nivel de zoom"},
    {L"Zoom: editar", L"Navega y anota sobre contenido ampliado"}
}};

constexpr std::size_t kVisibleShortcutRows = 6;

class Controller;

class DocumentRenderCache {
public:
    bool update(GraphicsDevice& graphics, Surface& surface, const Document& document,
                float offset_x, float offset_y, RectF viewport);
    void draw(ID2D1DeviceContext* context) const;
    void reset() noexcept;

private:
    ComPtr<ID2D1Bitmap1> bitmap_;
    std::uint64_t revision_{std::numeric_limits<std::uint64_t>::max()};
    std::uint64_t surface_generation_{};
    UINT width_{};
    UINT height_{};
    float offset_x_{};
    float offset_y_{};
    std::size_t item_count_{};
};

struct WorldTileKey {
    int x{};
    int y{};

    constexpr bool operator==(const WorldTileKey&) const noexcept = default;
};

struct WorldTileKeyHash {
    std::size_t operator()(WorldTileKey key) const noexcept {
        const auto x = static_cast<std::uint32_t>(key.x);
        const auto y = static_cast<std::uint32_t>(key.y);
        return static_cast<std::size_t>(
            (static_cast<std::uint64_t>(x) << 32U) ^
            static_cast<std::uint64_t>(y));
    }
};

// Sparse source-space cache for editable zoom annotations. Each drawable is
// rasterized once into 512 px GPU tiles; panning and zooming only transform the
// visible tiles, so navigation cost does not grow with the document history.
class WorldDocumentRenderCache {
public:
    bool update(GraphicsDevice& graphics, Surface& surface,
                const Document& document);
    void draw(ID2D1DeviceContext* context,
              const ZoomViewportTransform& transform) const;
    void reset() noexcept;

private:
    static constexpr int kTileSize = 512;
    // A target tile occupies approximately 1 MiB in BGRA8. Capping the sparse
    // atlas prevents a long multi-monitor session from exhausting shared GPU
    // memory on older PCs; rendering safely falls back to visible vectors.
    static constexpr std::size_t kMaxTileCount = 160;
    using TileMap = std::unordered_map<WorldTileKey, ComPtr<ID2D1Bitmap1>,
                                       WorldTileKeyHash>;

    bool rebuild(GraphicsDevice& graphics, ID2D1DeviceContext* context,
                 const Document& document);
    bool append(GraphicsDevice& graphics, ID2D1DeviceContext* context,
                const Drawable& drawable);
    bool ensure_tile(ID2D1DeviceContext* context, WorldTileKey key,
                     ID2D1Bitmap1** bitmap);
    static int tile_coordinate(float value) noexcept;
    static RectF tile_bounds(WorldTileKey key) noexcept;

    TileMap tiles_;
    std::uint64_t revision_{std::numeric_limits<std::uint64_t>::max()};
    std::uint64_t surface_generation_{};
    std::size_t item_count_{};
    std::uint64_t capacity_failed_revision_{
        std::numeric_limits<std::uint64_t>::max()};
    std::uint64_t capacity_failed_generation_{};
    bool capacity_exhausted_{};
};

class WindowBase {
public:
    explicit WindowBase(Controller& controller) : controller_(controller) {}
    virtual ~WindowBase() {
        if (window_) DestroyWindow(window_);
    }

    WindowBase(const WindowBase&) = delete;
    WindowBase& operator=(const WindowBase&) = delete;

    bool create(const wchar_t* class_name, const wchar_t* title, DWORD ex_style,
                DWORD style, const RECT& bounds, HWND parent = nullptr);
    bool initialize_surface(GraphicsDevice& graphics);
    void invalidate() const { if (window_) InvalidateRect(window_, nullptr, FALSE); }
    [[nodiscard]] HWND hwnd() const noexcept { return window_; }
    [[nodiscard]] Surface& surface() noexcept { return surface_; }

protected:
    virtual LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam);
    virtual void render() {}
    Controller& controller_;
    HWND window_{};
    Surface surface_;

private:
    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam,
                                        LPARAM lparam);
};

class OverlayWindow final : public WindowBase {
public:
    OverlayWindow(Controller& controller, RECT monitor_rect)
        : WindowBase(controller), monitor_rect_(monitor_rect) {}
    ~OverlayWindow() override {
        if (pencil_cursor_) DestroyCursor(pencil_cursor_);
    }

    bool initialize(GraphicsDevice& graphics);
    void update_interaction();
    void cancel_gesture();
    [[nodiscard]] const RECT& monitor_rect() const noexcept { return monitor_rect_; }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;
    void render() override;

private:
    void refresh_pencil_cursor();
    PointF global_point(LPARAM lparam) const noexcept;
    std::optional<PointF> pointer_point(WPARAM wparam) const noexcept;
    void begin_gesture(PointF point, float pressure = 1.0F);
    void update_gesture(PointF point, WPARAM keys);
    void finish_gesture(PointF point, WPARAM keys);

    RECT monitor_rect_{};
    bool drawing_{};
    bool erasing_{};
    bool pointer_active_{};
    UINT32 pointer_id_{};
    HCURSOR pencil_cursor_{};
    DocumentRenderCache document_cache_;
};

class PaletteWindow final : public WindowBase {
public:
    explicit PaletteWindow(Controller& controller) : WindowBase(controller) {}
    bool initialize(GraphicsDevice& graphics);
    void install_hotkeys();
    bool apply_hotkeys(const std::array<HotkeyBinding, kHotkeyActionCount>& bindings);
    void remove_hotkeys();
    void add_tray_icon();
    void remove_tray_icon();
    void show_notification(const wchar_t* title, const std::wstring& message);
    [[nodiscard]] bool activate_command_at(POINT point);
    [[nodiscard]] bool contains_screen_point(POINT point) const;
    void apply_size(int size);
    void apply_mode(ControlMode mode);
    void set_collapsed(bool collapsed);
    [[nodiscard]] bool collapsed() const noexcept { return collapsed_; }
    [[nodiscard]] int pixel_width() const noexcept {
        return control_pixel_width(mode_, scale_, collapsed_);
    }
    [[nodiscard]] int pixel_height() const noexcept {
        return control_pixel_height(mode_, scale_, collapsed_);
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;
    void render() override;

private:
    void render_linear(ID2D1DeviceContext* context, const UiTheme& theme);
    void install_tooltips();
    [[nodiscard]] bool command_at(POINT point) const;
    [[nodiscard]] bool collapsed_expand_at(POINT point) const;
    void begin_drag(POINT point);
    void activate_at(POINT point);
    void show_tool_menu();
    void show_tray_menu();
    void choose_custom_color();
    bool dragging_{};
    POINT drag_origin_{};
    POINT window_origin_{};
    NOTIFYICONDATAW tray_{};
    bool escape_down_{};
    bool highlight_cursor_initialized_{};
    POINT last_highlight_cursor_{};
    HWND tooltip_{};
    float scale_{kPaletteScales[1]};
    ControlMode mode_{ControlMode::Palette};
    bool collapsed_{};
    int hovered_linear_item_{-1};
    bool tracking_mouse_{};
    bool hotkeys_registered_{};
    std::array<HotkeyBinding, kHotkeyActionCount> registered_hotkeys_{kDefaultHotkeys};
};

class ColorWindow final : public WindowBase {
public:
    explicit ColorWindow(Controller& controller) : WindowBase(controller) {}
    bool initialize(GraphicsDevice& graphics);
    void toggle_near(HWND anchor);
    void hide() { ShowWindow(window_, SW_HIDE); }
    [[nodiscard]] bool visible() const noexcept { return IsWindowVisible(window_) != FALSE; }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;
    void render() override;

private:
    void choose_system_color();
    int hovered_item_{-1};
};

class ToolWindow final : public WindowBase {
public:
    explicit ToolWindow(Controller& controller) : WindowBase(controller) {}
    bool initialize(GraphicsDevice& graphics);
    void toggle_near(HWND anchor);
    void toggle_geometry_near(HWND anchor);
    void hide() { ShowWindow(window_, SW_HIDE); }
    [[nodiscard]] bool visible() const noexcept { return IsWindowVisible(window_) != FALSE; }
    [[nodiscard]] bool geometry_only() const noexcept { return geometry_only_; }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;
    void render() override;

private:
    void show_near(HWND anchor, bool geometry_only);
    [[nodiscard]] std::size_t tool_count() const noexcept;
    [[nodiscard]] Tool tool_at(std::size_t index) const noexcept;
    bool geometry_only_{};
    int hovered_item_{-1};
};

class TextInputWindow final : public WindowBase {
public:
    explicit TextInputWindow(Controller& controller) : WindowBase(controller) {}
    bool initialize(GraphicsDevice& graphics);
    void show_at(PointF position, Color color, float thickness);
    void update_style(Color color, float thickness);
    void commit();
    void cancel();
    [[nodiscard]] bool active() const noexcept { return active_; }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;
    void render() override;

private:
    PointF position_{};
    Color color_{};
    float thickness_{};
    std::wstring text_;
    bool active_{};
    bool caret_visible_{true};
};

class SettingsWindow final : public WindowBase {
public:
    explicit SettingsWindow(Controller& controller) : WindowBase(controller) {}
    ~SettingsWindow() override;
    bool initialize();
    void show_settings();
    void apply_theme();

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;

private:
    void paint_background(HDC dc);
    void paint_shortcuts(HDC dc, RECT bounds);
    void paint_help(HDC dc, RECT bounds);
    void refresh_controls();
    void refresh_shortcut_rows();
    void show_tab(int tab);
    void begin_hotkey_capture(std::size_t index);
    void finish_hotkey_capture(UINT virtual_key);
    void cancel_hotkey_capture();
    HWND title_{};
    HWND subtitle_{};
    HWND tab_general_{};
    HWND tab_shortcuts_{};
    HWND tab_help_{};
    HWND capture_{};
    HWND confirm_clear_{};
    HWND start_interact_{};
    HWND highlight_cursor_{};
    HWND fade_label_{};
    HWND fade_{};
    HWND thickness_label_{};
    HWND thickness_{};
    HWND zoom_label_{};
    HWND zoom_{};
    HWND zoom_view_label_{};
    HWND zoom_view_{};
    HWND zoom_invert_{};
    HWND zoom_lens_size_label_{};
    HWND zoom_lens_size_{};
    HWND palette_size_label_{};
    HWND palette_size_{};
    HWND palette_size_hint_{};
    HWND control_mode_label_{};
    HWND control_mode_{};
    HWND theme_label_{};
    HWND theme_dark_{};
    HWND theme_light_{};
    HWND reset_position_{};
    HWND shortcuts_{};
    HWND help_{};
    HWND help_website_{};
    HWND help_source_{};
    std::array<HWND, kVisibleShortcutRows> hotkey_buttons_{};
    std::array<HWND, kVisibleShortcutRows> hotkey_edit_buttons_{};
    HWND shortcut_scrollbar_{};
    HWND reset_hotkeys_{};
    HWND close_{};
    HWND chrome_close_{};
    HFONT title_font_{};
    HFONT body_font_{};
    HFONT small_font_{};
    HBRUSH background_brush_{};
    HBRUSH card_brush_{};
    int active_tab_{};
    int capturing_hotkey_{-1};
    std::size_t shortcut_scroll_offset_{};
};

class ZoomInkWindow final : public WindowBase {
public:
    explicit ZoomInkWindow(Controller& controller) : WindowBase(controller) {}
    ~ZoomInkWindow() override {
        if (pencil_cursor_) DestroyCursor(pencil_cursor_);
    }

    bool initialize(GraphicsDevice& graphics);
    bool freeze(RECT bounds, HWND magnifier);
    void show_live(RECT bounds);
    bool show_edit(RECT bounds, RECT source, float factor, bool annotating,
                   HWND magnifier);
    void update_edit_view(RECT bounds, RECT source, float factor);
    void set_edit_annotating(bool annotating);
    void hide();
    void set_bounds(RECT bounds);
    void set_frozen(bool frozen);
    void bring_to_front();
    void clear_annotations();
    bool undo();
    bool redo();
    [[nodiscard]] bool empty() const noexcept {
        return edit_mode_ ? edit_document_.empty() : document_.empty();
    }
    [[nodiscard]] std::size_t item_count() const noexcept {
        return edit_mode_ ? edit_document_.items().size() : document_.items().size();
    }
    [[nodiscard]] bool frozen() const noexcept { return frozen_; }
    [[nodiscard]] bool edit_mode() const noexcept { return edit_mode_; }
    [[nodiscard]] bool accepts_annotations() const noexcept {
        return frozen_ || (edit_mode_ && edit_annotating_);
    }
    void populate_edit_stress_document(std::size_t count);
    [[nodiscard]] std::optional<PointF> first_edit_view_point() const noexcept;
    void commit_text_screen(PointF position, Color color, float thickness,
                            std::wstring text);
    void cancel_gesture();

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;
    void render() override;

private:
    PointF local_point(LPARAM lparam) const noexcept;
    PointF annotation_point(PointF local) const noexcept;
    PointF view_point(PointF annotation) const noexcept;
    float annotation_length(float view_length) const noexcept;
    std::optional<PointF> pointer_local_point(WPARAM wparam) const noexcept;
    void refresh_pencil_cursor();
    void begin_gesture(PointF point, float pressure = 1.0F);
    void update_gesture(PointF point, WPARAM keys);
    void finish_gesture(PointF point, WPARAM keys);
    bool capture_snapshot(HWND magnifier, RECT bounds);
    bool capture_composite(PointF first, PointF second);

    Document document_;
    Document edit_document_;
    std::optional<Drawable> preview_;
    ComPtr<ID2D1Bitmap1> snapshot_;
    RECT bounds_{};
    bool frozen_{};
    bool edit_mode_{};
    bool edit_annotating_{};
    ZoomViewportTransform edit_transform_{};
    bool drawing_{};
    bool erasing_{};
    bool pointer_active_{};
    bool snapshot_has_content_{};
    UINT32 pointer_id_{};
    HCURSOR pencil_cursor_{};
    DocumentRenderCache document_cache_;
    WorldDocumentRenderCache edit_document_cache_;
};

class ZoomTargetWindow final : public WindowBase {
public:
    explicit ZoomTargetWindow(Controller& controller) : WindowBase(controller) {}
    bool initialize(GraphicsDevice& graphics);
    void show_at(POINT cursor);
    void hide() { ShowWindow(window_, SW_HIDE); }
    void bring_to_front();
    [[nodiscard]] bool visible() const noexcept {
        return IsWindowVisible(window_) != FALSE;
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;
    void render() override;
};

class ZoomLensFrameWindow final : public WindowBase {
public:
    explicit ZoomLensFrameWindow(Controller& controller) : WindowBase(controller) {}
    bool initialize(GraphicsDevice& graphics);
    void show_for(RECT lens_bounds);
    void hide() { ShowWindow(window_, SW_HIDE); }
    void bring_to_front();
    [[nodiscard]] bool visible() const noexcept {
        return IsWindowVisible(window_) != FALSE;
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;
    void render() override;

private:
    RECT lens_bounds_{};
};

class ZoomWindow;

class ZoomEditToolbarWindow final : public WindowBase {
public:
    ZoomEditToolbarWindow(Controller& controller, ZoomWindow& owner)
        : WindowBase(controller), owner_(owner) {}
    bool initialize(GraphicsDevice& graphics);
    void show_for(RECT zoom_bounds, float factor, ZoomEditState state);
    void hide() { ShowWindow(window_, SW_HIDE); }
    void bring_to_front();
    [[nodiscard]] bool visible() const noexcept {
        return IsWindowVisible(window_) != FALSE;
    }
    [[nodiscard]] bool contains_screen_point(POINT point) const noexcept;

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;
    void render() override;

private:
    enum class Command { None, ZoomOut, ZoomIn, Navigate, Annotate, Close };
    [[nodiscard]] Command command_at(POINT client) const noexcept;
    void execute(Command command);

    ZoomWindow& owner_;
    float factor_{2.0F};
    ZoomEditState state_{ZoomEditState::Navigate};
    Command hovered_{Command::None};
};

class ZoomWindow final : public WindowBase {
public:
    explicit ZoomWindow(Controller& controller) : WindowBase(controller) {}
    ~ZoomWindow() override;

    bool initialize(GraphicsDevice& graphics);
    bool show_zoom();
    void hide_zoom();
    void apply_theme();
    bool toggle_freeze();
    void toggle_edit_mode();
    void set_edit_state(ZoomEditState state);
    void adjust_zoom(float delta);
    void set_lens_diameter(int diameter);
    void adjust_lens_diameter(int direction);
    void execute_action(HotkeyAction action);
    void bring_to_front();
    void invalidate_ink();
    void clear_annotations();
    bool undo();
    bool redo();
    void commit_text_screen(PointF position, Color color, float thickness,
                            std::wstring text);
    [[nodiscard]] bool active() const noexcept { return active_; }
    [[nodiscard]] bool frozen() const noexcept {
        return ink_ && ink_->frozen();
    }
    [[nodiscard]] bool edit_active() const noexcept {
        return edit_state_ != ZoomEditState::Off;
    }
    [[nodiscard]] bool accepts_annotations() const noexcept {
        return ink_ && ink_->accepts_annotations();
    }
    [[nodiscard]] bool toolbar_contains(POINT point) const noexcept {
        return edit_toolbar_ && edit_toolbar_->visible() &&
               edit_toolbar_->contains_screen_point(point);
    }
    [[nodiscard]] bool annotations_empty() const noexcept {
        return !ink_ || ink_->empty();
    }
    [[nodiscard]] std::size_t annotation_count() const noexcept {
        return ink_ ? ink_->item_count() : 0U;
    }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;

private:
    static ZoomWindow* click_hook_owner_;
    static LRESULT CALLBACK click_hook_proc(int code, WPARAM wparam, LPARAM lparam);
    static LRESULT CALLBACK magnifier_subclass(
        HWND window, UINT message, WPARAM wparam, LPARAM lparam,
        UINT_PTR subclass_id, DWORD_PTR reference_data);
    void refresh_source();
    void update_lens_cursor(bool active);
    void remember_foreground_target();
    bool start_recordable_output();
    void stop_recordable_output();
    void update_recordable_output();
    void set_edit_passthrough(bool enabled);
    bool install_click_hook();
    void uninstall_click_hook();
    void apply_color_effect();
    void cycle_view();
    void apply_view_regions(ZoomView view, int width, int height);
    bool load_magnification();
    void finish_entry_animation();

    using MagInitializePointer = BOOL(WINAPI*)();
    using MagUninitializePointer = BOOL(WINAPI*)();
    using MagSetWindowSourcePointer = BOOL(WINAPI*)(HWND, RECT);
    using MagSetWindowTransformPointer = BOOL(WINAPI*)(HWND, PMAGTRANSFORM);
    using MagSetWindowFilterListPointer = BOOL(WINAPI*)(HWND, DWORD, int, HWND*);
    using MagSetColorEffectPointer = BOOL(WINAPI*)(HWND, PMAGCOLOREFFECT);

    HMODULE magnification_module_{};
    MagInitializePointer mag_initialize_{};
    MagUninitializePointer mag_uninitialize_{};
    MagSetWindowSourcePointer mag_set_source_{};
    MagSetWindowTransformPointer mag_set_transform_{};
    MagSetWindowFilterListPointer mag_set_filter_{};
    MagSetColorEffectPointer mag_set_color_effect_{};
    HWND magnifier_{};
    std::unique_ptr<ZoomInkWindow> ink_;
    std::unique_ptr<ZoomTargetWindow> target_;
    std::unique_ptr<ZoomLensFrameWindow> lens_frame_;
    std::unique_ptr<ZoomEditToolbarWindow> edit_toolbar_;
    RECT monitor_rect_{};
    RECT zoom_rect_{};
    RECT source_rect_{};
    HTHUMBNAIL recordable_thumbnail_{};
    SIZE recordable_source_size_{};
    RECT recordable_source_bounds_{};
    bool recordable_output_{};
    bool initialized_{};
    bool active_{};
    bool overview_{};
    ZoomEditState edit_state_{ZoomEditState::Off};
    POINT edit_anchor_{};
    HWND foreground_before_zoom_{};
    HHOOK click_hook_{};
    bool click_freeze_pending_{};
    HCURSOR lens_cursor_{};
    UINT lens_cursor_dpi_{};
    bool lens_cursor_active_{};
    bool source_initialized_{};
    POINT last_source_cursor_{};
    float last_source_factor_{};
    int last_source_view_{-1};
    int region_view_{-1};
    int region_width_{};
    int region_height_{};
    bool last_source_overview_{};
    bool entry_animation_active_{};
    std::chrono::steady_clock::time_point entry_animation_started_{};
    POINT entry_animation_anchor_{};
    float presented_factor_{1.0F};
    Tool tool_before_zoom_{Tool::Pen};
};

struct TransientDrawable {
    Drawable drawable;
    std::uint64_t expires_at_ms{};
};

class Controller {
public:
    bool initialize(std::wstring& error);
    int message_loop();
    void shutdown();

    AppState& state() noexcept { return state_; }
    GraphicsDevice& graphics() noexcept { return graphics_; }
    Preferences& preferences() noexcept { return preferences_; }
    PaletteWindow* palette() const noexcept { return palette_.get(); }
    const std::vector<std::unique_ptr<OverlayWindow>>& overlays() const noexcept {
        return overlays_;
    }
    std::optional<Drawable>& preview() noexcept { return preview_; }
    const std::optional<Drawable>& preview() const noexcept { return preview_; }
    const std::vector<TransientDrawable>& transient_drawables() const noexcept {
        return transient_drawables_;
    }

    void invalidate_all();
    void invalidate_document();
    void invalidate_preview();
    void update_overlay_interaction();
    void restack_palette();
    [[nodiscard]] bool route_palette_command(PointF screen_point);
    void set_tool(Tool tool);
    void set_color(Color color);
    void set_thickness(float thickness);
    void adjust_thickness_step(int direction);
    void toggle_visibility();
    void toggle_whiteboard();
    void toggle_blackboard();
    void clear_document();
    void undo();
    void redo();
    void toggle_zoom();
    void toggle_zoom_freeze();
    void toggle_color_panel();
    void toggle_tool_panel();
    void toggle_geometry_panel();
    void close_panels();
    void stop_transient_mode();
    void begin_text(PointF position);
    void commit_pending_text();
    void cancel_pending_text();
    void commit_text(PointF position, Color color, float thickness, std::wstring text);
    void commit_drawable(Drawable drawable);
    void update_transient_ink();
    bool capture_region(PointF first, PointF second,
                        bool allow_synthetic_capture = true);
    bool publish_capture_bitmap(HBITMAP bitmap, int width, int height);
    void rebuild_overlays();
    void report_runtime_error(const std::wstring& message);
    void request_exit();
    void save_palette_position();
    void show_settings_window();
    void apply_capture_preference();
    void set_palette_size(int size);
    void set_control_mode(ControlMode mode);
    void set_zoom_lens_diameter(int diameter);
    void adjust_zoom_lens_diameter(int direction);
    void set_theme(AppTheme theme);
    void execute_hotkey(HotkeyAction action);
    [[nodiscard]] bool matches_hotkey(HotkeyAction action, WPARAM virtual_key) const;
    bool set_hotkey_binding(HotkeyAction action, HotkeyBinding binding);
    bool reset_hotkeys();
    void reset_palette_position();
    void save_preferences();
    [[nodiscard]] bool zoom_active() const noexcept;
    [[nodiscard]] bool zoom_frozen() const noexcept;
    void restack_zoom();
    void populate_stress_document(std::size_t count);

private:
    static BOOL CALLBACK collect_monitor(HMONITOR monitor, HDC, LPRECT, LPARAM data);
    void create_overlays(std::wstring& error);

    GraphicsDevice graphics_;
    AppState state_;
    PreferencesStore preferences_store_;
    Preferences preferences_;
    std::optional<Drawable> preview_;
    std::vector<TransientDrawable> transient_drawables_;
    std::vector<std::unique_ptr<OverlayWindow>> overlays_;
    std::unique_ptr<PaletteWindow> palette_;
    std::unique_ptr<ColorWindow> colors_;
    std::unique_ptr<ToolWindow> tools_;
    std::unique_ptr<TextInputWindow> text_input_;
    std::unique_ptr<SettingsWindow> settings_;
    std::unique_ptr<ZoomWindow> zoom_;
    bool shutting_down_{};
};

D2D1_COLOR_F d2d_color(Color color, float opacity = 1.0F) {
    return D2D1::ColorF(static_cast<float>(color.r) / 255.0F,
                        static_cast<float>(color.g) / 255.0F,
                        static_cast<float>(color.b) / 255.0F,
                        static_cast<float>(color.a) / 255.0F * opacity);
}

ComPtr<ID2D1LinearGradientBrush> linear_gradient(
        ID2D1RenderTarget* target, D2D1_POINT_2F start, D2D1_POINT_2F end,
        const D2D1_GRADIENT_STOP* stops, UINT stop_count) {
    ComPtr<ID2D1GradientStopCollection> collection;
    ComPtr<ID2D1LinearGradientBrush> brush;
    if (FAILED(target->CreateGradientStopCollection(
            stops, stop_count, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP,
            collection.GetAddressOf())) || !collection) return brush;
    target->CreateLinearGradientBrush(
        D2D1::LinearGradientBrushProperties(start, end), collection.Get(),
        brush.GetAddressOf());
    return brush;
}

void apply_premium_window_chrome(HWND window) {
    const auto& theme = current_ui_theme();
    const BOOL dark = theme.light ? FALSE : TRUE;
    DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
    DwmSetWindowAttribute(window, 19, &dark, sizeof(dark));
    const DWORD rounded = 2;
    DwmSetWindowAttribute(window, 33, &rounded, sizeof(rounded));
    const COLORREF border = theme_colorref(theme.violet_strong);
    const COLORREF caption = theme_colorref(theme.surface_1);
    const COLORREF caption_text = theme_colorref(theme.text);
    DwmSetWindowAttribute(window, 34, &border, sizeof(border));
    DwmSetWindowAttribute(window, 35, &caption, sizeof(caption));
    DwmSetWindowAttribute(window, 36, &caption_text, sizeof(caption_text));
}

void paint_premium_combo(HWND window, HDC dc) {
    const auto& theme = current_ui_theme();
    RECT bounds{};
    GetClientRect(window, &bounds);
    HBRUSH background = CreateSolidBrush(theme_colorref(theme.surface_2));
    FillRect(dc, &bounds, background);
    DeleteObject(background);

    RECT face_rect = bounds;
    face_rect.right -= 1;
    face_rect.bottom -= 1;
    const bool focused = GetFocus() == window;
    HBRUSH face = CreateSolidBrush(theme_colorref(theme.surface_3));
    HPEN outline = CreatePen(PS_SOLID, focused ? 2 : 1,
                             theme_colorref(focused ? theme.violet : theme.line));
    HGDIOBJ previous_brush = SelectObject(dc, face);
    HGDIOBJ previous_pen = SelectObject(dc, outline);
    RoundRect(dc, face_rect.left, face_rect.top, face_rect.right, face_rect.bottom, 8, 8);
    SelectObject(dc, previous_pen);
    SelectObject(dc, previous_brush);
    DeleteObject(outline);
    DeleteObject(face);

    constexpr int arrow_width = 27;
    HPEN divider = CreatePen(PS_SOLID, 1, theme_colorref(theme.line));
    previous_pen = SelectObject(dc, divider);
    MoveToEx(dc, bounds.right - arrow_width, 4, nullptr);
    LineTo(dc, bounds.right - arrow_width, bounds.bottom - 4);
    SelectObject(dc, previous_pen);
    DeleteObject(divider);

    wchar_t value[128]{};
    const LRESULT selection = SendMessageW(window, CB_GETCURSEL, 0, 0);
    if (selection >= 0) {
        SendMessageW(window, CB_GETLBTEXT, static_cast<WPARAM>(selection),
                     reinterpret_cast<LPARAM>(value));
    }
    HGDIOBJ previous_font = nullptr;
    if (const auto font = reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0))) {
        previous_font = SelectObject(dc, font);
    }
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, theme_colorref(IsWindowEnabled(window) ? theme.text : theme.text_muted));
    RECT text_rect{10, 0, bounds.right - arrow_width - 7, bounds.bottom};
    DrawTextW(dc, value, -1, &text_rect,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (previous_font) SelectObject(dc, previous_font);

    HPEN chevron = CreatePen(PS_SOLID, 2,
                             theme_colorref(focused ? theme.violet_strong : theme.violet));
    previous_pen = SelectObject(dc, chevron);
    const int center_x = bounds.right - arrow_width / 2;
    const int center_y = bounds.bottom / 2;
    MoveToEx(dc, center_x - 4, center_y - 2, nullptr);
    LineTo(dc, center_x, center_y + 2);
    LineTo(dc, center_x + 4, center_y - 2);
    SelectObject(dc, previous_pen);
    DeleteObject(chevron);
}

LRESULT CALLBACK premium_combo_subclass(HWND window, UINT message, WPARAM wparam,
                                        LPARAM lparam, UINT_PTR subclass_id,
                                        DWORD_PTR) {
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window, &paint);
            paint_premium_combo(window, dc);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_PRINTCLIENT:
            paint_premium_combo(window, reinterpret_cast<HDC>(wparam));
            return 0;
        case WM_ERASEBKGND:
        case WM_NCPAINT:
            return 1;
        case CB_SETCURSEL: {
            const LRESULT result = DefSubclassProc(window, message, wparam, lparam);
            InvalidateRect(window, nullptr, FALSE);
            return result;
        }
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
        case WM_ENABLE:
        case WM_LBUTTONUP:
        case WM_KEYUP: {
            const LRESULT result = DefSubclassProc(window, message, wparam, lparam);
            InvalidateRect(window, nullptr, FALSE);
            return result;
        }
        case WM_NCDESTROY:
            RemoveWindowSubclass(window, premium_combo_subclass, subclass_id);
            break;
        default:
            break;
    }
    return DefSubclassProc(window, message, wparam, lparam);
}

std::uint64_t monotonic_milliseconds() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

constexpr std::array<Color, 42> kExtendedColors{{
    {24, 24, 27, 255}, {82, 82, 91, 255}, {161, 161, 170, 255},
    {228, 228, 231, 255}, {255, 255, 255, 255}, {120, 72, 40, 255},
    {239, 68, 68, 255}, {249, 115, 22, 255}, {245, 158, 11, 255},
    {255, 190, 45, 255}, {250, 204, 21, 255}, {234, 179, 8, 255},
    {132, 204, 22, 255}, {34, 197, 94, 255}, {16, 185, 129, 255},
    {20, 184, 166, 255}, {6, 182, 212, 255}, {14, 165, 233, 255},
    {31, 136, 229, 255}, {59, 130, 246, 255}, {99, 102, 241, 255},
    {139, 92, 246, 255}, {168, 85, 247, 255}, {217, 70, 239, 255},
    {236, 72, 153, 255}, {244, 63, 94, 255}, {190, 24, 93, 255},
    {127, 29, 29, 255}, {154, 52, 18, 255}, {133, 77, 14, 255},
    {63, 98, 18, 255}, {22, 101, 52, 255}, {17, 94, 89, 255},
    {21, 94, 117, 255}, {30, 64, 175, 255}, {49, 46, 129, 255},
    {88, 28, 135, 255}, {112, 26, 117, 255}, {131, 24, 67, 255},
    {255, 128, 161, 255}, {92, 225, 230, 255}, {137, 245, 173, 255}
}};

constexpr std::array<Tool, 12> kTools{{
    Tool::Interact, Tool::Pen, Tool::Highlighter, Tool::Eraser, Tool::Line,
    Tool::Rectangle, Tool::Ellipse, Tool::Arrow, Tool::CurvedArrow, Tool::Text,
    Tool::Screenshot, Tool::Zoom
}};

constexpr std::array<Tool, 5> kGeometryTools{{
    Tool::Line, Tool::Rectangle, Tool::Ellipse, Tool::Arrow, Tool::CurvedArrow
}};

bool point_in_circle(POINT point, float cx, float cy, float radius) {
    const float dx = static_cast<float>(point.x) - cx;
    const float dy = static_cast<float>(point.y) - cy;
    return dx * dx + dy * dy <= radius * radius;
}

bool point_near_segment(POINT point, float start_x, float start_y,
                        float end_x, float end_y, float radius) {
    const float segment_x = end_x - start_x;
    const float segment_y = end_y - start_y;
    const float length_squared = segment_x * segment_x + segment_y * segment_y;
    if (length_squared <= 0.0F) return point_in_circle(point, start_x, start_y, radius);
    const float offset_x = static_cast<float>(point.x) - start_x;
    const float offset_y = static_cast<float>(point.y) - start_y;
    const float position = std::clamp(
        (offset_x * segment_x + offset_y * segment_y) / length_squared, 0.0F, 1.0F);
    const float closest_x = start_x + position * segment_x;
    const float closest_y = start_y + position * segment_y;
    return point_in_circle(point, closest_x, closest_y, radius);
}

POINT palette_logical_point(POINT point, float scale) noexcept {
    return {
        static_cast<LONG>(std::lround(static_cast<float>(point.x) / scale)),
        static_cast<LONG>(std::lround(static_cast<float>(point.y) / scale))
    };
}

RECT palette_scaled_rect(RECT bounds, float scale) noexcept {
    return {
        static_cast<LONG>(std::lround(static_cast<float>(bounds.left) * scale)),
        static_cast<LONG>(std::lround(static_cast<float>(bounds.top) * scale)),
        static_cast<LONG>(std::lround(static_cast<float>(bounds.right) * scale)),
        static_cast<LONG>(std::lround(static_cast<float>(bounds.bottom) * scale))
    };
}

bool capture_desktop_duplication(GraphicsDevice& graphics, int left, int top,
                                 int width, int height, void* pixels) {
    ComPtr<IDXGIDevice> dxgi_device;
    if (FAILED(graphics.d3d()->QueryInterface(IID_PPV_ARGS(&dxgi_device)))) return false;
    ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgi_device->GetAdapter(adapter.GetAddressOf()))) return false;

    const RECT requested{left, top, left + width, top + height};
    std::uint64_t copied_area = 0;
    for (UINT output_index = 0;; ++output_index) {
        ComPtr<IDXGIOutput> output;
        const HRESULT enumerate = adapter->EnumOutputs(output_index, output.GetAddressOf());
        if (enumerate == DXGI_ERROR_NOT_FOUND) break;
        if (FAILED(enumerate)) return false;
        DXGI_OUTPUT_DESC output_description{};
        if (FAILED(output->GetDesc(&output_description)) ||
            !output_description.AttachedToDesktop) continue;
        RECT intersection{};
        if (!IntersectRect(&intersection, &requested,
                           &output_description.DesktopCoordinates)) continue;

        ComPtr<IDXGIOutput1> output1;
        if (FAILED(output.As(&output1))) continue;
        ComPtr<IDXGIOutputDuplication> duplication;
        if (FAILED(output1->DuplicateOutput(graphics.d3d(),
                                             duplication.GetAddressOf()))) continue;
        DXGI_OUTDUPL_FRAME_INFO frame_information{};
        ComPtr<IDXGIResource> frame_resource;
        HRESULT acquired = duplication->AcquireNextFrame(
            350, &frame_information, frame_resource.GetAddressOf());
        if (FAILED(acquired)) continue;

        ComPtr<ID3D11Texture2D> source;
        acquired = frame_resource.As(&source);
        if (FAILED(acquired)) {
            duplication->ReleaseFrame();
            continue;
        }
        D3D11_TEXTURE2D_DESC description{};
        source->GetDesc(&description);
        description.Usage = D3D11_USAGE_STAGING;
        description.BindFlags = 0;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        description.MiscFlags = 0;
        description.ArraySize = 1;
        description.MipLevels = 1;
        description.SampleDesc = {1, 0};
        ComPtr<ID3D11Texture2D> staging;
        HRESULT copied = graphics.d3d()->CreateTexture2D(
            &description, nullptr, staging.GetAddressOf());
        if (SUCCEEDED(copied)) {
            graphics.d3d_context()->CopyResource(staging.Get(), source.Get());
            D3D11_MAPPED_SUBRESOURCE mapped{};
            copied = graphics.d3d_context()->Map(staging.Get(), 0,
                                                  D3D11_MAP_READ, 0, &mapped);
            if (SUCCEEDED(copied)) {
                const int source_x = intersection.left -
                    output_description.DesktopCoordinates.left;
                const int source_y = intersection.top -
                    output_description.DesktopCoordinates.top;
                const int destination_x = intersection.left - left;
                const int destination_y = intersection.top - top;
                const std::size_t row_bytes = static_cast<std::size_t>(
                    intersection.right - intersection.left) * 4U;
                for (int row = 0; row < intersection.bottom - intersection.top; ++row) {
                    const auto* source_row = static_cast<const std::uint8_t*>(mapped.pData) +
                        static_cast<std::size_t>(source_y + row) * mapped.RowPitch +
                        static_cast<std::size_t>(source_x) * 4U;
                    auto* destination_row = static_cast<std::uint8_t*>(pixels) +
                        (static_cast<std::size_t>(destination_y + row) *
                         static_cast<std::size_t>(width) +
                         static_cast<std::size_t>(destination_x)) * 4U;
                    std::memcpy(destination_row, source_row, row_bytes);
                }
                graphics.d3d_context()->Unmap(staging.Get(), 0);
                copied_area += static_cast<std::uint64_t>(
                    intersection.right - intersection.left) *
                    static_cast<std::uint64_t>(intersection.bottom - intersection.top);
            }
        }
        duplication->ReleaseFrame();
    }
    return copied_area >= static_cast<std::uint64_t>(width) *
                          static_cast<std::uint64_t>(height);
}

struct CachedTextFormat {
    int size_key{};
    ComPtr<IDWriteTextFormat> format;
};

struct DrawableRenderResources {
    ComPtr<ID2D1SolidColorBrush> brush;
    ComPtr<ID2D1SolidColorBrush> screenshot_shade;
    ComPtr<ID2D1StrokeStyle> stroke_style;
    std::vector<CachedTextFormat> text_formats;

    bool initialize(GraphicsDevice& graphics, ID2D1DeviceContext* context) {
        if (FAILED(context->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::White), brush.GetAddressOf())) || !brush) {
            return false;
        }
        context->CreateSolidColorBrush(
            theme_color(current_ui_theme().violet, 0.12F), screenshot_shade.GetAddressOf());
        const D2D1_STROKE_STYLE_PROPERTIES properties = D2D1::StrokeStyleProperties(
            D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
            D2D1_LINE_JOIN_ROUND, 10.0F, D2D1_DASH_STYLE_SOLID, 0.0F);
        graphics.d2d_factory()->CreateStrokeStyle(
            properties, nullptr, 0, stroke_style.GetAddressOf());
        text_formats.reserve(5);
        return true;
    }

    IDWriteTextFormat* text_format(GraphicsDevice& graphics, float size) {
        const int key = static_cast<int>(std::lround(size * 10.0F));
        for (const auto& cached : text_formats) {
            if (cached.size_key == key) return cached.format.Get();
        }
        CachedTextFormat cached;
        cached.size_key = key;
        graphics.dwrite()->CreateTextFormat(
            L"Segoe UI Variable Text", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"es-CO",
            cached.format.GetAddressOf());
        if (!cached.format) return nullptr;
        text_formats.push_back(std::move(cached));
        return text_formats.back().format.Get();
    }
};

void draw_arrow_head(ID2D1DeviceContext* context, ID2D1Brush* brush, PointF before,
                     PointF end, float width, float reference_scale = 1.0F) {
    const auto head = arrow_head_points(before, end, width, reference_scale);
    context->DrawLine(D2D1::Point2F(end.x, end.y),
                      D2D1::Point2F(head.left.x, head.left.y), brush, width);
    context->DrawLine(D2D1::Point2F(end.x, end.y),
                      D2D1::Point2F(head.right.x, head.right.y), brush, width);
}

void draw_drawable(GraphicsDevice& graphics, ID2D1DeviceContext* context,
                   DrawableRenderResources& resources, const Drawable& drawable,
                   float offset_x, float offset_y,
                   float opacity = 1.0F, float scale = 1.0F,
                   bool source_space = false) {
    if (drawable.points.empty()) return;
    scale = std::max(scale, 0.001F);
    const float render_width = drawable.width * scale;
    const float render_reference_scale = drawable.reference_scale / scale;
    const float alpha = drawable.kind == Tool::Highlighter ? 0.34F * opacity : opacity;
    resources.brush->SetColor(d2d_color(drawable.color, alpha));
    ID2D1SolidColorBrush* brush = resources.brush.Get();

    const auto local = [offset_x, offset_y, scale](PointF point) {
        return D2D1::Point2F((point.x - offset_x) * scale,
                             (point.y - offset_y) * scale);
    };

    if ((drawable.kind == Tool::Rectangle || drawable.kind == Tool::Screenshot) &&
        drawable.points.size() >= 2) {
        const auto a = local(drawable.points.front());
        const auto b = local(drawable.points.back());
        const auto rectangle = D2D1::RectF(std::min(a.x, b.x), std::min(a.y, b.y),
                                            std::max(a.x, b.x), std::max(a.y, b.y));
        if (drawable.kind == Tool::Screenshot) {
            if (resources.screenshot_shade)
                context->FillRectangle(rectangle, resources.screenshot_shade.Get());
        }
        context->DrawRectangle(rectangle, brush, render_width, resources.stroke_style.Get());
        return;
    }
    if (drawable.kind == Tool::Ellipse && drawable.points.size() >= 2) {
        const auto a = local(drawable.points.front());
        const auto b = local(drawable.points.back());
        const float left = std::min(a.x, b.x);
        const float top = std::min(a.y, b.y);
        const float right = std::max(a.x, b.x);
        const float bottom = std::max(a.y, b.y);
        context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F((left + right) * 0.5F,
                                                         (top + bottom) * 0.5F),
                                           (right - left) * 0.5F, (bottom - top) * 0.5F),
                             brush, render_width, resources.stroke_style.Get());
        return;
    }
    if (drawable.kind == Tool::Text) {
        const float font_size = std::clamp(
            drawable.width * 3.4F * scale,
            source_space ? 1.0F : 16.0F, 256.0F);
        if (auto* format = resources.text_format(graphics, font_size)) {
            const auto origin = local(drawable.points.front());
            const auto extent = drawable.points.size() >= 2
                ? local(drawable.points[1])
                : D2D1::Point2F(origin.x + 600.0F * scale,
                                origin.y + 400.0F * scale);
            context->DrawTextW(drawable.text.c_str(), static_cast<UINT32>(drawable.text.size()),
                               format, D2D1::RectF(origin.x, origin.y,
                                                  extent.x, extent.y), brush);
        }
        return;
    }

    if (drawable.kind == Tool::CurvedArrow && drawable.points.size() >= 2) {
        const auto curve = curved_arrow_bezier(
            drawable.points.front(), drawable.points.back(),
            drawable.reference_scale);
        ComPtr<ID2D1PathGeometry> geometry;
        graphics.d2d_factory()->CreatePathGeometry(geometry.GetAddressOf());
        if (!geometry) return;
        ComPtr<ID2D1GeometrySink> sink;
        geometry->Open(sink.GetAddressOf());
        sink->BeginFigure(local(curve.start), D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddBezier(D2D1::BezierSegment(local(curve.control1), local(curve.control2),
                                             local(curve.end)));
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
        sink->Close();
        context->DrawGeometry(geometry.Get(), brush, render_width,
                              resources.stroke_style.Get());
        const auto tangent_local = local(curve.control2);
        const auto end_local = local(curve.end);
        draw_arrow_head(context, brush,
                        {tangent_local.x, tangent_local.y},
                        {end_local.x, end_local.y}, render_width,
                        render_reference_scale);
        return;
    }

    if (drawable.points.size() == 1) {
        const auto point = local(drawable.points.front());
        context->FillEllipse(D2D1::Ellipse(point, render_width * 0.5F,
                                           render_width * 0.5F), brush);
        return;
    }

    ComPtr<ID2D1PathGeometry> geometry;
    graphics.d2d_factory()->CreatePathGeometry(geometry.GetAddressOf());
    if (!geometry) return;
    ComPtr<ID2D1GeometrySink> sink;
    geometry->Open(sink.GetAddressOf());
    sink->BeginFigure(local(drawable.points.front()), D2D1_FIGURE_BEGIN_HOLLOW);
    const bool smooth = drawable.points.size() >= 3 &&
        (drawable.kind == Tool::Pen || drawable.kind == Tool::Highlighter);
    if (smooth) {
        for (std::size_t index = 0; index + 1 < drawable.points.size(); ++index) {
            const PointF p0 = drawable.points[index == 0 ? 0 : index - 1];
            const PointF p1 = drawable.points[index];
            const PointF p2 = drawable.points[index + 1];
            const PointF p3 = drawable.points[
                std::min(index + 2, drawable.points.size() - 1)];
            const PointF c1{p1.x + (p2.x - p0.x) / 6.0F,
                            p1.y + (p2.y - p0.y) / 6.0F};
            const PointF c2{p2.x - (p3.x - p1.x) / 6.0F,
                            p2.y - (p3.y - p1.y) / 6.0F};
            sink->AddBezier(D2D1::BezierSegment(local(c1), local(c2), local(p2)));
        }
    } else {
        for (std::size_t index = 1; index < drawable.points.size(); ++index) {
            sink->AddLine(local(drawable.points[index]));
        }
    }
    sink->EndFigure(D2D1_FIGURE_END_OPEN);
    sink->Close();
    context->DrawGeometry(geometry.Get(), brush, render_width,
                          resources.stroke_style.Get());
    if (drawable.kind == Tool::Arrow && drawable.points.size() >= 2) {
        const auto before_local = local(drawable.points[drawable.points.size() - 2]);
        const auto end_local = local(drawable.points.back());
        draw_arrow_head(context, brush,
                        {before_local.x, before_local.y},
                        {end_local.x, end_local.y}, render_width,
                        render_reference_scale);
    }
}

RectF drawable_render_bounds(const Drawable& drawable) noexcept {
    RectF bounds = drawable.bounds();
    if (drawable.kind == Tool::Text && drawable.points.size() == 1) {
        bounds.right = std::max(bounds.right, drawable.points.front().x + 600.0F);
        bounds.bottom = std::max(bounds.bottom, drawable.points.front().y + 400.0F);
    }
    return bounds;
}

bool DocumentRenderCache::update(GraphicsDevice& graphics, Surface& surface,
                                 const Document& document, float offset_x,
                                 float offset_y, RectF viewport) {
    if (revision_ == document.revision() &&
        surface_generation_ == surface.generation() &&
        width_ == surface.width() && height_ == surface.height() &&
        offset_x_ == offset_x && offset_y_ == offset_y) {
        return true;
    }

    auto* context = surface.context();
    if (!context) return false;
    const bool same_surface = bitmap_ &&
        surface_generation_ == surface.generation() &&
        width_ == surface.width() && height_ == surface.height() &&
        offset_x_ == offset_x && offset_y_ == offset_y;
    if (!same_surface) {
        const auto properties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                              D2D1_ALPHA_MODE_PREMULTIPLIED),
            96.0F, 96.0F);
        bitmap_.Reset();
        if (FAILED(context->CreateBitmap(
                D2D1::SizeU(surface.width(), surface.height()), nullptr, 0,
                properties, bitmap_.GetAddressOf())) || !bitmap_) {
            return false;
        }
    }

    const auto render_items = [&](std::size_t start, std::size_t count,
                                  bool clear_first) {
        if (start + count > document.items().size()) return false;
        ComPtr<ID2D1Image> original_target;
        context->GetTarget(original_target.GetAddressOf());
        if (!original_target) return false;
        context->SetTarget(bitmap_.Get());
        context->BeginDraw();
        context->SetTransform(D2D1::Matrix3x2F::Identity());
        if (clear_first) context->Clear(D2D1::ColorF(0, 0.0F));
        DrawableRenderResources resources;
        const bool resources_ready = count == 0 ||
                                     resources.initialize(graphics, context);
        if (resources_ready) {
            for (std::size_t index = start; index < start + count; ++index) {
                const auto& drawable = document.items()[index];
                if (drawable.bounds().intersects(viewport)) {
                    draw_drawable(graphics, context, resources, drawable,
                                  offset_x, offset_y);
                }
            }
        }
        const HRESULT draw_result = context->EndDraw();
        context->SetTarget(original_target.Get());
        return resources_ready && SUCCEEDED(draw_result);
    };

    bool updated_incrementally = false;
    const bool consecutive = revision_ != std::numeric_limits<std::uint64_t>::max() &&
                             document.revision() == revision_ + 1;
    if (same_surface && consecutive) {
        const auto change = document.last_change_kind();
        const std::size_t change_index = document.last_change_index();
        if (change == DocumentChangeKind::Append &&
            change_index == item_count_ && document.items().size() == item_count_ + 1) {
            updated_incrementally = render_items(item_count_, 1, false);
        } else if (change == DocumentChangeKind::Clear && document.empty()) {
            updated_incrementally = render_items(0, 0, true);
        }
    }

    if (!updated_incrementally) {
        if (!render_items(0, document.items().size(), true)) return false;
    }

    revision_ = document.revision();
    surface_generation_ = surface.generation();
    width_ = surface.width();
    height_ = surface.height();
    offset_x_ = offset_x;
    offset_y_ = offset_y;
    item_count_ = document.items().size();
    return true;
}

void DocumentRenderCache::draw(ID2D1DeviceContext* context) const {
    if (bitmap_) context->DrawBitmap(bitmap_.Get());
}

void DocumentRenderCache::reset() noexcept {
    bitmap_.Reset();
    revision_ = std::numeric_limits<std::uint64_t>::max();
    surface_generation_ = 0;
    width_ = 0;
    height_ = 0;
    offset_x_ = 0.0F;
    offset_y_ = 0.0F;
    item_count_ = 0;
}

int WorldDocumentRenderCache::tile_coordinate(float value) noexcept {
    if (!std::isfinite(value)) return 0;
    const double coordinate = std::floor(
        static_cast<double>(value) / static_cast<double>(kTileSize));
    return static_cast<int>(std::clamp(
        coordinate, static_cast<double>(std::numeric_limits<int>::min()),
        static_cast<double>(std::numeric_limits<int>::max())));
}

RectF WorldDocumentRenderCache::tile_bounds(WorldTileKey key) noexcept {
    const float left = static_cast<float>(key.x) * static_cast<float>(kTileSize);
    const float top = static_cast<float>(key.y) * static_cast<float>(kTileSize);
    return {left, top, left + static_cast<float>(kTileSize),
            top + static_cast<float>(kTileSize)};
}

bool WorldDocumentRenderCache::ensure_tile(ID2D1DeviceContext* context,
                                            WorldTileKey key,
                                            ID2D1Bitmap1** bitmap) {
    if (!context || !bitmap) return false;
    const auto found = tiles_.find(key);
    if (found != tiles_.end()) {
        *bitmap = found->second.Get();
        return true;
    }
    if (tiles_.size() >= kMaxTileCount) {
        capacity_exhausted_ = true;
        return false;
    }
    const auto properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                          D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0F, 96.0F);
    ComPtr<ID2D1Bitmap1> created;
    const HRESULT result = context->CreateBitmap(
        D2D1::SizeU(static_cast<UINT>(kTileSize),
                    static_cast<UINT>(kTileSize)),
        nullptr, 0, properties, created.GetAddressOf());
    if (FAILED(result) || !created) return false;
    auto [iterator, inserted] = tiles_.emplace(key, std::move(created));
    if (!inserted) return false;
    *bitmap = iterator->second.Get();
    return true;
}

bool WorldDocumentRenderCache::append(GraphicsDevice& graphics,
                                      ID2D1DeviceContext* context,
                                      const Drawable& drawable) {
    const RectF bounds = drawable_render_bounds(drawable);
    const int first_x = tile_coordinate(bounds.left);
    const int last_x = tile_coordinate(bounds.right);
    const int first_y = tile_coordinate(bounds.top);
    const int last_y = tile_coordinate(bounds.bottom);
    ComPtr<ID2D1Image> original_target;
    context->GetTarget(original_target.GetAddressOf());
    if (!original_target) return false;
    bool success = true;
    for (int y = first_y; y <= last_y && success; ++y) {
        for (int x = first_x; x <= last_x; ++x) {
            const WorldTileKey key{x, y};
            const bool existed = tiles_.contains(key);
            ID2D1Bitmap1* tile{};
            if (!ensure_tile(context, key, &tile) || !tile) {
                success = false;
                break;
            }
            context->SetTarget(tile);
            context->BeginDraw();
            context->SetTransform(D2D1::Matrix3x2F::Identity());
            if (!existed) context->Clear(D2D1::ColorF(0, 0.0F));
            DrawableRenderResources resources;
            if (!resources.initialize(graphics, context)) {
                success = false;
            } else {
                const RectF tile_rect = tile_bounds(key);
                draw_drawable(graphics, context, resources, drawable,
                              tile_rect.left, tile_rect.top,
                              1.0F, 1.0F, true);
            }
            const HRESULT result = context->EndDraw();
            if (FAILED(result)) success = false;
        }
    }
    context->SetTarget(original_target.Get());
    return success;
}

bool WorldDocumentRenderCache::rebuild(GraphicsDevice& graphics,
                                       ID2D1DeviceContext* context,
                                       const Document& document) {
    tiles_.clear();
    capacity_exhausted_ = false;
    for (const auto& drawable : document.items()) {
        const RectF bounds = drawable_render_bounds(drawable);
        const int first_x = tile_coordinate(bounds.left);
        const int last_x = tile_coordinate(bounds.right);
        const int first_y = tile_coordinate(bounds.top);
        const int last_y = tile_coordinate(bounds.bottom);
        for (int y = first_y; y <= last_y; ++y) {
            for (int x = first_x; x <= last_x; ++x) {
                ID2D1Bitmap1* unused{};
                if (!ensure_tile(context, {x, y}, &unused)) return false;
            }
        }
    }
    ComPtr<ID2D1Image> original_target;
    context->GetTarget(original_target.GetAddressOf());
    if (!original_target) return false;
    bool success = true;
    for (auto& [key, bitmap] : tiles_) {
        context->SetTarget(bitmap.Get());
        context->BeginDraw();
        context->SetTransform(D2D1::Matrix3x2F::Identity());
        context->Clear(D2D1::ColorF(0, 0.0F));
        DrawableRenderResources resources;
        if (!document.empty() && !resources.initialize(graphics, context)) {
            success = false;
        } else {
            const RectF tile_rect = tile_bounds(key);
            for (const auto& drawable : document.items()) {
                if (drawable_render_bounds(drawable).intersects(tile_rect)) {
                    draw_drawable(graphics, context, resources, drawable,
                                  tile_rect.left, tile_rect.top,
                                  1.0F, 1.0F, true);
                }
            }
        }
        const HRESULT result = context->EndDraw();
        if (FAILED(result)) success = false;
        if (!success) break;
    }
    context->SetTarget(original_target.Get());
    if (!success) tiles_.clear();
    return success;
}

bool WorldDocumentRenderCache::update(GraphicsDevice& graphics, Surface& surface,
                                      const Document& document) {
    auto* context = surface.context();
    if (!context) return false;
    if (surface_generation_ != surface.generation()) reset();
    if (capacity_failed_revision_ == document.revision() &&
        capacity_failed_generation_ == surface.generation()) return false;
    if (revision_ == document.revision()) return true;

    bool updated = false;
    const bool consecutive = revision_ != std::numeric_limits<std::uint64_t>::max() &&
                             document.revision() == revision_ + 1;
    if (consecutive && document.last_change_kind() == DocumentChangeKind::Append &&
        document.last_change_index() == item_count_ &&
        document.items().size() == item_count_ + 1) {
        updated = append(graphics, context, document.items().back());
    } else if (consecutive &&
               document.last_change_kind() == DocumentChangeKind::Clear &&
               document.empty()) {
        tiles_.clear();
        updated = true;
    }
    if (!updated) updated = rebuild(graphics, context, document);
    if (!updated) {
        if (capacity_exhausted_) {
            // Do not allocate and discard the same oversized atlas every frame.
            // A document revision or device/surface generation change retries it.
            tiles_.clear();
            capacity_failed_revision_ = document.revision();
            capacity_failed_generation_ = surface.generation();
            surface_generation_ = surface.generation();
        }
        return false;
    }
    revision_ = document.revision();
    surface_generation_ = surface.generation();
    item_count_ = document.items().size();
    capacity_failed_revision_ = std::numeric_limits<std::uint64_t>::max();
    capacity_failed_generation_ = 0;
    capacity_exhausted_ = false;
    return true;
}

void WorldDocumentRenderCache::draw(
        ID2D1DeviceContext* context,
        const ZoomViewportTransform& transform) const {
    if (!context) return;
    for (const auto& [key, bitmap] : tiles_) {
        const RectF world = tile_bounds(key);
        if (!world.intersects(transform.source)) continue;
        const auto top_left = transform.source_to_view({world.left, world.top});
        const auto bottom_right = transform.source_to_view({world.right, world.bottom});
        context->DrawBitmap(
            bitmap.Get(),
            D2D1::RectF(top_left.x, top_left.y, bottom_right.x, bottom_right.y),
            1.0F, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
            D2D1::RectF(0.0F, 0.0F, static_cast<float>(kTileSize),
                        static_cast<float>(kTileSize)));
    }
}

void WorldDocumentRenderCache::reset() noexcept {
    tiles_.clear();
    revision_ = std::numeric_limits<std::uint64_t>::max();
    surface_generation_ = 0;
    item_count_ = 0;
    capacity_failed_revision_ = std::numeric_limits<std::uint64_t>::max();
    capacity_failed_generation_ = 0;
    capacity_exhausted_ = false;
}

bool WindowBase::create(const wchar_t* class_name, const wchar_t* title, DWORD ex_style,
                        DWORD style, const RECT& bounds, HWND parent) {
    WNDCLASSEXW existing{};
    existing.cbSize = sizeof(existing);
    if (!GetClassInfoExW(GetModuleHandleW(nullptr), class_name, &existing)) {
        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.style = CS_HREDRAW | CS_VREDRAW;
        window_class.lpfnWndProc = &WindowBase::window_proc;
        window_class.hInstance = GetModuleHandleW(nullptr);
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101));
        window_class.hIconSm = window_class.hIcon;
        window_class.lpszClassName = class_name;
        if (!RegisterClassExW(&window_class)) return false;
    }
    window_ = CreateWindowExW(ex_style, class_name, title, style,
                              bounds.left, bounds.top, bounds.right - bounds.left,
                              bounds.bottom - bounds.top, parent, nullptr,
                              GetModuleHandleW(nullptr), this);
    return window_ != nullptr;
}

bool WindowBase::initialize_surface(GraphicsDevice& graphics) {
    RECT client{};
    GetClientRect(window_, &client);
    std::wstring error;
    if (!surface_.initialize(graphics, window_, static_cast<UINT>(client.right),
                             static_cast<UINT>(client.bottom), error)) {
        controller_.report_runtime_error(error);
        return false;
    }
    return true;
}

LRESULT WindowBase::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_MOUSEWHEEL &&
        (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 &&
        (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) {
        const SHORT delta = GET_WHEEL_DELTA_WPARAM(wparam);
        if (delta != 0) controller_.adjust_thickness_step(delta > 0 ? 1 : -1);
        return 0;
    }
    if (message == WM_TIMER && wparam == Surface::kPresentRetryTimer) {
        KillTimer(window_, Surface::kPresentRetryTimer);
        invalidate();
        return 0;
    }
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_SIZE && surface_.context()) {
        std::wstring error;
        if (!surface_.resize(LOWORD(lparam), HIWORD(lparam), error)) {
            controller_.report_runtime_error(error);
        }
        invalidate();
        return 0;
    }
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        BeginPaint(window_, &paint);
        render();
        EndPaint(window_, &paint);
        return 0;
    }
    return DefWindowProcW(window_, message, wparam, lparam);
}

LRESULT CALLBACK WindowBase::window_proc(HWND window, UINT message, WPARAM wparam,
                                         LPARAM lparam) {
    WindowBase* self = reinterpret_cast<WindowBase*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* creation = reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = static_cast<WindowBase*>(creation->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self) return DefWindowProcW(window, message, wparam, lparam);
    if (message == WM_NCDESTROY) {
        const LRESULT result = self->handle_message(message, wparam, lparam);
        self->window_ = nullptr;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        return result;
    }
    return self->handle_message(message, wparam, lparam);
}

bool OverlayWindow::initialize(GraphicsDevice& graphics) {
    constexpr DWORD ex_style = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                               WS_EX_LAYERED;
    if (!create(L"ElitePen.Overlay", L"Elite Pen Overlay", ex_style, WS_POPUP,
                monitor_rect_)) return false;
    SetLayeredWindowAttributes(window_, 0, 255, LWA_ALPHA);
    if (!initialize_surface(graphics)) return false;
    refresh_pencil_cursor();
    SetWindowDisplayAffinity(window_, WDA_NONE);
    ShowWindow(window_, SW_SHOWNOACTIVATE);
    update_interaction();
    return true;
}

void OverlayWindow::refresh_pencil_cursor() {
    HCURSOR next = create_pencil_cursor(GetDpiForWindow(window_));
    if (!next) return;
    if (pencil_cursor_) DestroyCursor(pencil_cursor_);
    pencil_cursor_ = next;
}

void OverlayWindow::update_interaction() {
    LONG_PTR style = GetWindowLongPtrW(window_, GWL_EXSTYLE);
    const bool transparent = controller_.state().tool == Tool::Interact ||
                             controller_.zoom_active();
    const bool was_transparent = (style & WS_EX_TRANSPARENT) != 0;
    if (transparent == was_transparent) return;
    if (transparent) style |= WS_EX_TRANSPARENT;
    else style &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
    SetWindowLongPtrW(window_, GWL_EXSTYLE, style);
    SetWindowPos(window_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void OverlayWindow::cancel_gesture() {
    if (!drawing_ && !erasing_) return;
    drawing_ = false;
    erasing_ = false;
    pointer_active_ = false;
    if (GetCapture() == window_) ReleaseCapture();
    controller_.state().document.end_compound();
    controller_.preview().reset();
    controller_.invalidate_document();
}

PointF OverlayWindow::global_point(LPARAM lparam) const noexcept {
    return {static_cast<float>(GET_X_LPARAM(lparam) + monitor_rect_.left),
            static_cast<float>(GET_Y_LPARAM(lparam) + monitor_rect_.top)};
}

std::optional<PointF> OverlayWindow::pointer_point(WPARAM wparam) const noexcept {
    POINTER_INFO info{};
    if (!GetPointerInfo(GET_POINTERID_WPARAM(wparam), &info)) return std::nullopt;
    return PointF{static_cast<float>(info.ptPixelLocation.x),
                  static_cast<float>(info.ptPixelLocation.y)};
}

void OverlayWindow::begin_gesture(PointF point, float pressure) {
    if (controller_.route_palette_command(point)) return;
    const Tool tool = current_gesture_tool(controller_.state().tool);
    if (tool == Tool::Eraser) {
        controller_.state().document.begin_compound();
        erasing_ = true;
        if (controller_.state().document.erase_at(point,
                std::max(12.0F, controller_.state().thickness * 2.5F)))
            controller_.invalidate_document();
        drawing_ = true;
        SetCapture(window_);
        return;
    }
    if (tool == Tool::Text) {
        controller_.begin_text(point);
        return;
    }
    if (!is_drawing_tool(tool)) return;
    Drawable drawable;
    drawable.kind = tool;
    drawable.color = controller_.state().color;
    drawable.width = controller_.state().effective_width() *
        std::clamp(pressure, 0.35F, 1.45F);
    drawable.points.push_back(point);
    if (tool == Tool::Line || tool == Tool::Rectangle || tool == Tool::Ellipse ||
        tool == Tool::Arrow || tool == Tool::CurvedArrow || tool == Tool::Screenshot) {
        drawable.points.push_back(point);
    }
    controller_.preview() = std::move(drawable);
    drawing_ = true;
    SetCapture(window_);
    controller_.invalidate_preview();
}

void OverlayWindow::update_gesture(PointF point, WPARAM keys) {
    if (!drawing_) return;
    if (controller_.state().tool == Tool::Eraser) {
        if ((keys & MK_LBUTTON) && controller_.state().document.erase_at(
                point, std::max(12.0F, controller_.state().thickness * 2.5F)))
            controller_.invalidate_document();
        return;
    }
    auto& preview = controller_.preview();
    if (!preview) return;
    preview->invalidate_bounds_cache();
    const Tool tool = preview->kind;
    if (tool == Tool::Line || tool == Tool::Rectangle || tool == Tool::Ellipse ||
        tool == Tool::Arrow || tool == Tool::CurvedArrow || tool == Tool::Screenshot) {
        if ((keys & MK_SHIFT) && (tool == Tool::Rectangle || tool == Tool::Ellipse)) {
            const PointF origin = preview->points.front();
            const float dx = point.x - origin.x;
            const float dy = point.y - origin.y;
            const float size = std::max(std::abs(dx), std::abs(dy));
            point.x = origin.x + std::copysign(size, dx == 0 ? 1.0F : dx);
            point.y = origin.y + std::copysign(size, dy == 0 ? 1.0F : dy);
        }
        preview->points.back() = point;
    } else if (distance(preview->points.back(), point) >= 1.0F) {
        if (preview->points.size() >= 8192) {
            std::vector<PointF> reduced;
            reduced.reserve(preview->points.size() / 2 + 1);
            for (std::size_t index = 0; index < preview->points.size(); index += 2) {
                reduced.push_back(preview->points[index]);
            }
            if (reduced.back().x != preview->points.back().x ||
                reduced.back().y != preview->points.back().y) {
                reduced.push_back(preview->points.back());
            }
            preview->points = std::move(reduced);
        }
        preview->points.push_back(point);
    }
    controller_.invalidate_preview();
}

void OverlayWindow::finish_gesture(PointF point, WPARAM keys) {
    if (!drawing_) return;
    update_gesture(point, keys);
    drawing_ = false;
    if (GetCapture() == window_) ReleaseCapture();
    if (erasing_) {
        controller_.state().document.end_compound();
        erasing_ = false;
        controller_.restack_palette();
        controller_.invalidate_document();
        return;
    }
    auto& preview = controller_.preview();
    if (!preview) return;
    Drawable completed = std::move(*preview);
    preview.reset();
    if (completed.kind == Tool::Screenshot && completed.points.size() >= 2) {
        controller_.invalidate_document();
        UpdateWindow(window_);
        controller_.capture_region(completed.points.front(), completed.points.back());
        controller_.set_tool(Tool::Interact);
        return;
    }
    if (completed.kind == Tool::Pen || completed.kind == Tool::Highlighter) {
        completed.points = simplify_path(completed.points,
            std::clamp(completed.width * 0.08F, 0.5F, 2.0F));
    }
    if (completed.points.size() >= 2 &&
        distance(completed.points.front(), completed.points.back()) < 2.0F) {
        completed.points.resize(1);
    }
    controller_.commit_drawable(std::move(completed));
    controller_.restack_palette();
}

LRESULT OverlayWindow::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case kQaQueryDrawingCursorMessage:
            return reinterpret_cast<LRESULT>(pencil_cursor_);
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_SETCURSOR:
            if (LOWORD(lparam) == HTCLIENT) {
                const Tool tool = current_gesture_tool(controller_.state().tool);
                HCURSOR cursor{};
                if (tool == Tool::Interact) cursor = LoadCursorW(nullptr, IDC_ARROW);
                else if (tool == Tool::Text) cursor = LoadCursorW(nullptr, IDC_IBEAM);
                else if ((tool == Tool::Pen || tool == Tool::Highlighter) && pencil_cursor_)
                    cursor = pencil_cursor_;
                else cursor = LoadCursorW(nullptr, IDC_CROSS);
                SetCursor(cursor);
                return TRUE;
            }
            break;
        case WM_DPICHANGED:
            refresh_pencil_cursor();
            return 0;
        case WM_NCHITTEST:
            if (controller_.palette() && controller_.palette()->contains_screen_point(
                    {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)})) {
                return HTTRANSPARENT;
            }
            if (controller_.state().tool == Tool::Interact || controller_.zoom_active())
                return HTTRANSPARENT;
            return HTCLIENT;
        case WM_LBUTTONDOWN:
            if (pointer_active_) return 0;
            begin_gesture(global_point(lparam));
            return 0;
        case WM_MOUSEMOVE:
            if (pointer_active_) return 0;
            update_gesture(global_point(lparam), wparam);
            return 0;
        case WM_LBUTTONUP:
            if (pointer_active_) return 0;
            finish_gesture(global_point(lparam), wparam);
            return 0;
        case WM_POINTERDOWN: {
            const UINT32 pointer_id = GET_POINTERID_WPARAM(wparam);
            POINTER_INPUT_TYPE type{};
            if (!GetPointerType(pointer_id, &type) ||
                (type != PT_PEN && type != PT_TOUCH)) return 0;
            const auto point = pointer_point(wparam);
            if (!point) return 0;
            float pressure = 1.0F;
            if (type == PT_PEN) {
                POINTER_PEN_INFO pen{};
                if (GetPointerPenInfo(pointer_id, &pen) &&
                    (pen.penMask & PEN_MASK_PRESSURE) != 0) {
                    const float normalized = static_cast<float>(pen.pressure) / 1024.0F;
                    pressure = 0.45F + normalized * 1.0F;
                }
            }
            pointer_active_ = true;
            pointer_id_ = pointer_id;
            begin_gesture(*point, pressure);
            return 0;
        }
        case WM_POINTERUPDATE: {
            if (!pointer_active_ || GET_POINTERID_WPARAM(wparam) != pointer_id_) return 0;
            const auto point = pointer_point(wparam);
            if (point) {
                WPARAM keys = MK_LBUTTON;
                if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) keys |= MK_SHIFT;
                update_gesture(*point, keys);
            }
            return 0;
        }
        case WM_POINTERUP: {
            if (!pointer_active_ || GET_POINTERID_WPARAM(wparam) != pointer_id_) return 0;
            const auto point = pointer_point(wparam);
            if (point) finish_gesture(*point, 0);
            pointer_active_ = false;
            pointer_id_ = 0;
            return 0;
        }
        case WM_POINTERCAPTURECHANGED:
            pointer_active_ = false;
            pointer_id_ = 0;
            if (drawing_ && controller_.preview())
                finish_gesture(controller_.preview()->points.back(), 0);
            else if (erasing_) cancel_gesture();
            return 0;
        case WM_CAPTURECHANGED:
            if (drawing_ && controller_.preview())
                finish_gesture(controller_.preview()->points.back(), 0);
            else if (erasing_) cancel_gesture();
            drawing_ = false;
            return 0;
        case WM_RBUTTONDOWN:
            controller_.stop_transient_mode();
            return 0;
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) controller_.stop_transient_mode();
            return 0;
        default:
            return WindowBase::handle_message(message, wparam, lparam);
    }
    return WindowBase::handle_message(message, wparam, lparam);
}

void OverlayWindow::render() {
    const RectF viewport{static_cast<float>(monitor_rect_.left),
                         static_cast<float>(monitor_rect_.top),
                         static_cast<float>(monitor_rect_.right),
                         static_cast<float>(monitor_rect_.bottom)};
    const bool annotations_visible = controller_.state().annotations_visible;
    const bool cache_ready = !annotations_visible || document_cache_.update(
        controller_.graphics(), surface_, controller_.state().document,
        static_cast<float>(monitor_rect_.left),
        static_cast<float>(monitor_rect_.top), viewport);
    const D2D1_COLOR_F background = controller_.state().whiteboard
        ? D2D1::ColorF(D2D1::ColorF::White)
        : (controller_.state().blackboard ? D2D1::ColorF(0x111318)
                                          : D2D1::ColorF(0, 0.0F));
    auto* context = surface_.begin_draw(background);
    if (!context) return;
    if (annotations_visible) {
        if (cache_ready) {
            document_cache_.draw(context);
        } else {
            DrawableRenderResources fallback;
            if (fallback.initialize(controller_.graphics(), context)) {
                for (const auto& drawable : controller_.state().document.items()) {
                    if (drawable.bounds().intersects(viewport)) {
                        draw_drawable(controller_.graphics(), context, fallback, drawable,
                                      static_cast<float>(monitor_rect_.left),
                                      static_cast<float>(monitor_rect_.top));
                    }
                }
            }
        }
        DrawableRenderResources live;
        const bool has_live_content = !controller_.transient_drawables().empty() ||
                                      controller_.preview().has_value();
        const bool live_ready = has_live_content &&
                                live.initialize(controller_.graphics(), context);
        const std::uint64_t now = monotonic_milliseconds();
        for (const auto& transient : controller_.transient_drawables()) {
            if (!live_ready || !transient.drawable.bounds().intersects(viewport)) continue;
            const std::uint64_t remaining = transient.expires_at_ms > now
                ? transient.expires_at_ms - now : 0;
            const float opacity = remaining >= 1200U ? 1.0F
                : static_cast<float>(remaining) / 1200.0F;
            draw_drawable(controller_.graphics(), context, live, transient.drawable,
                          static_cast<float>(monitor_rect_.left),
                          static_cast<float>(monitor_rect_.top), opacity);
        }
        if (live_ready && controller_.preview() &&
            controller_.preview()->bounds().intersects(viewport)) {
            draw_drawable(controller_.graphics(), context, live, *controller_.preview(),
                          static_cast<float>(monitor_rect_.left),
                          static_cast<float>(monitor_rect_.top), 0.82F);
        }
    }
    if (controller_.state().cursor_highlight) {
        const auto& theme = current_ui_theme();
        POINT cursor{};
        GetCursorPos(&cursor);
        if (cursor.x >= monitor_rect_.left && cursor.x < monitor_rect_.right &&
            cursor.y >= monitor_rect_.top && cursor.y < monitor_rect_.bottom) {
            const auto center = D2D1::Point2F(
                static_cast<float>(cursor.x - monitor_rect_.left),
                static_cast<float>(cursor.y - monitor_rect_.top));
            ComPtr<ID2D1SolidColorBrush> halo;
            ComPtr<ID2D1SolidColorBrush> ring;
            context->CreateSolidColorBrush(theme_color(theme.mint, 0.20F), halo.GetAddressOf());
            context->CreateSolidColorBrush(theme_color(theme.mint, 0.90F), ring.GetAddressOf());
            context->FillEllipse(D2D1::Ellipse(center, 25, 25), halo.Get());
            context->DrawEllipse(D2D1::Ellipse(center, 25, 25), ring.Get(), 3.0F);
        }
    }
    std::wstring error;
    if (!surface_.end_draw(error)) controller_.report_runtime_error(error);
}

bool PaletteWindow::initialize(GraphicsDevice& graphics) {
    const int work_left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int work_top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    scale_ = palette_scale_for_size(controller_.preferences().palette_size);
    mode_ = controller_.preferences().control_mode;
    collapsed_ = controller_.preferences().palette_collapsed;
    const RECT bounds{work_left + 28, work_top + 28,
                      work_left + 28 + pixel_width(), work_top + 28 + pixel_height()};
    // DirectComposition owns the palette pixels, including their premultiplied
    // alpha. A legacy layered-window backing bitmap can become opaque after the
    // swap chain is resized, exposing the window as a black rectangle. Keeping
    // the HWND out of the redirection surface lets DirectComposition preserve
    // per-pixel transparency at every palette scale.
    constexpr DWORD ex_style = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                               WS_EX_NOREDIRECTIONBITMAP;
    if (!create(L"ElitePen.Palette", L"Elite Pen", ex_style, WS_POPUP, bounds)) return false;
    if (!initialize_surface(graphics)) return false;
#ifndef ELITE_PEN_DEBUG
    if (controller_.preferences().exclude_palette_from_capture)
        SetWindowDisplayAffinity(window_, WDA_EXCLUDEFROMCAPTURE);
#endif
    ShowWindow(window_, SW_SHOWNOACTIVATE);
    install_tooltips();
    install_hotkeys();
    add_tray_icon();
    SetTimer(window_, 20, 33, nullptr);
    return true;
}

void PaletteWindow::apply_size(int size) {
    const float next_scale = palette_scale_for_size(size);
    if (std::abs(next_scale - scale_) < 0.001F) return;

    RECT current{};
    if (!GetWindowRect(window_, &current)) return;
    const int next_width = control_pixel_width(mode_, next_scale, collapsed_);
    const int next_height = control_pixel_height(mode_, next_scale, collapsed_);
    const int center_x = current.left + (current.right - current.left) / 2;
    const int center_y = current.top + (current.bottom - current.top) / 2;
    RECT desired{center_x - next_width / 2, center_y - next_height / 2,
                 center_x - next_width / 2 + next_width,
                 center_y - next_height / 2 + next_height};
    HMONITOR monitor = MonitorFromRect(&current, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    desired.left = static_cast<LONG>(std::clamp(
        static_cast<int>(desired.left), static_cast<int>(info.rcWork.left),
        static_cast<int>(info.rcWork.right) - next_width));
    desired.top = static_cast<LONG>(std::clamp(
        static_cast<int>(desired.top), static_cast<int>(info.rcWork.top),
        static_cast<int>(info.rcWork.bottom) - next_height));

    scale_ = next_scale;
    SetWindowPos(window_, nullptr, desired.left, desired.top, next_width, next_height,
                 SWP_NOACTIVATE | SWP_NOZORDER);
    if (tooltip_) {
        DestroyWindow(tooltip_);
        tooltip_ = nullptr;
    }
    install_tooltips();
    invalidate();
}

void PaletteWindow::apply_mode(ControlMode mode) {
    if (mode_ == mode || !window_) return;
    RECT current{};
    if (!GetWindowRect(window_, &current)) return;
    const int center_x = current.left + (current.right - current.left) / 2;
    const int center_y = current.top + (current.bottom - current.top) / 2;
    mode_ = mode;
    hovered_linear_item_ = -1;
    tracking_mouse_ = false;
    const int width = pixel_width();
    const int height = pixel_height();
    HMONITOR monitor = MonitorFromRect(&current, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    const int x = std::clamp(center_x - width / 2,
                             static_cast<int>(info.rcWork.left),
                             static_cast<int>(info.rcWork.right) - width);
    const int y = std::clamp(center_y - height / 2,
                             static_cast<int>(info.rcWork.top),
                             static_cast<int>(info.rcWork.bottom) - height);
    SetWindowPos(window_, HWND_TOPMOST, x, y, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    if (tooltip_) {
        DestroyWindow(tooltip_);
        tooltip_ = nullptr;
    }
    install_tooltips();
    invalidate();
}

void PaletteWindow::set_collapsed(bool collapsed) {
    if (collapsed_ == collapsed || !window_) return;
    RECT current{};
    if (!GetWindowRect(window_, &current)) return;
    const int center_x = current.left + (current.right - current.left) / 2;
    const int center_y = current.top + (current.bottom - current.top) / 2;
    collapsed_ = collapsed;
    controller_.preferences().palette_collapsed = collapsed_;
    const int width = pixel_width();
    const int height = pixel_height();
    HMONITOR monitor = MonitorFromRect(&current, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    const int x = std::clamp(center_x - width / 2,
                             static_cast<int>(info.rcWork.left),
                             static_cast<int>(info.rcWork.right) - width);
    const int y = std::clamp(center_y - height / 2,
                             static_cast<int>(info.rcWork.top),
                             static_cast<int>(info.rcWork.bottom) - height);
    SetWindowPos(window_, HWND_TOPMOST, x, y, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    if (tooltip_) {
        DestroyWindow(tooltip_);
        tooltip_ = nullptr;
    }
    install_tooltips();
    controller_.save_palette_position();
    controller_.save_preferences();
    invalidate();
}

void PaletteWindow::install_tooltips() {
    tooltip_ = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                               WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                               window_, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!tooltip_) return;
    SetWindowPos(tooltip_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SendMessageW(tooltip_, TTM_SETMAXTIPWIDTH, 0, 340);
    if (collapsed_) {
        TOOLINFOW information{};
        information.cbSize = sizeof(information);
        information.uFlags = TTF_SUBCLASS | TTF_TRANSPARENT;
        information.hwnd = window_;
        information.uId = 50;
        const int width = pixel_width();
        const int height = pixel_height();
        const float center_factor_x = mode_ == ControlMode::Linear ? 0.50F : 0.52F;
        const float center_factor_y = mode_ == ControlMode::Linear ? 0.50F : 0.46F;
        const float radius_factor = mode_ == ControlMode::Linear ? 0.28F : 0.17F;
        const int center_x = static_cast<int>(std::lround(
            static_cast<float>(width) * center_factor_x));
        const int center_y = static_cast<int>(std::lround(
            static_cast<float>(height) * center_factor_y));
        const int radius = std::max(7, static_cast<int>(std::lround(
            static_cast<float>(std::min(width, height)) * radius_factor)));
        information.rect = {center_x - radius, center_y - radius,
                            center_x + radius, center_y + radius};
        information.lpszText = const_cast<wchar_t*>(L"Expandir Elite Pen");
        SendMessageW(tooltip_, TTM_ADDTOOLW, 0,
                     reinterpret_cast<LPARAM>(&information));
        return;
    }
    if (mode_ == ControlMode::Linear) {
        UINT id = 1;
        const auto add_tip = [&](RECT logical, const wchar_t* text) {
            TOOLINFOW information{};
            information.cbSize = sizeof(information);
            information.uFlags = TTF_SUBCLASS | TTF_TRANSPARENT;
            information.hwnd = window_;
            information.uId = id++;
            information.rect = palette_scaled_rect(logical, scale_);
            information.lpszText = const_cast<wchar_t*>(text);
            SendMessageW(tooltip_, TTM_ADDTOOLW, 0,
                         reinterpret_cast<LPARAM>(&information));
        };
        add_tip(kLinearCollapseBounds, L"Contraer Elite Pen");
        for (const auto& item : kLinearItems) add_tip(item.bounds, item.tooltip);
        add_tip(kLinearThicknessBounds, L"Grosor del trazo");
        add_tip({9, 598, 67, 668}, L"Colores rapidos y selector completo");
        return;
    }
    struct Tip { UINT id; RECT bounds; const wchar_t* text; };
    constexpr std::array tips{
        Tip{1, {7, 11, 41, 144}, L"Grosor del trazo"},
        Tip{2, {50, 90, 84, 124}, L"Negro"},
        Tip{3, {52, 43, 86, 77}, L"Amarillo"},
        Tip{4, {102, 12, 136, 46}, L"Azul"},
        Tip{5, {152, 36, 186, 70}, L"Rojo"},
        Tip{6, {167, 79, 201, 113}, L"Verde"},
        Tip{7, {143, 113, 177, 147}, L"Morado"},
        Tip{8, {105, 132, 135, 162}, L"Mas colores"},
        Tip{9, {104, 61, 148, 105}, L"Ocultar o mostrar anotaciones"},
        Tip{17, {114, 101, 138, 125}, L"Contraer Elite Pen"},
        Tip{10, {42, 120, 86, 174}, L"Alternar entre lapiz y cursor normal"},
        Tip{11, {88, 151, 116, 184}, L"Pizarra blanca (clic) o negra (clic derecho)"},
        Tip{12, {102, 159, 213, 234}, L"Abrir herramientas y configuracion"},
        Tip{16, {203, 205, 239, 256}, L"Papelera: limpiar las anotaciones del contexto actual"}
    };
    for (const auto& tip : tips) {
        TOOLINFOW information{};
        information.cbSize = sizeof(information);
        information.uFlags = TTF_SUBCLASS | TTF_TRANSPARENT;
        information.hwnd = window_;
        information.uId = tip.id;
        information.rect = palette_scaled_rect(tip.bounds, scale_);
        information.lpszText = const_cast<wchar_t*>(tip.text);
        SendMessageW(tooltip_, TTM_ADDTOOLW, 0,
                     reinterpret_cast<LPARAM>(&information));
    }
}

void PaletteWindow::install_hotkeys() {
    // UI automation can run alongside a normal or recently terminated copy.
    // It invokes the same commands directly and must not steal the user's
    // machine-wide shortcuts merely to exercise unrelated interaction paths.
    wchar_t qa_instance[2]{};
    if (GetEnvironmentVariableW(L"ELITE_PEN_QA_INSTANCE_ID", qa_instance,
                                static_cast<DWORD>(std::size(qa_instance))) > 0) {
        registered_hotkeys_ = controller_.preferences().hotkeys;
        hotkeys_registered_ = true;
        return;
    }
    if (!apply_hotkeys(controller_.preferences().hotkeys)) {
        controller_.preferences().hotkeys = kDefaultHotkeys;
        apply_hotkeys(kDefaultHotkeys);
        controller_.save_preferences();
    }
}

bool PaletteWindow::apply_hotkeys(
        const std::array<HotkeyBinding, kHotkeyActionCount>& bindings) {
    const auto previous = registered_hotkeys_;
    const bool had_previous = hotkeys_registered_;
    remove_hotkeys();
    for (std::size_t index = 0; index < kGlobalHotkeyActionCount; ++index) {
        if (bindings[index].virtual_key == 0) continue;
        const int id = static_cast<int>(index) + 1;
        if (!RegisterHotKey(window_, id, bindings[index].modifiers | MOD_NOREPEAT,
                            bindings[index].virtual_key)) {
            for (int registered = 1;
                 registered <= static_cast<int>(kGlobalHotkeyActionCount); ++registered)
                UnregisterHotKey(window_, registered);
            hotkeys_registered_ = false;
            if (had_previous) {
                bool restored = true;
                for (std::size_t restore = 0; restore < kGlobalHotkeyActionCount; ++restore) {
                    if (previous[restore].virtual_key == 0) continue;
                    restored = RegisterHotKey(window_, static_cast<int>(restore) + 1,
                        previous[restore].modifiers | MOD_NOREPEAT,
                        previous[restore].virtual_key) != FALSE && restored;
                }
                if (restored) {
                    registered_hotkeys_ = previous;
                    hotkeys_registered_ = true;
                } else {
                    for (int restore = 1;
                         restore <= static_cast<int>(kGlobalHotkeyActionCount); ++restore)
                        UnregisterHotKey(window_, restore);
                }
            }
            return false;
        }
    }
    registered_hotkeys_ = bindings;
    hotkeys_registered_ = true;
    return true;
}

void PaletteWindow::remove_hotkeys() {
    if (!hotkeys_registered_) return;
    for (int id = 1; id <= static_cast<int>(kGlobalHotkeyActionCount); ++id)
        UnregisterHotKey(window_, id);
    hotkeys_registered_ = false;
}

void PaletteWindow::add_tray_icon() {
    tray_.cbSize = sizeof(tray_);
    tray_.hWnd = window_;
    tray_.uID = static_cast<UINT>(kTrayId);
    tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    tray_.uCallbackMessage = kTrayMessage;
    tray_.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101));
    if (!tray_.hIcon) tray_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    lstrcpynW(tray_.szTip, L"Elite Pen", ARRAYSIZE(tray_.szTip));
    Shell_NotifyIconW(NIM_ADD, &tray_);
    tray_.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &tray_);
}

void PaletteWindow::remove_tray_icon() {
    if (tray_.hWnd) Shell_NotifyIconW(NIM_DELETE, &tray_);
    tray_ = {};
}

void PaletteWindow::show_notification(const wchar_t* title, const std::wstring& message) {
    if (!tray_.hWnd) return;
    tray_.uFlags = NIF_INFO;
    lstrcpynW(tray_.szInfoTitle, title, ARRAYSIZE(tray_.szInfoTitle));
    lstrcpynW(tray_.szInfo, message.c_str(), ARRAYSIZE(tray_.szInfo));
    tray_.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &tray_);
    tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
}

void PaletteWindow::show_tool_menu() {
    controller_.toggle_tool_panel();
}

void PaletteWindow::choose_custom_color() {
    controller_.toggle_color_panel();
}

void PaletteWindow::show_tray_menu() {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 2001, L"Mostrar paleta");
    AppendMenuW(menu, MF_STRING, 2002, L"Ocultar / mostrar trazos");
    AppendMenuW(menu, MF_STRING, 2003, L"Pizarra blanca");
    AppendMenuW(menu, MF_STRING, 2006, L"Pizarra negra");
    AppendMenuW(menu, MF_STRING, 2004, L"Deshacer");
    AppendMenuW(menu, MF_STRING, 2005, L"Rehacer");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 2099, L"Salir de Elite Pen");
    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(window_);
    const int selected = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                        cursor.x, cursor.y, 0, window_, nullptr);
    DestroyMenu(menu);
    switch (selected) {
        case 2001: ShowWindow(window_, SW_SHOWNOACTIVATE); break;
        case 2002: controller_.toggle_visibility(); break;
        case 2003: controller_.toggle_whiteboard(); break;
        case 2006: controller_.toggle_blackboard(); break;
        case 2004: controller_.undo(); break;
        case 2005: controller_.redo(); break;
        case 2099: controller_.request_exit(); break;
        default: break;
    }
}

bool PaletteWindow::command_at(POINT point) const {
    if (collapsed_) return collapsed_expand_at(point);
    point = palette_logical_point(point, scale_);
    if (mode_ == ControlMode::Linear) {
        if (point_in_rect(point, kLinearCollapseBounds)) return true;
        for (const auto& item : kLinearItems) {
            if (point_in_rect(point, item.bounds)) return true;
        }
        if (point_in_rect(point, kLinearThicknessBounds)) return true;
        for (const auto& color_point : kLinearColorPoints) {
            if (point_in_circle(point, static_cast<float>(color_point.x),
                                static_cast<float>(color_point.y), 9.0F)) return true;
        }
        return point_in_circle(point, static_cast<float>(kLinearMoreColorsPoint.x),
                               static_cast<float>(kLinearMoreColorsPoint.y), 11.0F);
    }
    constexpr std::array<float, 5> thickness_y{32.0F, 49.0F, 69.0F, 93.0F, 122.0F};
    for (const float y : thickness_y) {
        if (point_in_circle(point, 23.0F, y, 11.0F)) return true;
    }
    constexpr std::array<POINT, 7> color_points{{
        {67, 107}, {69, 60}, {119, 29}, {169, 53}, {184, 96}, {160, 130}, {120, 147}
    }};
    for (const auto& color_point : color_points) {
        if (point_in_circle(point, static_cast<float>(color_point.x),
                            static_cast<float>(color_point.y), 17.0F)) return true;
    }
    return point_in_circle(point, 126, 83, 23) ||
           point_in_circle(point, 126, 113, 11) ||
           (point.x >= 205 && point_in_circle(point, 221, 233, 22)) ||
           (point.x >= 42 && point.x <= 86 && point.y >= 120 && point.y <= 174) ||
           (point.x >= 88 && point.x <= 116 && point.y >= 151 && point.y <= 184) ||
           (point.x < 210 && point_near_segment(point, 106, 173, 205, 220, 14));
}

bool PaletteWindow::collapsed_expand_at(POINT point) const {
    const int width = pixel_width();
    const int height = pixel_height();
    const float center_x = static_cast<float>(width) *
        (mode_ == ControlMode::Linear ? 0.50F : 0.52F);
    const float center_y = static_cast<float>(height) *
        (mode_ == ControlMode::Linear ? 0.50F : 0.46F);
    const float radius = std::max(7.0F,
        static_cast<float>(std::min(width, height)) *
            (mode_ == ControlMode::Linear ? 0.28F : 0.17F));
    return point_in_circle(point, center_x, center_y, radius);
}

void PaletteWindow::begin_drag(POINT point) {
    dragging_ = true;
    drag_origin_ = point;
    ClientToScreen(window_, &drag_origin_);
    RECT bounds{};
    GetWindowRect(window_, &bounds);
    window_origin_ = {bounds.left, bounds.top};
    SetCapture(window_);
}

bool PaletteWindow::contains_screen_point(POINT point) const {
    if (!IsWindowVisible(window_)) return false;
    RECT bounds{};
    return GetWindowRect(window_, &bounds) && PtInRect(&bounds, point);
}

bool PaletteWindow::activate_command_at(POINT point) {
    if (!command_at(point)) return false;
    activate_at(point);
    return true;
}

void PaletteWindow::activate_at(POINT point) {
    if (collapsed_) {
        if (collapsed_expand_at(point)) set_collapsed(false);
        else begin_drag(point);
        return;
    }
    const POINT physical_point = point;
    point = palette_logical_point(point, scale_);
    if (mode_ == ControlMode::Linear) {
        if (point_in_rect(point, kLinearCollapseBounds)) {
            set_collapsed(true);
            return;
        }
        for (const auto& item : kLinearItems) {
            if (!point_in_rect(point, item.bounds)) continue;
            switch (item.action) {
                case LinearAction::Visibility: controller_.toggle_visibility(); break;
                case LinearAction::Interact:
                    controller_.set_tool(controller_.state().tool == Tool::Interact
                        ? Tool::Pen : Tool::Interact);
                    break;
                case LinearAction::ToolPanel: show_tool_menu(); break;
                case LinearAction::Highlighter: controller_.set_tool(Tool::Highlighter); break;
                case LinearAction::Eraser: controller_.set_tool(Tool::Eraser); break;
                case LinearAction::Geometry: controller_.toggle_geometry_panel(); break;
                case LinearAction::Text: controller_.set_tool(Tool::Text); break;
                case LinearAction::Board: controller_.toggle_whiteboard(); break;
                case LinearAction::Zoom: controller_.toggle_zoom(); break;
                case LinearAction::Undo: controller_.undo(); break;
                case LinearAction::Redo: controller_.redo(); break;
                case LinearAction::Clear: controller_.clear_document(); break;
                case LinearAction::Settings: controller_.show_settings_window(); break;
            }
            return;
        }
        if (point_in_rect(point, kLinearThicknessBounds)) {
            std::size_t nearest = 0;
            LONG distance = std::numeric_limits<LONG>::max();
            for (std::size_t index = 0; index < kLinearThicknessPoints.size(); ++index) {
                const LONG current = std::abs(point.x - kLinearThicknessPoints[index].x);
                if (current < distance) { distance = current; nearest = index; }
            }
            controller_.set_thickness(kThicknessSteps[nearest]);
            return;
        }
        constexpr std::array<Color, 6> colors{
            kBlack, kYellow, kBlue, kRed, kGreen, kPurple};
        for (std::size_t index = 0; index < kLinearColorPoints.size(); ++index) {
            const POINT center = kLinearColorPoints[index];
            if (point_in_circle(point, static_cast<float>(center.x),
                                static_cast<float>(center.y), 9.0F)) {
                controller_.set_color(colors[index]);
                return;
            }
        }
        if (point_in_circle(point, static_cast<float>(kLinearMoreColorsPoint.x),
                            static_cast<float>(kLinearMoreColorsPoint.y), 11.0F)) {
            choose_custom_color();
            return;
        }
        begin_drag(physical_point);
        return;
    }
    constexpr std::array<float, 5> thickness_y{32.0F, 49.0F, 69.0F, 93.0F, 122.0F};
    for (std::size_t index = 0; index < thickness_y.size(); ++index) {
        if (point_in_circle(point, 23.0F, thickness_y[index], 10.0F)) {
            controller_.set_thickness(kThicknessSteps[index]);
            return;
        }
    }
    struct Swatch { float x; float y; Color color; };
    constexpr std::array swatches{
        Swatch{67, 107, kBlack}, Swatch{69, 60, kYellow},
        Swatch{119, 29, kBlue}, Swatch{169, 53, kRed},
        Swatch{184, 96, kGreen}, Swatch{160, 130, kPurple}};
    for (const auto& swatch : swatches) {
        if (point_in_circle(point, swatch.x, swatch.y, 15.0F)) {
            controller_.set_color(swatch.color);
            return;
        }
    }
    if (point_in_circle(point, 120, 147, 14)) { choose_custom_color(); return; }
    if (point_in_circle(point, 126, 83, 22)) { controller_.toggle_visibility(); return; }
    if (point_in_circle(point, 126, 113, 10)) { set_collapsed(true); return; }
    if (point.x >= 205 && point_in_circle(point, 221, 233, 21)) {
        controller_.clear_document();
        return;
    }
    if (point.x >= 42 && point.x <= 86 && point.y >= 120 && point.y <= 174) {
        controller_.set_tool(controller_.state().tool == Tool::Interact
            ? Tool::Pen : Tool::Interact);
        return;
    }
    if (point.x >= 88 && point.x <= 116 && point.y >= 155 && point.y <= 184) {
        controller_.toggle_whiteboard(); return;
    }
    if (point.x < 210 && point_near_segment(point, 106, 173, 205, 220, 14)) {
        show_tool_menu(); return;
    }

    begin_drag(physical_point);
}

LRESULT PaletteWindow::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case kQaQueryToolMessage:
            return static_cast<LRESULT>(controller_.state().tool);
        case kQaQueryColorMessage:
            return static_cast<LRESULT>(controller_.state().color.argb());
        case kQaQueryThicknessMessage:
            return static_cast<LRESULT>(std::lround(controller_.state().thickness * 10.0F));
        case kQaQueryThicknessWheelRouteMessage:
            return 1;
        case kQaQueryGlobalHotkeysMessage:
            return hotkeys_registered_ ? 1 : 0;
        case kQaQueryDocumentCountMessage:
            return static_cast<LRESULT>(controller_.state().document.items().size());
        case kQaQueryPaletteCollapsedMessage:
            return collapsed_ ? 1 : 0;
        case kQaQueryThemeMessage:
            return static_cast<LRESULT>(controller_.preferences().theme);
        case kQaQueryControlModeMessage:
            return static_cast<LRESULT>(mode_);
        case kQaSetControlModeMessage:
            controller_.set_control_mode(wparam == 1 ? ControlMode::Linear
                                                     : ControlMode::Palette);
            return static_cast<LRESULT>(mode_);
        case kQaPopulateStressDocumentMessage:
            controller_.populate_stress_document(
                std::clamp<std::size_t>(static_cast<std::size_t>(wparam), 1, 20000));
            return static_cast<LRESULT>(controller_.state().document.items().size());
        case kQaQueryBoardModeMessage:
            return controller_.state().whiteboard ? 1 :
                   (controller_.state().blackboard ? 2 : 0);
        case WM_SETCURSOR:
            if (LOWORD(lparam) == HTCLIENT) {
                POINT point{};
                GetCursorPos(&point);
                ScreenToClient(window_, &point);
                SetCursor(LoadCursorW(nullptr, command_at(point) ? IDC_HAND : IDC_SIZEALL));
                return TRUE;
            }
            break;
        case WM_LBUTTONDOWN:
            activate_at({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
            return 0;
        case WM_MOUSEMOVE:
            if (dragging_ && (wparam & MK_LBUTTON)) {
                POINT current{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                ClientToScreen(window_, &current);
                SetWindowPos(window_, nullptr,
                             window_origin_.x + current.x - drag_origin_.x,
                             window_origin_.y + current.y - drag_origin_.y,
                             0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER |
                                   SWP_NOSENDCHANGING);
            } else if (mode_ == ControlMode::Linear && !collapsed_) {
                const POINT logical = palette_logical_point(
                    {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)}, scale_);
                int hovered = point_in_rect(logical, kLinearCollapseBounds) ? -2 : -1;
                for (std::size_t index = 0; hovered == -1 && index < kLinearItems.size(); ++index) {
                    if (point_in_rect(logical, kLinearItems[index].bounds))
                        hovered = static_cast<int>(index);
                }
                if (hovered == -1 && point_in_rect(logical, kLinearThicknessBounds)) hovered = -3;
                if (hovered_linear_item_ != hovered) {
                    hovered_linear_item_ = hovered;
                    invalidate();
                }
                if (!tracking_mouse_) {
                    TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window_, 0};
                    TrackMouseEvent(&tracking);
                    tracking_mouse_ = true;
                }
            }
            return 0;
        case WM_MOUSELEAVE:
            tracking_mouse_ = false;
            if (hovered_linear_item_ != -1) {
                hovered_linear_item_ = -1;
                invalidate();
            }
            return 0;
        case WM_LBUTTONUP:
            dragging_ = false;
            if (GetCapture() == window_) ReleaseCapture();
            controller_.save_palette_position();
            return 0;
        case WM_CANCELMODE:
        case WM_CAPTURECHANGED:
            dragging_ = false;
            return 0;
        case WM_RBUTTONUP:
            if (const POINT point = palette_logical_point(
                    {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)}, scale_);
                (mode_ == ControlMode::Linear &&
                 point_in_rect(point, kLinearItems[7].bounds)) ||
                (mode_ == ControlMode::Palette && point.x >= 88 && point.x <= 116 &&
                 point.y >= 155 && point.y <= 184)) {
                controller_.toggle_blackboard();
            } else {
                show_tray_menu();
            }
            return 0;
        case WM_HOTKEY:
            if (wparam >= 1 &&
                wparam <= static_cast<WPARAM>(kGlobalHotkeyActionCount)) {
                controller_.execute_hotkey(static_cast<HotkeyAction>(
                    static_cast<std::size_t>(wparam) - 1));
            }
            return 0;
        case kThicknessWheelMessage:
            controller_.adjust_thickness_step(wparam != 0 ? 1 : -1);
            return 0;
        case WM_TIMER:
            if (wparam == 20) {
                const bool down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
                if (down && !escape_down_) controller_.stop_transient_mode();
                escape_down_ = down;
                if (controller_.state().cursor_highlight) {
                    POINT cursor{};
                    if (GetCursorPos(&cursor) &&
                        (!highlight_cursor_initialized_ ||
                         cursor.x != last_highlight_cursor_.x ||
                         cursor.y != last_highlight_cursor_.y)) {
                        last_highlight_cursor_ = cursor;
                        highlight_cursor_initialized_ = true;
                        controller_.invalidate_document();
                    }
                } else {
                    highlight_cursor_initialized_ = false;
                }
                controller_.update_transient_ink();
                return 0;
            }
            break;
        case kTrayMessage:
            if (LOWORD(lparam) == WM_CONTEXTMENU || LOWORD(lparam) == WM_RBUTTONUP) {
                show_tray_menu();
            } else if (LOWORD(lparam) == WM_LBUTTONUP) {
                ShowWindow(window_, IsWindowVisible(window_) ? SW_HIDE : SW_SHOWNOACTIVATE);
            }
            return 0;
        case WM_DISPLAYCHANGE:
            controller_.rebuild_overlays();
            return 0;
        case WM_SETTINGCHANGE:
            invalidate();
            return 0;
        case kExitMessage:
            DestroyWindow(window_);
            return 0;
        case WM_DESTROY:
            KillTimer(window_, 20);
            remove_hotkeys();
            remove_tray_icon();
            PostQuitMessage(0);
            return 0;
        default:
            return WindowBase::handle_message(message, wparam, lparam);
    }
    return WindowBase::handle_message(message, wparam, lparam);
}

void PaletteWindow::render() {
    auto* context = surface_.begin_draw(D2D1::ColorF(0, 0.0F));
    if (!context) return;
    const auto& theme = current_ui_theme();
    if (collapsed_) {
        const float width = static_cast<float>(surface_.width());
        const float height = static_cast<float>(surface_.height());
        if (mode_ == ControlMode::Linear) {
            ComPtr<ID2D1SolidColorBrush> shadow;
            ComPtr<ID2D1SolidColorBrush> panel;
            ComPtr<ID2D1SolidColorBrush> border;
            ComPtr<ID2D1SolidColorBrush> accent;
            context->CreateSolidColorBrush(
                theme_color(theme.shadow, theme.light ? 0.18F : 0.44F), shadow.GetAddressOf());
            context->CreateSolidColorBrush(theme_color(theme.surface_1, 0.985F), panel.GetAddressOf());
            context->CreateSolidColorBrush(theme_color(theme.line), border.GetAddressOf());
            context->CreateSolidColorBrush(theme_color(theme.violet), accent.GetAddressOf());
            const auto shadow_rect = D2D1::RoundedRect(
                D2D1::RectF(2.5F, 3.5F, width - 0.5F, height - 0.5F),
                std::min(width, height) * 0.34F, std::min(width, height) * 0.34F);
            const auto panel_rect = D2D1::RoundedRect(
                D2D1::RectF(1, 1, width - 2, height - 2),
                std::min(width, height) * 0.32F, std::min(width, height) * 0.32F);
            context->FillRoundedRectangle(shadow_rect, shadow.Get());
            context->FillRoundedRectangle(panel_rect, panel.Get());
            context->DrawRoundedRectangle(panel_rect, border.Get(), 1.0F);
            const float cx = width * 0.50F;
            const float cy = height * 0.50F;
            const float span = std::clamp(std::min(width, height) * 0.18F, 4.0F, 9.0F);
            context->DrawLine(D2D1::Point2F(cx - 2, cy - 2),
                              D2D1::Point2F(cx - span, cy - span), accent.Get(), 1.7F);
            context->DrawLine(D2D1::Point2F(cx - span, cy - span),
                              D2D1::Point2F(cx - span + 4, cy - span), accent.Get(), 1.7F);
            context->DrawLine(D2D1::Point2F(cx - span, cy - span),
                              D2D1::Point2F(cx - span, cy - span + 4), accent.Get(), 1.7F);
            context->DrawLine(D2D1::Point2F(cx + 2, cy + 2),
                              D2D1::Point2F(cx + span, cy + span), accent.Get(), 1.7F);
            context->DrawLine(D2D1::Point2F(cx + span, cy + span),
                              D2D1::Point2F(cx + span - 4, cy + span), accent.Get(), 1.7F);
            context->DrawLine(D2D1::Point2F(cx + span, cy + span),
                              D2D1::Point2F(cx + span, cy + span - 4), accent.Get(), 1.7F);
            std::wstring error;
            if (!surface_.end_draw(error)) controller_.report_runtime_error(error);
            return;
        }
        ComPtr<ID2D1SolidColorBrush> shadow;
        ComPtr<ID2D1SolidColorBrush> border;
        ComPtr<ID2D1SolidColorBrush> icon;
        context->CreateSolidColorBrush(theme_color(theme.shadow, theme.light ? 0.18F : 0.42F),
                                       shadow.GetAddressOf());
        context->CreateSolidColorBrush(theme_color(theme.line, 0.96F), border.GetAddressOf());
        context->CreateSolidColorBrush(theme_color(theme.violet_strong), icon.GetAddressOf());
        ComPtr<ID2D1PathGeometry> shape;
        controller_.graphics().d2d_factory()->CreatePathGeometry(shape.GetAddressOf());
        ComPtr<ID2D1GeometrySink> mini;
        shape->Open(mini.GetAddressOf());
        mini->BeginFigure(D2D1::Point2F(width * 0.22F, height * 0.18F),
                          D2D1_FIGURE_BEGIN_FILLED);
        mini->AddBezier(D2D1::BezierSegment(
            D2D1::Point2F(width * 0.42F, height * 0.01F),
            D2D1::Point2F(width * 0.82F, height * 0.08F),
            D2D1::Point2F(width * 0.91F, height * 0.39F)));
        mini->AddBezier(D2D1::BezierSegment(
            D2D1::Point2F(width * 0.99F, height * 0.70F),
            D2D1::Point2F(width * 0.65F, height * 0.94F),
            D2D1::Point2F(width * 0.49F, height * 0.79F)));
        mini->AddBezier(D2D1::BezierSegment(
            D2D1::Point2F(width * 0.35F, height * 0.67F),
            D2D1::Point2F(width * 0.35F, height * 0.99F),
            D2D1::Point2F(width * 0.12F, height * 0.79F)));
        mini->AddBezier(D2D1::BezierSegment(
            D2D1::Point2F(width * -0.02F, height * 0.65F),
            D2D1::Point2F(width * 0.04F, height * 0.34F),
            D2D1::Point2F(width * 0.22F, height * 0.18F)));
        mini->EndFigure(D2D1_FIGURE_END_CLOSED);
        mini->Close();
        const D2D1_GRADIENT_STOP stops[]{{0.0F, theme_color(theme.surface_2)},
                                         {1.0F, theme_color(theme.surface_1)}};
        const auto fill = linear_gradient(context, D2D1::Point2F(0, 0),
                                           D2D1::Point2F(width, height), stops, 2);
        context->SetTransform(D2D1::Matrix3x2F::Translation(0, 1.5F));
        context->FillGeometry(shape.Get(), shadow.Get());
        context->SetTransform(D2D1::Matrix3x2F::Identity());
        context->FillGeometry(shape.Get(), fill ? static_cast<ID2D1Brush*>(fill.Get())
                                                : static_cast<ID2D1Brush*>(shadow.Get()));
        context->DrawGeometry(shape.Get(), border.Get(), 1.1F);
        const float cx = width * 0.52F;
        const float cy = height * 0.46F;
        const float span = std::clamp(std::min(width, height) * 0.11F, 4.0F, 7.0F);
        context->DrawLine(D2D1::Point2F(cx - 2, cy - 2),
                          D2D1::Point2F(cx - span, cy - span), icon.Get(), 1.7F);
        context->DrawLine(D2D1::Point2F(cx - span, cy - span),
                          D2D1::Point2F(cx - span + 4, cy - span), icon.Get(), 1.7F);
        context->DrawLine(D2D1::Point2F(cx - span, cy - span),
                          D2D1::Point2F(cx - span, cy - span + 4), icon.Get(), 1.7F);
        context->DrawLine(D2D1::Point2F(cx + 2, cy + 2),
                          D2D1::Point2F(cx + span, cy + span), icon.Get(), 1.7F);
        context->DrawLine(D2D1::Point2F(cx + span, cy + span),
                          D2D1::Point2F(cx + span - 4, cy + span), icon.Get(), 1.7F);
        context->DrawLine(D2D1::Point2F(cx + span, cy + span),
                          D2D1::Point2F(cx + span, cy + span - 4), icon.Get(), 1.7F);
        std::wstring error;
        if (!surface_.end_draw(error)) controller_.report_runtime_error(error);
        return;
    }
    if (mode_ == ControlMode::Linear) {
        render_linear(context, theme);
        std::wstring error;
        if (!surface_.end_draw(error)) controller_.report_runtime_error(error);
        return;
    }
    context->SetTransform(D2D1::Matrix3x2F::Scale(scale_, scale_));

    ComPtr<ID2D1SolidColorBrush> cream;
    ComPtr<ID2D1SolidColorBrush> shadow;
    ComPtr<ID2D1SolidColorBrush> ink;
    ComPtr<ID2D1SolidColorBrush> gold;
    ComPtr<ID2D1SolidColorBrush> gold_bright;
    ComPtr<ID2D1SolidColorBrush> panel_border;
    ComPtr<ID2D1SolidColorBrush> rail;
    ComPtr<ID2D1SolidColorBrush> muted;
    ComPtr<ID2D1SolidColorBrush> glass;
    ComPtr<ID2D1SolidColorBrush> swatch_shell;
    context->CreateSolidColorBrush(theme_color(theme.text), cream.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.shadow, theme.light ? 0.18F : 0.38F),
                                   shadow.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.text), ink.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.violet), gold.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.violet_strong), gold_bright.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.line, 0.94F), panel_border.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.surface_1, 0.98F), rail.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.text_soft), muted.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.light ? 0xFFFFFF : 0xFFFFFF,
                                               theme.light ? 0.48F : 0.18F),
                                   glass.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.surface_2, 0.98F),
                                   swatch_shell.GetAddressOf());

    context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(129, 87), 88, 78), shadow.Get());
    ComPtr<ID2D1PathGeometry> palette_geometry;
    controller_.graphics().d2d_factory()->CreatePathGeometry(palette_geometry.GetAddressOf());
    ComPtr<ID2D1GeometrySink> sink;
    palette_geometry->Open(sink.GetAddressOf());
    sink->BeginFigure(D2D1::Point2F(59, 28), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(91, 2), D2D1::Point2F(166, 3),
                                        D2D1::Point2F(199, 42)));
    sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(228, 78), D2D1::Point2F(205, 135),
                                        D2D1::Point2F(168, 142)));
    sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(136, 149), D2D1::Point2F(139, 188),
                                        D2D1::Point2F(99, 183)));
    sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(53, 179), D2D1::Point2F(34, 145),
                                        D2D1::Point2F(50, 116)));
    sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(62, 94), D2D1::Point2F(23, 91),
                                        D2D1::Point2F(36, 55)));
    sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(41, 42), D2D1::Point2F(48, 35),
                                        D2D1::Point2F(59, 28)));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    const D2D1_GRADIENT_STOP palette_stops[]{
        {0.0F, theme_color(theme.surface_2)},
        {0.58F, theme_color(theme.surface_1)},
        {1.0F, theme_color(theme.surface_1)}
    };
    const auto palette_fill = linear_gradient(
        context, D2D1::Point2F(48, 15), D2D1::Point2F(191, 171),
        palette_stops, static_cast<UINT>(std::size(palette_stops)));
    if (palette_fill) context->FillGeometry(palette_geometry.Get(), palette_fill.Get());
    else context->FillGeometry(palette_geometry.Get(), rail.Get());
    context->DrawGeometry(palette_geometry.Get(), panel_border.Get(), 1.45F);
    context->DrawGeometry(palette_geometry.Get(), glass.Get(), 0.55F);

    struct Swatch { float x; float y; Color color; };
    constexpr std::array swatches{
        Swatch{67, 107, kBlack}, Swatch{69, 60, kYellow},
        Swatch{119, 29, kBlue}, Swatch{169, 53, kRed},
        Swatch{184, 96, kGreen}, Swatch{160, 130, kPurple}};
    for (const auto& swatch : swatches) {
        ComPtr<ID2D1SolidColorBrush> color_brush;
        context->CreateSolidColorBrush(d2d_color(swatch.color), color_brush.GetAddressOf());
        context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(swatch.x, swatch.y), 14.5F, 14.5F),
                             swatch_shell.Get());
        if (controller_.state().color == swatch.color) {
            context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(swatch.x, swatch.y), 17, 17),
                                 gold_bright.Get(), 2.4F);
            context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(swatch.x, swatch.y), 14.5F, 14.5F),
                                 gold.Get(), 1.0F);
        }
        context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(swatch.x, swatch.y), 11.5F, 11.5F),
                             color_brush.Get());
        context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(swatch.x, swatch.y), 11.5F, 11.5F),
                             glass.Get(), 1.0F);
        context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(swatch.x - 3.5F, swatch.y - 4.0F),
                                           3.2F, 2.1F), glass.Get());
    }
    // The advanced selector is intentionally just a clean plus sign after purple.
    context->DrawLine(D2D1::Point2F(114.5F, 147), D2D1::Point2F(125.5F, 147),
                      gold_bright.Get(), 1.8F);
    context->DrawLine(D2D1::Point2F(120, 141.5F), D2D1::Point2F(120, 152.5F),
                      gold_bright.Get(), 1.8F);

    const auto thickness_shadow = D2D1::RoundedRect(D2D1::RectF(5, 13, 41, 144), 17, 17);
    context->FillRoundedRectangle(thickness_shadow, shadow.Get());
    const auto thickness_panel = D2D1::RoundedRect(D2D1::RectF(7, 11, 39, 142), 16, 16);
    context->FillRoundedRectangle(thickness_panel, rail.Get());
    context->DrawRoundedRectangle(thickness_panel, panel_border.Get(), 1.15F);
    context->DrawLine(D2D1::Point2F(11, 27), D2D1::Point2F(11, 126), glass.Get(), 0.8F);

    constexpr std::array<float, 5> thicknesses{2, 4, 7, 12, 20};
    constexpr std::array<float, 5> ys{32, 49, 69, 93, 122};
    for (std::size_t index = 0; index < ys.size(); ++index) {
        const float radius = 1.7F + static_cast<float>(index) * 1.45F;
        if (std::abs(controller_.state().thickness - thicknesses[index]) < 0.1F) {
            context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(23, ys[index]),
                                                radius + 4, radius + 4), gold_bright.Get(), 1.8F);
            context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(23, ys[index]), radius, radius),
                                 cream.Get());
        } else {
            context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(23, ys[index]), radius, radius),
                                 muted.Get());
        }
    }

    // Eye: open means visible; closed means annotations are preserved but hidden.
    if (controller_.state().annotations_visible) {
        ComPtr<ID2D1PathGeometry> eye;
        controller_.graphics().d2d_factory()->CreatePathGeometry(eye.GetAddressOf());
        ComPtr<ID2D1GeometrySink> eye_sink;
        eye->Open(eye_sink.GetAddressOf());
        eye_sink->BeginFigure(D2D1::Point2F(108, 83), D2D1_FIGURE_BEGIN_HOLLOW);
        eye_sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(118, 72), D2D1::Point2F(135, 72),
                                                D2D1::Point2F(145, 83)));
        eye_sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(135, 94), D2D1::Point2F(118, 94),
                                                D2D1::Point2F(108, 83)));
        eye_sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        eye_sink->Close();
        context->DrawGeometry(eye.Get(), cream.Get(), 1.65F);
        context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(126.5F, 83), 5.0F, 6.5F),
                             gold.Get());
        context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(126.5F, 83), 2.1F, 3.1F),
                             rail.Get());
        context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(125.2F, 80.8F), 1.0F, 1.2F),
                             glass.Get());
    } else {
        ComPtr<ID2D1PathGeometry> closed_eye;
        controller_.graphics().d2d_factory()->CreatePathGeometry(closed_eye.GetAddressOf());
        ComPtr<ID2D1GeometrySink> closed_eye_sink;
        closed_eye->Open(closed_eye_sink.GetAddressOf());
        closed_eye_sink->BeginFigure(D2D1::Point2F(108, 81), D2D1_FIGURE_BEGIN_HOLLOW);
        closed_eye_sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(118, 92),
                                                       D2D1::Point2F(136, 92),
                                                       D2D1::Point2F(145, 81)));
        closed_eye_sink->EndFigure(D2D1_FIGURE_END_OPEN);
        closed_eye_sink->Close();
        context->DrawGeometry(closed_eye.Get(), cream.Get(), 2.0F);
        context->DrawLine(D2D1::Point2F(115, 87), D2D1::Point2F(111, 92), gold.Get(), 1.5F);
        context->DrawLine(D2D1::Point2F(126.5F, 90), D2D1::Point2F(126.5F, 96), gold.Get(), 1.5F);
        context->DrawLine(D2D1::Point2F(138, 87), D2D1::Point2F(142, 92), gold.Get(), 1.5F);
    }

    // Compact hibernation control beneath the eye. Its hit zone stays generous,
    // but the visual is intentionally only the inward-chevron glyph.
    context->DrawLine(D2D1::Point2F(116.5F, 108.0F), D2D1::Point2F(123.5F, 113.0F),
                      gold_bright.Get(), 1.65F);
    context->DrawLine(D2D1::Point2F(116.5F, 118.0F), D2D1::Point2F(123.5F, 113.0F),
                      gold_bright.Get(), 1.65F);
    context->DrawLine(D2D1::Point2F(135.5F, 108.0F), D2D1::Point2F(128.5F, 113.0F),
                      gold_bright.Get(), 1.65F);
    context->DrawLine(D2D1::Point2F(135.5F, 118.0F), D2D1::Point2F(128.5F, 113.0F),
                      gold_bright.Get(), 1.65F);

    // Functional brush: tip mode, white ferrule/board and a clean tool-menu handle.
    ComPtr<ID2D1SolidColorBrush> bristle;
    ComPtr<ID2D1SolidColorBrush> ferrule;
    ComPtr<ID2D1SolidColorBrush> handle;
    ComPtr<ID2D1SolidColorBrush> active_paint;
    ComPtr<ID2D1SolidColorBrush> danger;
    context->CreateSolidColorBrush(theme_color(theme.light ? 0x5A4674 : 0x28223E),
                                   bristle.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.light ? 0xF4F5F8 : 0xDDE1EA),
                                   ferrule.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.violet), handle.GetAddressOf());
    context->CreateSolidColorBrush(d2d_color(controller_.state().color),
                                   active_paint.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.danger), danger.GetAddressOf());
    const D2D1_GRADIENT_STOP handle_stops[]{
        {0.0F, theme_color(theme.violet_strong)},
        {1.0F, theme_color(theme.violet)}
    };
    const auto handle_gradient = linear_gradient(
        context, D2D1::Point2F(106, 173), D2D1::Point2F(205, 220),
        handle_stops, static_cast<UINT>(std::size(handle_stops)));
    const D2D1_GRADIENT_STOP ferrule_stops[]{
        {0.0F, theme_color(0xFFFFFF)},
        {0.44F, theme_color(theme.light ? 0xE7EAF1 : 0xD8DCE6)},
        {1.0F, theme_color(theme.light ? 0xAEB6C6 : 0x737C8E)}
    };
    const auto ferrule_gradient = linear_gradient(
        context, D2D1::Point2F(78, 151), D2D1::Point2F(108, 182),
        ferrule_stops, static_cast<UINT>(std::size(ferrule_stops)));
    context->DrawLine(D2D1::Point2F(106, 177), D2D1::Point2F(205, 224),
                      shadow.Get(), 20.0F);
    context->DrawLine(D2D1::Point2F(106, 173), D2D1::Point2F(205, 220),
                      handle_gradient ? static_cast<ID2D1Brush*>(handle_gradient.Get())
                                      : static_cast<ID2D1Brush*>(handle.Get()), 17.0F);
    context->DrawLine(D2D1::Point2F(110, 169), D2D1::Point2F(203, 214),
                      glass.Get(), 1.0F);
    D2D1_POINT_2F ferrule_points[]{{82, 151}, {110, 164}, {102, 182}, {75, 169}};
    ComPtr<ID2D1PathGeometry> ferrule_geometry;
    controller_.graphics().d2d_factory()->CreatePathGeometry(ferrule_geometry.GetAddressOf());
    ComPtr<ID2D1GeometrySink> ferrule_sink;
    ferrule_geometry->Open(ferrule_sink.GetAddressOf());
    ferrule_sink->BeginFigure(ferrule_points[0], D2D1_FIGURE_BEGIN_FILLED);
    ferrule_sink->AddLines(ferrule_points + 1, 3);
    ferrule_sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    ferrule_sink->Close();
    context->FillGeometry(ferrule_geometry.Get(), ferrule_gradient
        ? static_cast<ID2D1Brush*>(ferrule_gradient.Get())
        : static_cast<ID2D1Brush*>(ferrule.Get()));
    context->DrawGeometry(ferrule_geometry.Get(), panel_border.Get(), 1.15F);

    ComPtr<ID2D1PathGeometry> bristle_geometry;
    controller_.graphics().d2d_factory()->CreatePathGeometry(bristle_geometry.GetAddressOf());
    ComPtr<ID2D1GeometrySink> bristle_sink;
    bristle_geometry->Open(bristle_sink.GetAddressOf());
    bristle_sink->BeginFigure(D2D1::Point2F(76, 169), D2D1_FIGURE_BEGIN_FILLED);
    bristle_sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(61, 169), D2D1::Point2F(49, 158),
                                                D2D1::Point2F(48, 137)));
    bristle_sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(57, 149), D2D1::Point2F(80, 141),
                                                D2D1::Point2F(84, 153)));
    bristle_sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(87, 160), D2D1::Point2F(82, 166),
                                                D2D1::Point2F(76, 169)));
    bristle_sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    bristle_sink->Close();
    context->FillGeometry(bristle_geometry.Get(), bristle.Get());
    context->DrawGeometry(bristle_geometry.Get(), glass.Get(), 0.8F);
    context->DrawLine(D2D1::Point2F(49, 137), D2D1::Point2F(50, 123), shadow.Get(), 9.0F);
    context->DrawLine(D2D1::Point2F(49, 137), D2D1::Point2F(50, 123),
                      active_paint.Get(), 6.5F);

    // Pointer/pen toggle lives in the brush tip.
    if (controller_.state().tool == Tool::Interact) {
        context->DrawLine(D2D1::Point2F(63, 149), D2D1::Point2F(74, 162),
                          ferrule.Get(), 2.0F);
        context->DrawLine(D2D1::Point2F(63, 149), D2D1::Point2F(65, 165),
                          ferrule.Get(), 2.0F);
    } else {
        context->DrawLine(D2D1::Point2F(61, 162), D2D1::Point2F(75, 149),
                          ferrule.Get(), 2.3F);
        context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(61, 162), 2.3F, 2.3F),
                             ferrule.Get());
    }

    // A familiar trash can makes the destructive action immediately recognizable.
    context->DrawRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(213, 224, 229, 247), 3, 3), danger.Get(), 2.0F);
    context->DrawLine(D2D1::Point2F(210, 220), D2D1::Point2F(232, 220),
                      danger.Get(), 2.1F);
    context->DrawLine(D2D1::Point2F(216, 216), D2D1::Point2F(226, 216),
                      danger.Get(), 2.1F);
    context->DrawLine(D2D1::Point2F(217, 229), D2D1::Point2F(217, 243),
                      danger.Get(), 1.45F);
    context->DrawLine(D2D1::Point2F(221, 229), D2D1::Point2F(221, 243),
                      danger.Get(), 1.45F);
    context->DrawLine(D2D1::Point2F(225, 229), D2D1::Point2F(225, 243),
                      danger.Get(), 1.45F);

    std::wstring error;
    if (!surface_.end_draw(error)) controller_.report_runtime_error(error);
}

void PaletteWindow::render_linear(ID2D1DeviceContext* context, const UiTheme& theme) {
    context->SetTransform(D2D1::Matrix3x2F::Scale(scale_, scale_));
    ComPtr<ID2D1SolidColorBrush> panel;
    ComPtr<ID2D1SolidColorBrush> border;
    ComPtr<ID2D1SolidColorBrush> text;
    ComPtr<ID2D1SolidColorBrush> muted;
    ComPtr<ID2D1SolidColorBrush> accent;
    ComPtr<ID2D1SolidColorBrush> selected;
    ComPtr<ID2D1SolidColorBrush> hovered;
    ComPtr<ID2D1SolidColorBrush> danger;
    ComPtr<ID2D1SolidColorBrush> shadow;
    ComPtr<ID2D1SolidColorBrush> glass;
    context->CreateSolidColorBrush(theme_color(theme.surface_1, 0.985F), panel.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.line, 0.96F), border.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.text), text.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.text_muted), muted.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.violet), accent.GetAddressOf());
    context->CreateSolidColorBrush(
        theme_color(theme.light ? 0xE7E2FB : 0x28223E), selected.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.surface_4), hovered.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.danger), danger.GetAddressOf());
    context->CreateSolidColorBrush(
        theme_color(theme.shadow, theme.light ? 0.17F : 0.43F), shadow.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(0xFFFFFF, theme.light ? 0.48F : 0.16F),
                                   glass.GetAddressOf());

    const auto shadow_rect = D2D1::RoundedRect(D2D1::RectF(5, 5, 75, 676), 21, 21);
    const auto panel_rect = D2D1::RoundedRect(D2D1::RectF(2, 2, 73, 673), 19, 19);
    context->FillRoundedRectangle(shadow_rect, shadow.Get());
    context->FillRoundedRectangle(panel_rect, panel.Get());
    context->DrawRoundedRectangle(panel_rect, border.Get(), 1.1F);
    context->DrawLine(D2D1::Point2F(12, 51), D2D1::Point2F(64, 51), border.Get(), 0.8F);

    // Compact Elite nib and inward chevrons: recognizable brand plus hibernation.
    context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(38, 25), 11, 11), selected.Get());
    context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(38, 25), 11, 11), accent.Get(), 1.3F);
    context->DrawLine(D2D1::Point2F(32, 31), D2D1::Point2F(43, 20), accent.Get(), 2.2F);
    context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(31.5F, 31.5F), 2, 2), accent.Get());
    context->DrawLine(D2D1::Point2F(29, 40), D2D1::Point2F(35, 44), muted.Get(), 1.4F);
    context->DrawLine(D2D1::Point2F(47, 40), D2D1::Point2F(41, 44), muted.Get(), 1.4F);

    ComPtr<IDWriteTextFormat> glyph_format;
    controller_.graphics().dwrite()->CreateTextFormat(
        L"Segoe UI Variable Text", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 18.0F, L"es-CO",
        glyph_format.GetAddressOf());
    if (glyph_format) {
        glyph_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        glyph_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    const Tool current_tool = controller_.state().tool;
    for (std::size_t index = 0; index < kLinearItems.size(); ++index) {
        const auto& item = kLinearItems[index];
        const float cx = 38.0F;
        const float cy = (static_cast<float>(item.bounds.top) +
                          static_cast<float>(item.bounds.bottom)) * 0.5F;
        bool active = false;
        switch (item.action) {
            case LinearAction::Visibility: active = !controller_.state().annotations_visible; break;
            case LinearAction::Interact:
                active = current_tool == Tool::Interact || current_tool == Tool::Pen; break;
            case LinearAction::Highlighter: active = current_tool == Tool::Highlighter; break;
            case LinearAction::Eraser: active = current_tool == Tool::Eraser; break;
            case LinearAction::Geometry:
                active = current_tool == Tool::Line || current_tool == Tool::Rectangle ||
                         current_tool == Tool::Ellipse || current_tool == Tool::Arrow ||
                         current_tool == Tool::CurvedArrow;
                break;
            case LinearAction::Text: active = current_tool == Tool::Text; break;
            case LinearAction::Board:
                active = controller_.state().whiteboard || controller_.state().blackboard; break;
            case LinearAction::Zoom: active = controller_.zoom_active(); break;
            default: break;
        }
        const bool is_hovered = hovered_linear_item_ == static_cast<int>(index);
        const auto row = D2D1::RoundedRect(
            D2D1::RectF(static_cast<float>(item.bounds.left), static_cast<float>(item.bounds.top),
                        static_cast<float>(item.bounds.right), static_cast<float>(item.bounds.bottom)),
            10, 10);
        if (active || is_hovered) {
            context->FillRoundedRectangle(row, active ? selected.Get() : hovered.Get());
            context->DrawRoundedRectangle(row, active ? accent.Get() : border.Get(),
                                           active ? 1.5F : 0.8F);
        }
        ID2D1Brush* glyph = item.action == LinearAction::Clear
            ? static_cast<ID2D1Brush*>(danger.Get())
            : static_cast<ID2D1Brush*>(active ? accent.Get() : text.Get());
        switch (item.action) {
            case LinearAction::Visibility:
                if (controller_.state().annotations_visible) {
                    context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 10, 6), glyph, 1.8F);
                    context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 2.8F, 2.8F), glyph);
                } else {
                    context->DrawLine(D2D1::Point2F(cx - 10, cy + 3),
                                      D2D1::Point2F(cx + 10, cy + 3), glyph, 2.0F);
                    context->DrawLine(D2D1::Point2F(cx - 7, cy + 3),
                                      D2D1::Point2F(cx - 4, cy + 8), glyph, 1.4F);
                    context->DrawLine(D2D1::Point2F(cx + 7, cy + 3),
                                      D2D1::Point2F(cx + 4, cy + 8), glyph, 1.4F);
                }
                break;
            case LinearAction::Interact:
                if (current_tool == Tool::Interact) {
                    context->DrawLine(D2D1::Point2F(cx - 7, cy - 10),
                                      D2D1::Point2F(cx + 7, cy + 7), glyph, 2.0F);
                    context->DrawLine(D2D1::Point2F(cx - 7, cy - 10),
                                      D2D1::Point2F(cx - 4, cy + 10), glyph, 2.0F);
                } else {
                    context->DrawLine(D2D1::Point2F(cx - 8, cy + 8),
                                      D2D1::Point2F(cx + 8, cy - 8), glyph, 2.4F);
                    context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - 9, cy + 9), 2, 2), glyph);
                }
                break;
            case LinearAction::ToolPanel:
                for (int y : {-5, 5}) for (int x : {-5, 5})
                    context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(
                        cx + static_cast<float>(x), cy + static_cast<float>(y)),
                        2.2F, 2.2F), glyph);
                break;
            case LinearAction::Highlighter:
                context->DrawLine(D2D1::Point2F(cx - 8, cy + 6),
                                  D2D1::Point2F(cx + 7, cy - 6), glyph, 5.2F);
                context->DrawLine(D2D1::Point2F(cx - 10, cy + 9),
                                  D2D1::Point2F(cx - 3, cy + 9), accent.Get(), 2.0F);
                break;
            case LinearAction::Eraser:
                context->DrawRoundedRectangle(D2D1::RoundedRect(
                    D2D1::RectF(cx - 9, cy - 6, cx + 9, cy + 6), 3, 3), glyph, 2.0F);
                context->DrawLine(D2D1::Point2F(cx - 3, cy - 6),
                                  D2D1::Point2F(cx + 3, cy + 6), glyph, 1.4F);
                break;
            case LinearAction::Geometry:
                context->DrawRectangle(D2D1::RectF(cx - 10, cy - 8, cx + 2, cy + 5), glyph, 1.8F);
                context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx + 5, cy + 2), 7, 6), glyph, 1.8F);
                break;
            case LinearAction::Text:
                if (glyph_format) context->DrawTextW(L"T", 1, glyph_format.Get(),
                    D2D1::RectF(cx - 14, cy - 14, cx + 14, cy + 14), glyph);
                break;
            case LinearAction::Board:
                context->DrawRoundedRectangle(D2D1::RoundedRect(
                    D2D1::RectF(cx - 11, cy - 8, cx + 11, cy + 7), 2, 2), glyph, 1.8F);
                context->DrawLine(D2D1::Point2F(cx - 5, cy + 10),
                                  D2D1::Point2F(cx + 5, cy + 10), glyph, 1.8F);
                break;
            case LinearAction::Zoom:
                context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx - 2, cy - 2), 7, 7), glyph, 2.0F);
                context->DrawLine(D2D1::Point2F(cx + 3, cy + 3),
                                  D2D1::Point2F(cx + 10, cy + 10), glyph, 2.4F);
                break;
            case LinearAction::Undo:
            case LinearAction::Redo: {
                const float direction = item.action == LinearAction::Undo ? -1.0F : 1.0F;
                ComPtr<ID2D1PathGeometry> curve;
                controller_.graphics().d2d_factory()->CreatePathGeometry(curve.GetAddressOf());
                ComPtr<ID2D1GeometrySink> sink;
                curve->Open(sink.GetAddressOf());
                sink->BeginFigure(D2D1::Point2F(cx + direction * 9, cy + 6), D2D1_FIGURE_BEGIN_HOLLOW);
                sink->AddBezier(D2D1::BezierSegment(
                    D2D1::Point2F(cx + direction * 8, cy - 8),
                    D2D1::Point2F(cx - direction * 6, cy - 9),
                    D2D1::Point2F(cx - direction * 9, cy - 1)));
                sink->EndFigure(D2D1_FIGURE_END_OPEN);
                sink->Close();
                context->DrawGeometry(curve.Get(), glyph, 2.0F);
                context->DrawLine(D2D1::Point2F(cx - direction * 9, cy - 1),
                                  D2D1::Point2F(cx - direction * 4, cy - 5), glyph, 2.0F);
                break;
            }
            case LinearAction::Clear:
                context->DrawRoundedRectangle(D2D1::RoundedRect(
                    D2D1::RectF(cx - 7, cy - 6, cx + 7, cy + 10), 2, 2), glyph, 2.0F);
                context->DrawLine(D2D1::Point2F(cx - 10, cy - 10),
                                  D2D1::Point2F(cx + 10, cy - 10), glyph, 2.0F);
                context->DrawLine(D2D1::Point2F(cx - 4, cy - 13),
                                  D2D1::Point2F(cx + 4, cy - 13), glyph, 2.0F);
                break;
            case LinearAction::Settings:
                context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 7, 7), glyph, 1.8F);
                context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 2.2F, 2.2F), glyph);
                for (int ray = 0; ray < 8; ++ray) {
                    const float angle = static_cast<float>(ray) * 0.785398F;
                    context->DrawLine(
                        D2D1::Point2F(cx + std::cos(angle) * 8.0F,
                                       cy + std::sin(angle) * 8.0F),
                        D2D1::Point2F(cx + std::cos(angle) * 10.5F,
                                       cy + std::sin(angle) * 10.5F), glyph, 1.5F);
                }
                break;
        }
    }

    const auto thickness_panel = D2D1::RoundedRect(
        D2D1::RectF(9, 550, 67, 592), 10, 10);
    context->FillRoundedRectangle(thickness_panel,
        hovered_linear_item_ == -3 ? hovered.Get() : panel.Get());
    context->DrawRoundedRectangle(thickness_panel, border.Get(), 0.9F);
    for (std::size_t index = 0; index < kLinearThicknessPoints.size(); ++index) {
        const float radius = 1.7F + static_cast<float>(index) * 0.75F;
        const auto center = D2D1::Point2F(static_cast<float>(kLinearThicknessPoints[index].x),
                                          static_cast<float>(kLinearThicknessPoints[index].y));
        if (std::abs(controller_.state().thickness - kThicknessSteps[index]) < 0.1F)
            context->DrawEllipse(D2D1::Ellipse(center, radius + 3, radius + 3), accent.Get(), 1.6F);
        context->FillEllipse(D2D1::Ellipse(center, radius, radius),
                             index == 0 ? text.Get() : muted.Get());
    }

    context->DrawLine(D2D1::Point2F(12, 598), D2D1::Point2F(64, 598), border.Get(), 0.8F);
    constexpr std::array<Color, 6> colors{kBlack, kYellow, kBlue, kRed, kGreen, kPurple};
    for (std::size_t index = 0; index < colors.size(); ++index) {
        const POINT point = kLinearColorPoints[index];
        ComPtr<ID2D1SolidColorBrush> color;
        context->CreateSolidColorBrush(d2d_color(colors[index]), color.GetAddressOf());
        if (controller_.state().color == colors[index]) {
            context->DrawEllipse(D2D1::Ellipse(
                D2D1::Point2F(static_cast<float>(point.x), static_cast<float>(point.y)), 8, 8),
                accent.Get(), 1.7F);
        }
        context->FillEllipse(D2D1::Ellipse(
            D2D1::Point2F(static_cast<float>(point.x), static_cast<float>(point.y)), 5.5F, 5.5F),
            color.Get());
        context->DrawEllipse(D2D1::Ellipse(
            D2D1::Point2F(static_cast<float>(point.x), static_cast<float>(point.y)), 5.5F, 5.5F),
            glass.Get(), 0.8F);
    }
    const float plus_x = static_cast<float>(kLinearMoreColorsPoint.x);
    const float plus_y = static_cast<float>(kLinearMoreColorsPoint.y);
    context->DrawLine(D2D1::Point2F(plus_x - 6, plus_y),
                      D2D1::Point2F(plus_x + 6, plus_y), accent.Get(), 1.8F);
    context->DrawLine(D2D1::Point2F(plus_x, plus_y - 6),
                      D2D1::Point2F(plus_x, plus_y + 6), accent.Get(), 1.8F);
}

bool ColorWindow::initialize(GraphicsDevice& graphics) {
    const RECT bounds{0, 0, 344, 330};
    constexpr DWORD ex_style = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                               WS_EX_LAYERED;
    if (!create(L"ElitePen.Colors", L"Colores — Elite Pen", ex_style, WS_POPUP,
                bounds)) return false;
    SetLayeredWindowAttributes(window_, 0, 255, LWA_ALPHA);
    if (!initialize_surface(graphics)) return false;
#ifndef ELITE_PEN_DEBUG
    if (controller_.preferences().exclude_palette_from_capture)
        SetWindowDisplayAffinity(window_, WDA_EXCLUDEFROMCAPTURE);
#endif
    return true;
}

void ColorWindow::toggle_near(HWND anchor) {
    RECT anchor_rect{};
    GetWindowRect(anchor, &anchor_rect);
    RECT desired{anchor_rect.right + 10, anchor_rect.top,
                 anchor_rect.right + 10 + 344, anchor_rect.top + 330};
    HMONITOR monitor = MonitorFromRect(&anchor_rect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    if (desired.right > info.rcWork.right) {
        desired.left = anchor_rect.left - 354;
        desired.right = desired.left + 344;
    }
    desired.top = std::clamp(static_cast<int>(desired.top),
                             static_cast<int>(info.rcWork.top),
                             static_cast<int>(info.rcWork.bottom) - 330);
    SetWindowPos(window_, HWND_TOPMOST, desired.left, desired.top, 344, 330,
                 SWP_SHOWWINDOW | SWP_NOACTIVATE);
    invalidate();
}

void ColorWindow::choose_system_color() {
    static COLORREF custom_colors[16]{};
    CHOOSECOLORW chooser{};
    chooser.lStructSize = sizeof(chooser);
    chooser.hwndOwner = window_;
    chooser.rgbResult = RGB(controller_.state().color.r, controller_.state().color.g,
                            controller_.state().color.b);
    chooser.lpCustColors = custom_colors;
    chooser.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ANYCOLOR;
    SetForegroundWindow(window_);
    if (ChooseColorW(&chooser)) {
        controller_.set_color({GetRValue(chooser.rgbResult), GetGValue(chooser.rgbResult),
                               GetBValue(chooser.rgbResult), 255});
        hide();
    }
}

LRESULT ColorWindow::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_LBUTTONDOWN: {
            const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            for (std::size_t index = 0; index < kExtendedColors.size(); ++index) {
                const int column = static_cast<int>(index % 7);
                const int row = static_cast<int>(index / 7);
                if (point_in_circle(point, 31.0F + static_cast<float>(column) * 47.0F,
                                    57.0F + static_cast<float>(row) * 36.0F, 16.0F)) {
                    controller_.set_color(kExtendedColors[index]);
                    hovered_item_ = -1;
                    hide();
                    return 0;
                }
            }
            if (point.x >= 18 && point.x <= 326 && point.y >= 283 && point.y <= 316) {
                hovered_item_ = -1;
                choose_system_color();
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            int next = -1;
            for (std::size_t index = 0; index < kExtendedColors.size(); ++index) {
                const int column = static_cast<int>(index % 7);
                const int row = static_cast<int>(index / 7);
                if (point_in_circle(point, 31.0F + static_cast<float>(column) * 47.0F,
                                    57.0F + static_cast<float>(row) * 36.0F, 16.0F)) {
                    next = static_cast<int>(index);
                    break;
                }
            }
            if (next < 0 && point.x >= 18 && point.x <= 326 &&
                point.y >= 283 && point.y <= 316) next = 42;
            if (next != hovered_item_) { hovered_item_ = next; invalidate(); }
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window_, 0};
            TrackMouseEvent(&tracking);
            return 0;
        }
        case WM_MOUSELEAVE:
            hovered_item_ = -1;
            invalidate();
            return 0;
        case WM_RBUTTONDOWN:
            hovered_item_ = -1;
            hide();
            return 0;
        default:
            return WindowBase::handle_message(message, wparam, lparam);
    }
}

void ColorWindow::render() {
    auto* context = surface_.begin_draw(D2D1::ColorF(0, 0.0F));
    if (!context) return;
    const auto& theme = current_ui_theme();
    ComPtr<ID2D1SolidColorBrush> panel;
    ComPtr<ID2D1SolidColorBrush> raised;
    ComPtr<ID2D1SolidColorBrush> hover_surface;
    ComPtr<ID2D1SolidColorBrush> border;
    ComPtr<ID2D1SolidColorBrush> text;
    ComPtr<ID2D1SolidColorBrush> muted;
    ComPtr<ID2D1SolidColorBrush> gold;
    ComPtr<ID2D1SolidColorBrush> shadow;
    ComPtr<ID2D1SolidColorBrush> glass;
    ComPtr<ID2D1SolidColorBrush> selected;
    context->CreateSolidColorBrush(theme_color(theme.surface_1, 0.985F), panel.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.surface_3), raised.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.surface_4), hover_surface.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.line), border.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.text), text.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.text_muted), muted.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.violet), gold.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.shadow, theme.light ? 0.18F : 0.45F),
                                   shadow.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(0xFFFFFF, theme.light ? 0.48F : 0.16F),
                                   glass.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.violet_strong), selected.GetAddressOf());
    context->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(4, 5, 344, 330), 20, 20), shadow.Get());
    const auto panel_rect = D2D1::RoundedRect(D2D1::RectF(2, 2, 342, 328), 18, 18);
    context->FillRoundedRectangle(panel_rect, panel.Get());
    context->DrawRoundedRectangle(panel_rect, border.Get(), 1.15F);
    context->DrawLine(D2D1::Point2F(20, 41), D2D1::Point2F(324, 41), border.Get(), 0.8F);

    ComPtr<IDWriteTextFormat> title_format;
    ComPtr<IDWriteTextFormat> caption_format;
    controller_.graphics().dwrite()->CreateTextFormat(L"Segoe UI Variable Text", nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        13.0F, L"es-CO", title_format.GetAddressOf());
    controller_.graphics().dwrite()->CreateTextFormat(L"Segoe UI Variable Text", nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        11.0F, L"es-CO", caption_format.GetAddressOf());
    if (title_format) {
        context->FillRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(18, 17, 22, 31), 2, 2), gold.Get());
        context->DrawTextW(L"COLORES", 7, title_format.Get(),
                           D2D1::RectF(31, 13, 125, 35), text.Get());
    }
    if (caption_format) {
        context->DrawTextW(L"Selecciona tu tinta", 19, caption_format.Get(),
                           D2D1::RectF(142, 15, 320, 35), muted.Get());
    }
    for (std::size_t index = 0; index < kExtendedColors.size(); ++index) {
        const float x = 31.0F + static_cast<float>(index % 7) * 47.0F;
        const float y = 57.0F + static_cast<float>(index / 7) * 36.0F;
        ComPtr<ID2D1SolidColorBrush> color;
        context->CreateSolidColorBrush(d2d_color(kExtendedColors[index]), color.GetAddressOf());
        context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y + 1), 15.0F, 15.0F),
                             shadow.Get());
        if (controller_.state().color == kExtendedColors[index]) {
            context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), 16, 16),
                                  selected.Get(), 2.4F);
        } else if (hovered_item_ == static_cast<int>(index)) {
            context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), 15.5F, 15.5F),
                                 gold.Get(), 1.2F);
        }
        context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), 11.8F, 11.8F), color.Get());
        context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x - 3.2F, y - 3.8F), 3.0F, 1.8F),
                             glass.Get());
        if (kExtendedColors[index] == Color{255, 255, 255, 255}) {
            context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), 11.8F, 11.8F),
                                 border.Get(), 1.0F);
        }
    }
    const auto custom = D2D1::RoundedRect(D2D1::RectF(18, 283, 326, 316), 9, 9);
    context->FillRoundedRectangle(custom, hovered_item_ == 42
        ? hover_surface.Get() : raised.Get());
    context->DrawRoundedRectangle(custom, hovered_item_ == 42 ? gold.Get() : border.Get(),
                                  hovered_item_ == 42 ? 1.3F : 1.1F);
    context->DrawLine(D2D1::Point2F(35, 299), D2D1::Point2F(45, 299), gold.Get(), 1.8F);
    context->DrawLine(D2D1::Point2F(40, 294), D2D1::Point2F(40, 304), gold.Get(), 1.8F);
    if (caption_format) {
        context->DrawTextW(L"Color personalizado…", 20, caption_format.Get(),
                           D2D1::RectF(58, 290, 300, 313), text.Get());
    }
    std::wstring error;
    if (!surface_.end_draw(error)) controller_.report_runtime_error(error);
}

bool ToolWindow::initialize(GraphicsDevice& graphics) {
    const RECT bounds{0, 0, 366, 400};
    constexpr DWORD ex_style = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                               WS_EX_LAYERED;
    if (!create(L"ElitePen.Tools", L"Herramientas — Elite Pen", ex_style, WS_POPUP,
                bounds)) return false;
    SetLayeredWindowAttributes(window_, 0, 255, LWA_ALPHA);
    if (!initialize_surface(graphics)) return false;
#ifndef ELITE_PEN_DEBUG
    if (controller_.preferences().exclude_palette_from_capture)
        SetWindowDisplayAffinity(window_, WDA_EXCLUDEFROMCAPTURE);
#endif
    return true;
}

void ToolWindow::toggle_near(HWND anchor) {
    show_near(anchor, false);
}

void ToolWindow::toggle_geometry_near(HWND anchor) {
    show_near(anchor, true);
}

std::size_t ToolWindow::tool_count() const noexcept {
    return geometry_only_ ? kGeometryTools.size() : kTools.size();
}

Tool ToolWindow::tool_at(std::size_t index) const noexcept {
    return geometry_only_ ? kGeometryTools[index] : kTools[index];
}

void ToolWindow::show_near(HWND anchor, bool geometry_only) {
    geometry_only_ = geometry_only;
    const int panel_height = geometry_only_ ? 112 : 400;
    RECT anchor_rect{};
    GetWindowRect(anchor, &anchor_rect);
    RECT desired{anchor_rect.right + 10, anchor_rect.top,
                 anchor_rect.right + 376, anchor_rect.top + panel_height};
    HMONITOR monitor = MonitorFromRect(&anchor_rect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    if (desired.right > info.rcWork.right) {
        desired.left = anchor_rect.left - 376;
        desired.right = desired.left + 366;
    }
    desired.top = std::clamp(static_cast<int>(desired.top),
                             static_cast<int>(info.rcWork.top),
                             static_cast<int>(info.rcWork.bottom) - panel_height);
    SetWindowPos(window_, HWND_TOPMOST, desired.left, desired.top, 366, panel_height,
                 SWP_SHOWWINDOW | SWP_NOACTIVATE);
    invalidate();
}

LRESULT ToolWindow::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_LBUTTONDOWN) {
        const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        for (std::size_t index = 0; index < tool_count(); ++index) {
            RECT item{};
            if (geometry_only_) {
                const int left = 14 + static_cast<int>(index) * 68;
                item = {left, 43, left + 58, 95};
            } else {
                const int column = static_cast<int>(index % 2);
                const int row = static_cast<int>(index / 2);
                item = {15 + column * 169, 48 + row * 48,
                        15 + column * 169 + 162, 48 + row * 48 + 40};
            }
            if (PtInRect(&item, point)) {
                const Tool tool = tool_at(index);
                hovered_item_ = -1;
                hide();
                controller_.set_tool(tool);
                return 0;
            }
        }
        if (!geometry_only_) {
            constexpr RECT settings_item{15, 340, 351, 382};
            if (PtInRect(&settings_item, point)) {
                hovered_item_ = -1;
                hide();
                controller_.show_settings_window();
                return 0;
            }
        }
        return 0;
    }
    if (message == WM_MOUSEMOVE) {
        const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        int next = -1;
        for (std::size_t index = 0; index < tool_count(); ++index) {
            RECT item{};
            if (geometry_only_) {
                const int left = 14 + static_cast<int>(index) * 68;
                item = {left, 43, left + 58, 95};
            } else {
                const int column = static_cast<int>(index % 2);
                const int row = static_cast<int>(index / 2);
                item = {15 + column * 169, 48 + row * 48,
                        15 + column * 169 + 162, 48 + row * 48 + 40};
            }
            if (PtInRect(&item, point)) { next = static_cast<int>(index); break; }
        }
        if (!geometry_only_) {
            constexpr RECT settings_item{15, 340, 351, 382};
            if (PtInRect(&settings_item, point)) next = static_cast<int>(tool_count());
        }
        if (next != hovered_item_) { hovered_item_ = next; invalidate(); }
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window_, 0};
        TrackMouseEvent(&tracking);
        return 0;
    }
    if (message == WM_MOUSELEAVE) {
        hovered_item_ = -1;
        invalidate();
        return 0;
    }
    if (message == WM_RBUTTONDOWN) { hovered_item_ = -1; hide(); return 0; }
    return WindowBase::handle_message(message, wparam, lparam);
}

void ToolWindow::render() {
    auto* context = surface_.begin_draw(D2D1::ColorF(0, 0.0F));
    if (!context) return;
    const auto& theme = current_ui_theme();
    ComPtr<ID2D1SolidColorBrush> panel;
    ComPtr<ID2D1SolidColorBrush> border;
    ComPtr<ID2D1SolidColorBrush> text;
    ComPtr<ID2D1SolidColorBrush> accent;
    ComPtr<ID2D1SolidColorBrush> hover;
    ComPtr<ID2D1SolidColorBrush> raised;
    ComPtr<ID2D1SolidColorBrush> hover_surface;
    ComPtr<ID2D1SolidColorBrush> muted;
    ComPtr<ID2D1SolidColorBrush> shadow;
    context->CreateSolidColorBrush(theme_color(theme.surface_1, 0.985F), panel.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.line), border.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.text), text.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.violet), accent.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.light ? 0xE5E0FA : 0x29233F), hover.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.surface_2), raised.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.surface_4), hover_surface.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.text_muted), muted.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.shadow, theme.light ? 0.18F : 0.45F),
                                   shadow.GetAddressOf());
    const float panel_bottom = static_cast<float>(surface_.height()) - 2.0F;
    context->FillRoundedRectangle(D2D1::RoundedRect(
        D2D1::RectF(4, 5, 366, static_cast<float>(surface_.height())), 20, 20), shadow.Get());
    const auto background = D2D1::RoundedRect(
        D2D1::RectF(2, 2, 364, panel_bottom), 18, 18);
    context->FillRoundedRectangle(background, panel.Get());
    context->DrawRoundedRectangle(background, border.Get(), 1.15F);
    context->DrawLine(D2D1::Point2F(18, 41), D2D1::Point2F(348, 41), border.Get(), 0.8F);

    ComPtr<IDWriteTextFormat> title_format;
    ComPtr<IDWriteTextFormat> item_format;
    controller_.graphics().dwrite()->CreateTextFormat(L"Segoe UI Variable Text", nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        13.0F, L"es-CO", title_format.GetAddressOf());
    controller_.graphics().dwrite()->CreateTextFormat(L"Segoe UI Variable Text", nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        12.5F, L"es-CO", item_format.GetAddressOf());
    if (title_format) {
        const wchar_t* title = geometry_only_ ? L"FIGURAS" : L"HERRAMIENTAS";
        context->FillRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(17, 17, 21, 31), 2, 2), accent.Get());
        context->DrawTextW(title, static_cast<UINT32>(wcslen(title)), title_format.Get(),
                           D2D1::RectF(30, 13, 220, 36), text.Get());
        context->DrawTextW(L"ELITE PEN", 9, title_format.Get(),
                           D2D1::RectF(270, 13, 348, 36), muted.Get());
    }
    for (std::size_t index = 0; index < tool_count(); ++index) {
        const Tool panel_tool = tool_at(index);
        const float left = geometry_only_
            ? 14.0F + static_cast<float>(index) * 68.0F
            : 15.0F + static_cast<float>(index % 2) * 169.0F;
        const float top = geometry_only_
            ? 43.0F
            : 48.0F + static_cast<float>(index / 2) * 48.0F;
        const float item_width = geometry_only_ ? 58.0F : 162.0F;
        const float item_height = geometry_only_ ? 52.0F : 40.0F;
        const auto item = D2D1::RoundedRect(
            D2D1::RectF(left, top, left + item_width, top + item_height), 9, 9);
        const bool active = controller_.state().tool == panel_tool;
        const bool hovered = hovered_item_ == static_cast<int>(index);
        context->FillRoundedRectangle(item, active ? hover.Get()
            : (hovered ? hover_surface.Get() : raised.Get()));
        context->DrawRoundedRectangle(item, active ? accent.Get()
            : (hovered ? muted.Get() : border.Get()), active ? 1.8F : 0.85F);
        const float icon_x = left + (geometry_only_ ? item_width * 0.5F : 20.0F);
        const float icon_y = top + item_height * 0.5F;
        switch (panel_tool) {
            case Tool::Interact:
                context->DrawLine(D2D1::Point2F(icon_x - 6, icon_y - 9),
                                  D2D1::Point2F(icon_x + 6, icon_y + 8), text.Get(), 2.0F);
                context->DrawLine(D2D1::Point2F(icon_x - 6, icon_y - 9),
                                  D2D1::Point2F(icon_x - 3, icon_y + 10), text.Get(), 2.0F);
                break;
            case Tool::Pen:
                context->DrawLine(D2D1::Point2F(icon_x - 7, icon_y + 7),
                                  D2D1::Point2F(icon_x + 7, icon_y - 7), text.Get(), 2.2F);
                context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(icon_x - 8, icon_y + 8),
                                                   1.8F, 1.8F), text.Get());
                break;
            case Tool::Highlighter:
                context->DrawLine(D2D1::Point2F(icon_x - 7, icon_y + 6),
                                  D2D1::Point2F(icon_x + 7, icon_y - 6), text.Get(), 5.0F);
                break;
            case Tool::Eraser:
                context->DrawRoundedRectangle(D2D1::RoundedRect(
                    D2D1::RectF(icon_x - 8, icon_y - 6, icon_x + 8, icon_y + 6), 3, 3),
                    text.Get(), 2.0F);
                break;
            case Tool::Rectangle:
                context->DrawRectangle(D2D1::RectF(icon_x - 8, icon_y - 7,
                                                   icon_x + 8, icon_y + 7), text.Get(), 1.8F);
                break;
            case Tool::Ellipse:
                context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(icon_x, icon_y), 9, 7),
                                     text.Get(), 1.8F);
                break;
            case Tool::Line:
                context->DrawLine(D2D1::Point2F(icon_x - 8, icon_y + 7),
                                  D2D1::Point2F(icon_x + 8, icon_y - 7), text.Get(), 2.0F);
                break;
            case Tool::Arrow:
                context->DrawLine(D2D1::Point2F(icon_x - 8, icon_y + 5),
                                  D2D1::Point2F(icon_x + 8, icon_y - 5), text.Get(), 2.0F);
                context->DrawLine(D2D1::Point2F(icon_x + 8, icon_y - 5),
                                  D2D1::Point2F(icon_x + 2, icon_y - 5), text.Get(), 2.0F);
                break;
            case Tool::CurvedArrow: {
                ComPtr<ID2D1PathGeometry> curve;
                controller_.graphics().d2d_factory()->CreatePathGeometry(curve.GetAddressOf());
                ComPtr<ID2D1GeometrySink> curve_sink;
                curve->Open(curve_sink.GetAddressOf());
                curve_sink->BeginFigure(D2D1::Point2F(icon_x - 8, icon_y + 5),
                                        D2D1_FIGURE_BEGIN_HOLLOW);
                curve_sink->AddBezier(D2D1::BezierSegment(
                    D2D1::Point2F(icon_x - 3, icon_y - 8),
                    D2D1::Point2F(icon_x + 5, icon_y - 8),
                    D2D1::Point2F(icon_x + 8, icon_y - 2)));
                curve_sink->EndFigure(D2D1_FIGURE_END_OPEN);
                curve_sink->Close();
                context->DrawGeometry(curve.Get(), text.Get(), 2.0F);
                draw_arrow_head(context, text.Get(), {icon_x + 5, icon_y - 8},
                                {icon_x + 8, icon_y - 2}, 2.0F);
                break;
            }
            case Tool::Text:
                if (item_format) context->DrawTextW(L"T", 1, item_format.Get(),
                    D2D1::RectF(icon_x - 5, icon_y - 10, icon_x + 8, icon_y + 10), text.Get());
                break;
            case Tool::Screenshot:
                context->DrawRectangle(D2D1::RectF(icon_x - 9, icon_y - 7,
                                                   icon_x + 9, icon_y + 7), text.Get(), 1.7F);
                context->DrawLine(D2D1::Point2F(icon_x - 5, icon_y + 3),
                                  D2D1::Point2F(icon_x - 1, icon_y - 1), text.Get(), 1.5F);
                context->DrawLine(D2D1::Point2F(icon_x - 1, icon_y - 1),
                                  D2D1::Point2F(icon_x + 6, icon_y + 5), text.Get(), 1.5F);
                break;
            case Tool::Zoom:
                context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(icon_x - 2, icon_y - 2),
                                                   6, 6), text.Get(), 2.0F);
                context->DrawLine(D2D1::Point2F(icon_x + 3, icon_y + 3),
                                  D2D1::Point2F(icon_x + 9, icon_y + 9), text.Get(), 2.4F);
                break;
            default:
                context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(icon_x, icon_y),
                                                   panel_tool == Tool::Eraser ? 6.0F : 3.0F,
                                                   panel_tool == Tool::Eraser ? 6.0F : 3.0F),
                                     text.Get());
                break;
        }
        if (item_format && !geometry_only_) {
            const wchar_t* name = tool_name(panel_tool);
            context->DrawTextW(name, static_cast<UINT32>(wcslen(name)), item_format.Get(),
                               D2D1::RectF(left + 40, top + 11, left + 155, top + 35),
                               text.Get());
        }
    }
    if (!geometry_only_) {
        const auto settings_item = D2D1::RoundedRect(D2D1::RectF(15, 340, 351, 382), 9, 9);
        const bool settings_hovered = hovered_item_ == static_cast<int>(tool_count());
        context->FillRoundedRectangle(settings_item,
            settings_hovered ? hover_surface.Get() : raised.Get());
        context->DrawRoundedRectangle(settings_item,
            settings_hovered ? muted.Get() : border.Get(), 0.9F);
        constexpr D2D1_POINT_2F gear_center{35, 361};
        context->DrawEllipse(D2D1::Ellipse(gear_center, 6.0F, 6.0F), accent.Get(), 1.8F);
        context->FillEllipse(D2D1::Ellipse(gear_center, 2.0F, 2.0F), accent.Get());
        for (int index = 0; index < 8; ++index) {
            const float angle = static_cast<float>(index) * 0.785398F;
            context->DrawLine(
                D2D1::Point2F(gear_center.x + std::cos(angle) * 7.0F,
                               gear_center.y + std::sin(angle) * 7.0F),
                D2D1::Point2F(gear_center.x + std::cos(angle) * 9.0F,
                               gear_center.y + std::sin(angle) * 9.0F), accent.Get(), 1.6F);
        }
        if (item_format) {
            context->DrawTextW(L"Configuracion", 13, item_format.Get(),
                               D2D1::RectF(55, 350, 220, 376), text.Get());
        }
    }
    std::wstring error;
    if (!surface_.end_draw(error)) controller_.report_runtime_error(error);
}

bool TextInputWindow::initialize(GraphicsDevice& graphics) {
    const RECT bounds{0, 0, 640, 420};
    // DirectComposition supplies the alpha channel. A legacy layered backing
    // surface can be promoted to an opaque black rectangle when this large,
    // focused window is shown over another DirectComposition surface.
    constexpr DWORD ex_style = WS_EX_TOPMOST | WS_EX_TOOLWINDOW |
                               WS_EX_NOREDIRECTIONBITMAP;
    if (!create(L"ElitePen.TextInput", L"Insertar texto — Elite Pen",
                 ex_style, WS_POPUP, bounds)) return false;
    if (!initialize_surface(graphics)) return false;
#ifndef ELITE_PEN_DEBUG
    if (controller_.preferences().exclude_palette_from_capture)
        SetWindowDisplayAffinity(window_, WDA_EXCLUDEFROMCAPTURE);
#endif
    return true;
}

void TextInputWindow::show_at(PointF position, Color color, float thickness) {
    if (active_) commit();
    position_ = position;
    color_ = color;
    thickness_ = thickness;
    text_.clear();
    active_ = true;
    caret_visible_ = true;

    POINT requested{static_cast<LONG>(std::lround(position.x)),
                    static_cast<LONG>(std::lround(position.y))};
    HMONITOR monitor = MonitorFromPoint(requested, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    const int x = std::clamp(requested.x, info.rcWork.left, info.rcWork.right - 40);
    const int y = std::clamp(requested.y, info.rcWork.top, info.rcWork.bottom - 40);
    const int width = std::max(40, std::min(640, static_cast<int>(info.rcWork.right) - x));
    const int height = std::max(40, std::min(420, static_cast<int>(info.rcWork.bottom) - y));
    SetWindowPos(window_, HWND_TOPMOST, x, y, width, height, SWP_SHOWWINDOW);
    SetForegroundWindow(window_);
    SetFocus(window_);
    SetTimer(window_, 31, 500, nullptr);
    invalidate();
}

void TextInputWindow::update_style(Color color, float thickness) {
    color_ = color;
    thickness_ = thickness;
    if (active_) invalidate();
}

void TextInputWindow::commit() {
    if (!active_) return;
    active_ = false;
    KillTimer(window_, 31);
    ShowWindow(window_, SW_HIDE);
    std::wstring text = std::move(text_);
    text_.clear();
    if (text.find_first_not_of(L" \t\r\n") != std::wstring::npos) {
        controller_.commit_text(position_, color_, thickness_, std::move(text));
    }
}

void TextInputWindow::cancel() {
    if (!active_) return;
    active_ = false;
    text_.clear();
    KillTimer(window_, 31);
    ShowWindow(window_, SW_HIDE);
}

void TextInputWindow::render() {
    auto* context = surface_.begin_draw(D2D1::ColorF(0, 0.0F));
    if (!context) return;
    if (active_) {
        const float font_size = std::clamp(thickness_ * 3.4F, 16.0F, 72.0F);
        ComPtr<ID2D1SolidColorBrush> brush;
        context->CreateSolidColorBrush(d2d_color(color_), brush.GetAddressOf());
        ComPtr<IDWriteTextFormat> format;
        controller_.graphics().dwrite()->CreateTextFormat(
            L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, font_size, L"es-CO", format.GetAddressOf());
        RECT client{};
        GetClientRect(window_, &client);
        ComPtr<IDWriteTextLayout> layout;
        if (format) {
            controller_.graphics().dwrite()->CreateTextLayout(
                text_.c_str(), static_cast<UINT32>(text_.size()), format.Get(),
                static_cast<float>(std::clamp(static_cast<int>(client.right), 1, 600)),
                static_cast<float>(std::clamp(static_cast<int>(client.bottom), 1, 400)),
                layout.GetAddressOf());
        }
        if (layout && brush) {
            context->DrawTextLayout(D2D1::Point2F(0, 0), layout.Get(), brush.Get());
            if (caret_visible_) {
                FLOAT caret_x = 0;
                FLOAT caret_y = 0;
                DWRITE_HIT_TEST_METRICS metrics{};
                layout->HitTestTextPosition(static_cast<UINT32>(text_.size()), FALSE,
                                            &caret_x, &caret_y, &metrics);
                const float caret_height = metrics.height > 0 ? metrics.height : font_size * 1.2F;
                context->DrawLine(D2D1::Point2F(caret_x, caret_y),
                                  D2D1::Point2F(caret_x, caret_y + caret_height),
                                  brush.Get(), 1.7F);
            }
        }
    }
    std::wstring error;
    if (!surface_.end_draw(error)) controller_.report_runtime_error(error);
}

LRESULT TextInputWindow::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    constexpr std::size_t max_text_length = 8192;
    switch (message) {
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) { cancel(); return 0; }
            if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && wparam == 'V') {
                SendMessageW(window_, WM_PASTE, 0, 0);
                return 0;
            }
            break;
        case WM_CHAR:
            if (!active_) return 0;
            if (wparam == L'\b') {
                if (!text_.empty()) {
                    text_.pop_back();
                    if (!text_.empty() && text_.back() >= 0xD800 && text_.back() <= 0xDBFF) {
                        text_.pop_back();
                    }
                }
            } else if (wparam == L'\r') {
                if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) commit();
                else if (text_.size() < max_text_length) text_.push_back(L'\n');
            } else if (wparam == L'\t') {
                text_.append(std::min<std::size_t>(4, max_text_length - text_.size()),
                             L' ');
            } else if (wparam >= 0x20 && wparam != 0x7F &&
                       text_.size() < max_text_length &&
                       !(wparam >= 0xD800 && wparam <= 0xDBFF &&
                         text_.size() + 1 >= max_text_length)) {
                text_.push_back(static_cast<wchar_t>(wparam));
            }
            caret_visible_ = true;
            invalidate();
            return 0;
        case WM_PASTE:
            if (active_ && OpenClipboard(window_)) {
                if (HANDLE data = GetClipboardData(CF_UNICODETEXT)) {
                    if (const auto* value = static_cast<const wchar_t*>(GlobalLock(data))) {
                        const std::size_t remaining = max_text_length - text_.size();
                        const std::size_t available = GlobalSize(data) / sizeof(wchar_t);
                        text_.append(value, wcsnlen(value, std::min(remaining, available)));
                        if (!text_.empty() && text_.back() >= 0xD800 &&
                            text_.back() <= 0xDBFF) text_.pop_back();
                        GlobalUnlock(data);
                    }
                }
                CloseClipboard();
                caret_visible_ = true;
                invalidate();
            }
            return 0;
        case WM_TIMER:
            if (wparam == 31 && active_) {
                caret_visible_ = !caret_visible_;
                invalidate();
                return 0;
            }
            break;
        case kQaCommitInlineTextMessage:
            commit();
            return 0;
        case WM_CLOSE:
            cancel();
            return 0;
        default: break;
    }
    return WindowBase::handle_message(message, wparam, lparam);
}

SettingsWindow::~SettingsWindow() {
    if (title_font_) DeleteObject(title_font_);
    if (body_font_) DeleteObject(body_font_);
    if (small_font_) DeleteObject(small_font_);
    if (background_brush_) DeleteObject(background_brush_);
    if (card_brush_) DeleteObject(card_brush_);
}

bool SettingsWindow::initialize() {
    const RECT bounds{0, 0, 590, 590};
    if (!create(L"ElitePen.Settings", L"Configuracion — Elite Pen",
                WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                WS_POPUP, bounds)) return false;
    apply_premium_window_chrome(window_);
    const auto& theme = current_ui_theme();
    background_brush_ = CreateSolidBrush(theme_colorref(theme.background));
    card_brush_ = CreateSolidBrush(theme_colorref(theme.surface_2));
    title_font_ = CreateFontW(-25, 0, 0, 0, 600, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Variable Display");
    body_font_ = CreateFontW(-17, 0, 0, 0, 400, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Variable Text");
    small_font_ = CreateFontW(-14, 0, 0, 0, 400, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Variable Text");
    title_ = CreateWindowW(L"STATIC", L"ELITE PEN", WS_CHILD | WS_VISIBLE,
                           31, 12, 473, 30, window_, nullptr,
                           GetModuleHandleW(nullptr), nullptr);
    subtitle_ = CreateWindowW(L"STATIC", L"Preferencias de anotación y presentación · 2.9.0",
                              WS_CHILD | WS_VISIBLE, 32, 40, 473, 20, window_, nullptr,
                              GetModuleHandleW(nullptr), nullptr);
    chrome_close_ = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                  BS_OWNERDRAW, 541, 14, 32, 32, window_,
                                  reinterpret_cast<HMENU>(IDCANCEL),
                                  GetModuleHandleW(nullptr), nullptr);
    tab_general_ = CreateWindowW(L"BUTTON", L"General", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                 BS_OWNERDRAW, 18, 68, 150, 34, window_,
                                 reinterpret_cast<HMENU>(4101),
                                 GetModuleHandleW(nullptr), nullptr);
    tab_shortcuts_ = CreateWindowW(L"BUTTON", L"Atajos", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                   BS_OWNERDRAW, 174, 68, 150, 34, window_,
                                   reinterpret_cast<HMENU>(4102),
                                   GetModuleHandleW(nullptr), nullptr);
    tab_help_ = CreateWindowW(L"BUTTON", L"Ayuda", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                              BS_OWNERDRAW, 330, 68, 150, 34, window_,
                              reinterpret_cast<HMENU>(4104),
                              GetModuleHandleW(nullptr), nullptr);
    capture_ = CreateWindowW(L"BUTTON", L"Ocultar Elite Pen en capturas de pantalla",
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                             24, 119, 500, 25, window_, reinterpret_cast<HMENU>(4001),
                             GetModuleHandleW(nullptr), nullptr);
    confirm_clear_ = CreateWindowW(L"BUTTON", L"Pedir confirmación antes de limpiar",
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                   24, 150, 500, 25, window_, reinterpret_cast<HMENU>(4002),
                                   GetModuleHandleW(nullptr), nullptr);
    start_interact_ = CreateWindowW(L"BUTTON", L"Iniciar en modo cursor normal",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                    24, 181, 500, 25, window_, reinterpret_cast<HMENU>(4003),
                                    GetModuleHandleW(nullptr), nullptr);
    highlight_cursor_ = CreateWindowW(L"BUTTON", L"Resaltar la posición del cursor",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                      24, 212, 270, 25, window_, reinterpret_cast<HMENU>(4008),
                                      GetModuleHandleW(nullptr), nullptr);
    fade_label_ = CreateWindowW(L"STATIC", L"Tinta:", WS_CHILD | WS_VISIBLE,
                                316, 215, 52, 22, window_, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
    fade_ = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                          CBS_DROPDOWNLIST, 370, 210, 188, 130, window_,
                          reinterpret_cast<HMENU>(4009), GetModuleHandleW(nullptr), nullptr);
    for (const wchar_t* value : {L"Permanente", L"3 segundos", L"8 segundos", L"15 segundos"}) {
        SendMessageW(fade_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
    }
    control_mode_label_ = CreateWindowW(L"STATIC", L"Presentación:",
                                        WS_CHILD | WS_VISIBLE, 24, 270, 102, 24,
                                        window_, nullptr, GetModuleHandleW(nullptr), nullptr);
    control_mode_ = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                  CBS_DROPDOWNLIST, 130, 265, 190, 110, window_,
                                  reinterpret_cast<HMENU>(4014),
                                  GetModuleHandleW(nullptr), nullptr);
    for (const wchar_t* value : {L"Paleta de pintor", L"Lineal vertical"}) {
        SendMessageW(control_mode_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
    }
    palette_size_label_ = CreateWindowW(L"STATIC", L"Tamaño:",
                                        WS_CHILD | WS_VISIBLE, 340, 270, 65, 24,
                                        window_, nullptr, GetModuleHandleW(nullptr), nullptr);
    palette_size_ = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                  CBS_DROPDOWNLIST, 405, 265, 153, 150, window_,
                                  reinterpret_cast<HMENU>(4011),
                                  GetModuleHandleW(nullptr), nullptr);
    for (const wchar_t* value : {L"Compacta · 80 %", L"Estándar · 100 %",
                                  L"Grande · 125 %", L"Muy grande · 150 %"}) {
        SendMessageW(palette_size_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
    }
    thickness_label_ = CreateWindowW(L"STATIC", L"Grosor inicial:", WS_CHILD | WS_VISIBLE,
                                     24, 313, 112, 24, window_, nullptr,
                                     GetModuleHandleW(nullptr), nullptr);
    thickness_ = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                               CBS_DROPDOWNLIST, 140, 308, 108, 160, window_,
                               reinterpret_cast<HMENU>(4010), GetModuleHandleW(nullptr), nullptr);
    for (const wchar_t* value : {L"2 px", L"4 px", L"7 px", L"12 px", L"20 px"}) {
        SendMessageW(thickness_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
    }
    zoom_label_ = CreateWindowW(L"STATIC", L"Ampliación:", WS_CHILD | WS_VISIBLE,
                                275, 313, 102, 24, window_, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
    zoom_ = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                          CBS_DROPDOWNLIST, 380, 308, 178, 180, window_,
                          reinterpret_cast<HMENU>(4004), GetModuleHandleW(nullptr), nullptr);
    for (const wchar_t* value : {L"1.5×", L"2×", L"3×", L"4×", L"6×", L"8×"}) {
        SendMessageW(zoom_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
    }
    zoom_view_label_ = CreateWindowW(L"STATIC", L"Vista:",
                                     WS_CHILD | WS_VISIBLE, 24, 356, 56, 24,
                                     window_, nullptr, GetModuleHandleW(nullptr), nullptr);
    zoom_view_ = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                               CBS_DROPDOWNLIST, 84, 351, 246, 150, window_,
                               reinterpret_cast<HMENU>(4006), GetModuleHandleW(nullptr), nullptr);
    for (const wchar_t* value : {L"Pantalla completa", L"Lente", L"Acoplada"}) {
        SendMessageW(zoom_view_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
    }
    zoom_invert_ = CreateWindowW(L"BUTTON", L"Invertir colores en zoom",
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                 350, 353, 208, 25, window_, reinterpret_cast<HMENU>(4007),
                                 GetModuleHandleW(nullptr), nullptr);
    zoom_lens_size_label_ = CreateWindowW(
        L"STATIC", L"Lente circular:", WS_CHILD | WS_VISIBLE,
        24, 390, 112, 24, window_, nullptr, GetModuleHandleW(nullptr), nullptr);
    zoom_lens_size_ = CreateWindowW(
        L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
        140, 385, 184, 160, window_, reinterpret_cast<HMENU>(4015),
        GetModuleHandleW(nullptr), nullptr);
    for (const wchar_t* value : {L"Pequeña · 360 px", L"Media · 440 px",
                                  L"Estándar · 520 px", L"Grande · 640 px",
                                  L"Muy grande · 760 px"}) {
        SendMessageW(zoom_lens_size_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
    }
    palette_size_hint_ = CreateWindowW(L"STATIC",
        L"Shift + rueda o [ ] cambia el diámetro de la lente en vivo.",
        WS_CHILD | WS_VISIBLE, 340, 383, 218, 31, window_, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    theme_label_ = CreateWindowW(L"STATIC", L"Apariencia:",
                                 WS_CHILD | WS_VISIBLE, 24, 449, 88, 27, window_, nullptr,
                                 GetModuleHandleW(nullptr), nullptr);
    theme_dark_ = CreateWindowW(L"BUTTON", L"Oscuro", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                BS_OWNERDRAW, 116, 442, 104, 34, window_,
                                reinterpret_cast<HMENU>(4012), GetModuleHandleW(nullptr), nullptr);
    theme_light_ = CreateWindowW(L"BUTTON", L"Claro", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                 BS_OWNERDRAW, 228, 442, 104, 34, window_,
                                 reinterpret_cast<HMENU>(4013), GetModuleHandleW(nullptr), nullptr);
    reset_position_ = CreateWindowW(L"BUTTON", L"Restablecer posición de Elite Pen",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                    24, 489, 284, 31, window_, reinterpret_cast<HMENU>(4005),
                                    GetModuleHandleW(nullptr), nullptr);
    shortcuts_ = CreateWindowW(L"STATIC",
        L"Atajos de Elite Pen. Gestos: Shift Línea; Ctrl Rectángulo; Tab Elipse; "
        L"Ctrl+Shift Flecha; Shift+Tab Flecha curva. Zoom: E Zoom editable; "
        L"Mano usa la app ampliada; Lápiz congela y anota; Espacio vuelve a Mano; "
        L"en Lente, Shift+rueda o [ ] cambia el diámetro.",
        WS_CHILD | SS_OWNERDRAW, 24, 119, 540, 400, window_,
        reinterpret_cast<HMENU>(4103), GetModuleHandleW(nullptr), nullptr);
    for (std::size_t index = 0; index < hotkey_buttons_.size(); ++index) {
        hotkey_buttons_[index] = CreateWindowW(
            L"BUTTON", L"", WS_CHILD | BS_OWNERDRAW,
            350, 147 + static_cast<int>(index) * 35, 158, 28, window_,
            reinterpret_cast<HMENU>(4200 + index), GetModuleHandleW(nullptr), nullptr);
        hotkey_edit_buttons_[index] = CreateWindowW(
            L"BUTTON", L"Editar", WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
            514, 147 + static_cast<int>(index) * 35, 32, 28, window_,
            reinterpret_cast<HMENU>(4400 + index), GetModuleHandleW(nullptr), nullptr);
    }
    shortcut_scrollbar_ = CreateWindowW(
        L"SCROLLBAR", L"", WS_CHILD | SBS_VERT,
        550, 147, 14, 203, window_, reinterpret_cast<HMENU>(4408),
        GetModuleHandleW(nullptr), nullptr);
    reset_hotkeys_ = CreateWindowW(L"BUTTON", L"Restablecer atajos",
                                    WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
                                    405, 112, 146, 29, window_,
                                    reinterpret_cast<HMENU>(4300),
                                    GetModuleHandleW(nullptr), nullptr);
    help_ = CreateWindowW(L"STATIC",
        L"Ayuda de Elite Pen 2.9.0. Anotación, pizarra, captura y zoom para Windows. "
        L"Código abierto bajo Apache License 2.0. Desarrollado por Power Elite Studio.",
        WS_CHILD | SS_OWNERDRAW, 24, 119, 540, 400, window_,
        reinterpret_cast<HMENU>(4105), GetModuleHandleW(nullptr), nullptr);
    help_website_ = CreateWindowW(L"BUTTON", L"Visitar powerelite.studio",
        WS_CHILD | WS_TABSTOP | BS_OWNERDRAW, 32, 465, 250, 38, window_,
        reinterpret_cast<HMENU>(4500), GetModuleHandleW(nullptr), nullptr);
    help_source_ = CreateWindowW(L"BUTTON", L"Código fuente en GitHub",
        WS_CHILD | WS_TABSTOP | BS_OWNERDRAW, 292, 465, 266, 38, window_,
        reinterpret_cast<HMENU>(4501), GetModuleHandleW(nullptr), nullptr);
    close_ = CreateWindowW(L"BUTTON", L"Cerrar", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                           BS_OWNERDRAW, 455, 542, 100, 32, window_,
                           reinterpret_cast<HMENU>(IDOK), GetModuleHandleW(nullptr), nullptr);
    for (HWND child : {title_, subtitle_, tab_general_, tab_shortcuts_, tab_help_,
                       capture_, confirm_clear_,
                       start_interact_, highlight_cursor_,
                       fade_label_, fade_, thickness_label_, thickness_, zoom_label_,
                       zoom_, zoom_view_label_, zoom_view_, zoom_invert_,
                       zoom_lens_size_label_, zoom_lens_size_, control_mode_label_,
                       control_mode_, palette_size_label_, palette_size_, palette_size_hint_,
                       theme_label_, theme_dark_, theme_light_,
                       reset_position_,
                       shortcuts_, shortcut_scrollbar_, reset_hotkeys_, help_, help_website_,
                       help_source_,
                       close_, chrome_close_}) {
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(body_font_), TRUE);
        SetWindowTheme(child, theme.light ? L"Explorer" : L"DarkMode_Explorer", nullptr);
    }
    for (HWND button : hotkey_buttons_) {
        SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(small_font_), TRUE);
        SetWindowTheme(button, theme.light ? L"Explorer" : L"DarkMode_Explorer", nullptr);
    }
    for (HWND button : hotkey_edit_buttons_) {
        SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(small_font_), TRUE);
        SetWindowTheme(button, theme.light ? L"Explorer" : L"DarkMode_Explorer", nullptr);
    }
    SendMessageW(title_, WM_SETFONT, reinterpret_cast<WPARAM>(title_font_), TRUE);
    SendMessageW(subtitle_, WM_SETFONT, reinterpret_cast<WPARAM>(small_font_), TRUE);
    SendMessageW(palette_size_hint_, WM_SETFONT, reinterpret_cast<WPARAM>(small_font_), TRUE);
    SendMessageW(shortcuts_, WM_SETFONT, reinterpret_cast<WPARAM>(small_font_), TRUE);
    SendMessageW(help_, WM_SETFONT, reinterpret_cast<WPARAM>(small_font_), TRUE);
    SetWindowSubclass(fade_, premium_combo_subclass, 4009, 0);
    SetWindowSubclass(thickness_, premium_combo_subclass, 4010, 0);
    SetWindowSubclass(zoom_, premium_combo_subclass, 4004, 0);
    SetWindowSubclass(zoom_view_, premium_combo_subclass, 4006, 0);
    SetWindowSubclass(zoom_lens_size_, premium_combo_subclass, 4015, 0);
    SetWindowSubclass(palette_size_, premium_combo_subclass, 4011, 0);
    SetWindowSubclass(control_mode_, premium_combo_subclass, 4014, 0);
    apply_theme();
    show_tab(0);
    return true;
}

void SettingsWindow::apply_theme() {
    const auto& theme = current_ui_theme();
    if (background_brush_) DeleteObject(background_brush_);
    if (card_brush_) DeleteObject(card_brush_);
    background_brush_ = CreateSolidBrush(theme_colorref(theme.background));
    card_brush_ = CreateSolidBrush(theme_colorref(theme.surface_2));
    apply_premium_window_chrome(window_);
    const wchar_t* native_theme = theme.light ? L"Explorer" : L"DarkMode_Explorer";
    for (HWND child : {title_, subtitle_, tab_general_, tab_shortcuts_, tab_help_,
                       capture_, confirm_clear_,
                       start_interact_, highlight_cursor_, fade_label_, fade_, thickness_label_,
                       thickness_, zoom_label_, zoom_, zoom_view_label_, zoom_view_, zoom_invert_,
                       zoom_lens_size_label_, zoom_lens_size_,
                       control_mode_label_, control_mode_, palette_size_label_, palette_size_,
                       palette_size_hint_, theme_label_,
                       theme_dark_, theme_light_, reset_position_, shortcuts_, shortcut_scrollbar_,
                       reset_hotkeys_, help_, help_website_, help_source_, close_, chrome_close_}) {
        if (child) SetWindowTheme(child, native_theme, nullptr);
    }
    for (HWND child : hotkey_buttons_) if (child) SetWindowTheme(child, native_theme, nullptr);
    for (HWND child : hotkey_edit_buttons_) if (child) SetWindowTheme(child, native_theme, nullptr);
    RedrawWindow(window_, nullptr, nullptr,
                 RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
}

void SettingsWindow::paint_background(HDC dc) {
    const auto& theme = current_ui_theme();
    RECT client{};
    GetClientRect(window_, &client);
    FillRect(dc, &client, background_brush_);

    HPEN frame_pen = CreatePen(PS_SOLID, 1, theme_colorref(theme.line));
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    HGDIOBJ old_pen = SelectObject(dc, frame_pen);
    RoundRect(dc, 0, 0, client.right, client.bottom, 22, 22);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(frame_pen);

    HBRUSH brand_brush = CreateSolidBrush(theme_colorref(theme.violet));
    old_brush = SelectObject(dc, brand_brush);
    old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, 18, 17, 22, 31, 4, 4);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(brand_brush);

    HPEN border = CreatePen(PS_SOLID, 1, theme_colorref(theme.line));
    old_brush = SelectObject(dc, card_brush_);
    old_pen = SelectObject(dc, border);
    if (active_tab_ == 0) {
        RoundRect(dc, 15, 110, 575, 252, 18, 18);
        RoundRect(dc, 15, 253, 575, 413, 18, 18);
        RoundRect(dc, 15, 414, 575, 530, 18, 18);
    } else {
        RoundRect(dc, 15, 110, 575, 530, 18, 18);
    }
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(border);

    HPEN gold_pen = CreatePen(PS_SOLID, 3, theme_colorref(theme.violet));
    old_pen = SelectObject(dc, gold_pen);
    const int tab_accent_left = 25 + active_tab_ * 156;
    MoveToEx(dc, tab_accent_left, 110, nullptr);
    LineTo(dc, tab_accent_left + 67, 110);
    SelectObject(dc, old_pen);
    DeleteObject(gold_pen);
}

void SettingsWindow::paint_shortcuts(HDC dc, RECT bounds) {
    const auto& theme = current_ui_theme();
    FillRect(dc, &bounds, card_brush_);
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ previous_font = SelectObject(dc, body_font_);
    SetTextColor(dc, theme_colorref(theme.violet));
    RECT heading{bounds.left, bounds.top + 2, bounds.right - 168, bounds.top + 23};
    DrawTextW(dc, L"ATAJOS CONFIGURABLES", -1, &heading,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    for (std::size_t slot = 0; slot < kVisibleShortcutRows; ++slot) {
        const std::size_t index = shortcut_scroll_offset_ + slot;
        if (index >= kHotkeyInfo.size()) break;
        const int y = bounds.top + 28 + static_cast<int>(slot) * 35;
        SelectObject(dc, small_font_);
        SetTextColor(dc, theme_colorref(theme.text));
        RECT title{bounds.left, y, bounds.left + 122, y + 16};
        DrawTextW(dc, kHotkeyInfo[index].title, -1, &title,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        SetTextColor(dc, theme_colorref(theme.text_muted));
        RECT description{bounds.left + 126, y, bounds.left + 318, y + 16};
        DrawTextW(dc, kHotkeyInfo[index].description, -1, &description,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
    SelectObject(dc, body_font_);
    SetTextColor(dc, theme_colorref(theme.violet));
    RECT gesture_heading{bounds.left, bounds.top + 236, bounds.right, bounds.top + 255};
    DrawTextW(dc, L"GESTOS DEL LAPIZ · TECLA + ARRASTRE", -1, &gesture_heading,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SelectObject(dc, small_font_);
    constexpr std::array<HotkeyInfo, 5> gestures{{
        {L"Shift", L"Linea"},
        {L"Ctrl", L"Rectangulo"},
        {L"Tab", L"Elipse"},
        {L"Ctrl + Shift", L"Flecha"},
        {L"Shift + Tab", L"Flecha curva Bezier"}
    }};
    constexpr std::array<int, 5> gesture_x{0, 170, 337, 0, 267};
    constexpr std::array<int, 5> gesture_y{257, 257, 257, 279, 279};
    constexpr std::array<int, 5> gesture_width{160, 157, 175, 257, 249};
    constexpr std::array<int, 5> gesture_key_width{46, 46, 42, 86, 86};
    for (std::size_t index = 0; index < gestures.size(); ++index) {
        const int x = bounds.left + gesture_x[index];
        const int y = bounds.top + gesture_y[index];
        SetTextColor(dc, theme_colorref(theme.text));
        RECT key{x, y, x + gesture_key_width[index], y + 18};
        DrawTextW(dc, gestures[index].title, -1, &key,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        SetTextColor(dc, theme_colorref(theme.text_soft));
        RECT description{x + gesture_key_width[index] + 8, y,
                         x + gesture_width[index], y + 18};
        DrawTextW(dc, gestures[index].description, -1, &description,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
    SelectObject(dc, body_font_);
    SetTextColor(dc, theme_colorref(theme.violet));
    RECT context_heading{bounds.left, bounds.top + 303, bounds.right, bounds.top + 322};
    DrawTextW(dc, L"GUIA RAPIDA DEL ZOOM", -1, &context_heading,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SelectObject(dc, small_font_);
    constexpr std::array<HotkeyInfo, 6> contextual{{
        {L"P / clic", L"Congelar o reanudar · Rueda / + / - ampliar"},
        {L"E", L"Zoom editable · Mano usa la app · Lapiz anota"},
        {L"F / L / D", L"Vistas · I invertir · 0 vista general"},
        {L"Shift+rueda / [ ]", L"Cambiar diámetro de la lente circular"},
        {L"Espacio / M", L"Recorrer vistas · Esc / F4 / clic der. salir"},
        {L"Texto", L"Ctrl+Enter insertar · Ctrl+V pegar · Esc cancelar"}
    }};
    for (std::size_t index = 0; index < contextual.size(); ++index) {
        const int y = bounds.top + 322 + static_cast<int>(index) * 14;
        SetTextColor(dc, theme_colorref(theme.text));
        RECT key{bounds.left, y, bounds.left + 92, y + 16};
        DrawTextW(dc, contextual[index].title, -1, &key,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        SetTextColor(dc, theme_colorref(theme.text_soft));
        RECT description{bounds.left + 96, y, bounds.right, y + 16};
        DrawTextW(dc, contextual[index].description, -1, &description,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
    SelectObject(dc, previous_font);
}

void SettingsWindow::paint_help(HDC dc, RECT bounds) {
    const auto& theme = current_ui_theme();
    FillRect(dc, &bounds, card_brush_);
    SetBkMode(dc, TRANSPARENT);
    HGDIOBJ previous_font = SelectObject(dc, small_font_);

    RECT badge{bounds.left + 4, bounds.top + 4, bounds.left + 154, bounds.top + 29};
    HBRUSH badge_brush = CreateSolidBrush(theme_colorref(
        theme.light ? 0xE5E0FA : 0x29233F));
    HPEN badge_pen = CreatePen(PS_SOLID, 1, theme_colorref(theme.violet));
    HGDIOBJ previous_brush = SelectObject(dc, badge_brush);
    HGDIOBJ previous_pen = SelectObject(dc, badge_pen);
    RoundRect(dc, badge.left, badge.top, badge.right, badge.bottom, 12, 12);
    SelectObject(dc, previous_pen);
    SelectObject(dc, previous_brush);
    DeleteObject(badge_pen);
    DeleteObject(badge_brush);
    SetTextColor(dc, theme_colorref(theme.violet_strong));
    DrawTextW(dc, L"ACCESO ANTICIPADO", -1, &badge,
              DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    SelectObject(dc, title_font_);
    SetTextColor(dc, theme_colorref(theme.text));
    RECT product{bounds.left, bounds.top + 42, bounds.right, bounds.top + 74};
    DrawTextW(dc, L"Elite Pen", -1, &product, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SelectObject(dc, small_font_);
    SetTextColor(dc, theme_colorref(theme.text_muted));
    RECT version{bounds.left, bounds.top + 73, bounds.right, bounds.top + 94};
    DrawTextW(dc, L"Version 2.9.0 · Windows 10 y 11 · x64", -1, &version,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    SelectObject(dc, body_font_);
    SetTextColor(dc, theme_colorref(theme.text_soft));
    RECT description{bounds.left, bounds.top + 104, bounds.right - 8, bounds.top + 168};
    DrawTextW(dc,
        L"Anota, explica y amplía sobre cualquier aplicación. Incluye tinta, texto, "
        L"figuras, pizarras, captura y zoom congelable sin interrumpir tu presentación.",
        -1, &description, DT_LEFT | DT_WORDBREAK);

    SelectObject(dc, small_font_);
    constexpr std::array<const wchar_t*, 3> capabilities{{
        L"• Paleta de pintor y barra lineal con controles rápidos",
        L"• Zoom vivo, congelado y editable con anotaciones ancladas",
        L"• Edición portable o instalable; preferencias guardadas localmente"
    }};
    for (std::size_t index = 0; index < capabilities.size(); ++index) {
        SetTextColor(dc, theme_colorref(index == 1 ? theme.mint : theme.text));
        RECT line{bounds.left, bounds.top + 178 + static_cast<int>(index) * 23,
                  bounds.right - 8, bounds.top + 199 + static_cast<int>(index) * 23};
        DrawTextW(dc, capabilities[index], -1, &line,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }

    HPEN divider = CreatePen(PS_SOLID, 1, theme_colorref(theme.line));
    previous_pen = SelectObject(dc, divider);
    MoveToEx(dc, bounds.left, bounds.top + 253, nullptr);
    LineTo(dc, bounds.right - 8, bounds.top + 253);
    SelectObject(dc, previous_pen);
    DeleteObject(divider);

    SetTextColor(dc, theme_colorref(theme.violet));
    RECT maker_label{bounds.left, bounds.top + 263, bounds.right, bounds.top + 283};
    DrawTextW(dc, L"DESARROLLADO POR", -1, &maker_label,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SelectObject(dc, body_font_);
    SetTextColor(dc, theme_colorref(theme.text));
    RECT maker{bounds.left, bounds.top + 286, bounds.right, bounds.top + 310};
    DrawTextW(dc, L"Power Elite Studio", -1, &maker,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SelectObject(dc, small_font_);
    SetTextColor(dc, theme_colorref(theme.text_muted));
    RECT privacy{bounds.left, bounds.top + 311, bounds.right, bounds.top + 332};
    DrawTextW(dc, L"Sin cuenta · Sin telemetría · Código abierto Apache 2.0", -1, &privacy,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    RECT website_label{bounds.left, bounds.top + 326, bounds.right, bounds.top + 344};
    DrawTextW(dc, L"Sitio oficial y repositorio público", -1, &website_label,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SelectObject(dc, previous_font);
}

void SettingsWindow::show_tab(int tab) {
    active_tab_ = std::clamp(tab, 0, 2);
    const int general_visibility = active_tab_ == 0 ? SW_SHOW : SW_HIDE;
    for (HWND control : {capture_, confirm_clear_, start_interact_, highlight_cursor_,
                         fade_label_, fade_, control_mode_label_, control_mode_,
                         palette_size_label_, palette_size_,
                         palette_size_hint_, thickness_label_, thickness_, zoom_label_, zoom_,
                         zoom_view_label_, zoom_view_, zoom_invert_, zoom_lens_size_label_,
                         zoom_lens_size_, theme_label_, theme_dark_,
                         theme_light_, reset_position_}) {
        ShowWindow(control, general_visibility);
    }
    ShowWindow(shortcuts_, active_tab_ == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(reset_hotkeys_, active_tab_ == 1 ? SW_SHOW : SW_HIDE);
    ShowWindow(help_, active_tab_ == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(help_website_, active_tab_ == 2 ? SW_SHOW : SW_HIDE);
    ShowWindow(help_source_, active_tab_ == 2 ? SW_SHOW : SW_HIDE);
    refresh_shortcut_rows();
    InvalidateRect(tab_general_, nullptr, TRUE);
    InvalidateRect(tab_shortcuts_, nullptr, TRUE);
    InvalidateRect(tab_help_, nullptr, TRUE);
    InvalidateRect(window_, nullptr, TRUE);
}

void SettingsWindow::refresh_controls() {
    const auto& preferences = controller_.preferences();
    SendMessageW(capture_, BM_SETCHECK,
                 preferences.exclude_palette_from_capture ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(confirm_clear_, BM_SETCHECK,
                 preferences.confirm_clear ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(start_interact_, BM_SETCHECK,
                 preferences.start_in_interact_mode ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(highlight_cursor_, BM_SETCHECK,
                 preferences.highlight_cursor ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(palette_size_, CB_SETCURSEL,
                 static_cast<WPARAM>(std::clamp(preferences.palette_size, 0, 3)), 0);
    SendMessageW(control_mode_, CB_SETCURSEL,
                 static_cast<WPARAM>(preferences.control_mode), 0);
    constexpr std::array<int, 4> fade_values{0, 3, 8, 15};
    std::size_t fade_selection = 0;
    for (std::size_t index = 0; index < fade_values.size(); ++index) {
        if (preferences.fade_seconds == fade_values[index]) fade_selection = index;
    }
    SendMessageW(fade_, CB_SETCURSEL, static_cast<WPARAM>(fade_selection), 0);
    constexpr std::array<float, 5> thickness_values{2.0F, 4.0F, 7.0F, 12.0F, 20.0F};
    std::size_t thickness_selection = 0;
    float thickness_difference = 100.0F;
    for (std::size_t index = 0; index < thickness_values.size(); ++index) {
        const float current = std::abs(controller_.state().thickness - thickness_values[index]);
        if (current < thickness_difference) {
            thickness_difference = current;
            thickness_selection = index;
        }
    }
    SendMessageW(thickness_, CB_SETCURSEL, static_cast<WPARAM>(thickness_selection), 0);
    constexpr std::array<float, 6> factors{1.5F, 2.0F, 3.0F, 4.0F, 6.0F, 8.0F};
    std::size_t closest = 0;
    float difference = 100.0F;
    for (std::size_t index = 0; index < factors.size(); ++index) {
        const float current = std::abs(controller_.state().zoom_factor - factors[index]);
        if (current < difference) { difference = current; closest = index; }
    }
    SendMessageW(zoom_, CB_SETCURSEL, closest, 0);
    SendMessageW(zoom_view_, CB_SETCURSEL,
                 static_cast<WPARAM>(std::clamp(preferences.zoom_view, 0, 2)), 0);
    std::size_t lens_size_selection = 0;
    int lens_size_difference = std::numeric_limits<int>::max();
    for (std::size_t index = 0; index < kZoomLensDiameters.size(); ++index) {
        const int current = std::abs(
            preferences.zoom_lens_diameter - kZoomLensDiameters[index]);
        if (current < lens_size_difference) {
            lens_size_difference = current;
            lens_size_selection = index;
        }
    }
    SendMessageW(zoom_lens_size_, CB_SETCURSEL,
                 static_cast<WPARAM>(lens_size_selection), 0);
    SendMessageW(zoom_invert_, BM_SETCHECK,
                 preferences.zoom_invert ? BST_CHECKED : BST_UNCHECKED, 0);
    InvalidateRect(theme_dark_, nullptr, TRUE);
    InvalidateRect(theme_light_, nullptr, TRUE);
    cancel_hotkey_capture();
    refresh_shortcut_rows();
}

void SettingsWindow::refresh_shortcut_rows() {
    const std::size_t maximum_offset = kHotkeyActionCount > kVisibleShortcutRows
        ? kHotkeyActionCount - kVisibleShortcutRows : 0;
    shortcut_scroll_offset_ = std::min(shortcut_scroll_offset_, maximum_offset);
    const bool visible = active_tab_ == 1;
    for (std::size_t slot = 0; slot < kVisibleShortcutRows; ++slot) {
        const std::size_t index = shortcut_scroll_offset_ + slot;
        const bool row_visible = visible && index < kHotkeyActionCount;
        if (row_visible) {
            const std::wstring label = capturing_hotkey_ == static_cast<int>(index)
                ? L"Pulsa combinacion..."
                : hotkey_text(controller_.preferences().hotkeys[index]);
            SetWindowTextW(hotkey_buttons_[slot], label.c_str());
        }
        ShowWindow(hotkey_buttons_[slot], row_visible ? SW_SHOW : SW_HIDE);
        ShowWindow(hotkey_edit_buttons_[slot], row_visible ? SW_SHOW : SW_HIDE);
        InvalidateRect(hotkey_buttons_[slot], nullptr, TRUE);
        InvalidateRect(hotkey_edit_buttons_[slot], nullptr, TRUE);
    }
    ShowWindow(shortcut_scrollbar_, visible ? SW_SHOW : SW_HIDE);
    SCROLLINFO information{};
    information.cbSize = sizeof(information);
    information.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    information.nMin = 0;
    information.nMax = static_cast<int>(kHotkeyActionCount) - 1;
    information.nPage = static_cast<UINT>(kVisibleShortcutRows);
    information.nPos = static_cast<int>(shortcut_scroll_offset_);
    SetScrollInfo(shortcut_scrollbar_, SB_CTL, &information, TRUE);
    if (shortcuts_) InvalidateRect(shortcuts_, nullptr, TRUE);
}

void SettingsWindow::begin_hotkey_capture(std::size_t index) {
    if (index >= kHotkeyActionCount) return;
    cancel_hotkey_capture();
    capturing_hotkey_ = static_cast<int>(index);
    refresh_shortcut_rows();
    SetFocus(window_);
}

void SettingsWindow::cancel_hotkey_capture() {
    if (capturing_hotkey_ < 0) return;
    capturing_hotkey_ = -1;
    refresh_shortcut_rows();
}

void SettingsWindow::finish_hotkey_capture(UINT virtual_key) {
    if (capturing_hotkey_ < 0) return;
    if (virtual_key == VK_ESCAPE) {
        cancel_hotkey_capture();
        return;
    }
    if (virtual_key == VK_CONTROL || virtual_key == VK_LCONTROL || virtual_key == VK_RCONTROL ||
        virtual_key == VK_SHIFT || virtual_key == VK_LSHIFT || virtual_key == VK_RSHIFT ||
        virtual_key == VK_MENU || virtual_key == VK_LMENU || virtual_key == VK_RMENU ||
        virtual_key == VK_LWIN || virtual_key == VK_RWIN) return;
    UINT modifiers = 0;
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) modifiers |= MOD_CONTROL;
    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) modifiers |= MOD_SHIFT;
    if ((GetKeyState(VK_MENU) & 0x8000) != 0) modifiers |= MOD_ALT;
    if ((GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0)
        modifiers |= MOD_WIN;
    const std::size_t capture_index = static_cast<std::size_t>(capturing_hotkey_);
    if (virtual_key == VK_DELETE || virtual_key == VK_BACK) {
        if (!controller_.set_hotkey_binding(
                static_cast<HotkeyAction>(capture_index), HotkeyBinding{})) {
            MessageBoxW(window_, L"No se pudo quitar este atajo.", L"Elite Pen",
                        MB_OK | MB_ICONWARNING);
            return;
        }
        capturing_hotkey_ = -1;
        refresh_shortcut_rows();
        return;
    }
    const bool function_key = virtual_key >= VK_F1 && virtual_key <= VK_F24;
    const bool global = capture_index < kGlobalHotkeyActionCount;
    if (global && modifiers == 0 && !function_key) {
        MessageBoxW(window_, L"Usa Ctrl, Alt, Shift o Win junto a la tecla. "
                    L"Las teclas F1-F24 tambien pueden usarse solas. "
                    L"Pulsa Supr o Retroceso para dejarlo sin asignar.",
                    L"Atajo seguro — Elite Pen", MB_OK | MB_ICONINFORMATION);
        return;
    }
    const auto action = static_cast<HotkeyAction>(capture_index);
    const HotkeyBinding binding{modifiers, virtual_key};
    if (!controller_.set_hotkey_binding(action, binding)) {
        MessageBoxW(window_, L"La combinación ya está en uso por Elite Pen o por Windows. "
                    L"Prueba otra.", L"Atajo no disponible — Elite Pen",
                    MB_OK | MB_ICONWARNING);
        return;
    }
    capturing_hotkey_ = -1;
    refresh_shortcut_rows();
}

void SettingsWindow::show_settings() {
    refresh_controls();
    POINT cursor{};
    GetCursorPos(&cursor);
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    const int x = info.rcWork.left + (info.rcWork.right - info.rcWork.left - 590) / 2;
    const int y = info.rcWork.top + (info.rcWork.bottom - info.rcWork.top - 590) / 2;
    SetWindowPos(window_, HWND_TOPMOST, x, y, 590, 590, SWP_SHOWWINDOW);
    SetWindowRgn(window_, CreateRoundRectRgn(0, 0, 591, 591, 22, 22), TRUE);
    InvalidateRect(window_, nullptr, TRUE);
    UpdateWindow(window_);
    SetForegroundWindow(window_);
    SetFocus(active_tab_ == 0 ? capture_ :
             (active_tab_ == 1 ? tab_shortcuts_ : help_website_));
}

LRESULT SettingsWindow::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window_, &paint);
            paint_background(dc);
            EndPaint(window_, &paint);
            return 0;
        }
        case WM_COMMAND: {
            const int id = LOWORD(wparam);
            if (id == 4101 && HIWORD(wparam) == BN_CLICKED) {
                show_tab(0);
                return 0;
            }
            if (id == 4102 && HIWORD(wparam) == BN_CLICKED) {
                show_tab(1);
                return 0;
            }
            if (id == 4104 && HIWORD(wparam) == BN_CLICKED) {
                show_tab(2);
                return 0;
            }
            if (id == 4500 && HIWORD(wparam) == BN_CLICKED) {
                const HINSTANCE result = ShellExecuteW(
                    window_, L"open", L"https://powerelite.studio/", nullptr, nullptr,
                    SW_SHOWNORMAL);
                if (reinterpret_cast<INT_PTR>(result) <= 32) {
                    MessageBoxW(window_, L"No se pudo abrir el navegador. Visita "
                                L"https://powerelite.studio/", L"Elite Pen",
                                MB_OK | MB_ICONINFORMATION);
                }
                return 0;
            }
            if (id == 4501 && HIWORD(wparam) == BN_CLICKED) {
                const HINSTANCE result = ShellExecuteW(
                    window_, L"open", L"https://github.com/powerelitestudio/elite-pen",
                    nullptr, nullptr, SW_SHOWNORMAL);
                if (reinterpret_cast<INT_PTR>(result) <= 32) {
                    MessageBoxW(window_, L"No se pudo abrir el navegador. Visita "
                                L"https://github.com/powerelitestudio/elite-pen",
                                L"Elite Pen", MB_OK | MB_ICONINFORMATION);
                }
                return 0;
            }
            if (id >= 4400 && id < 4400 + static_cast<int>(kVisibleShortcutRows) &&
                HIWORD(wparam) == BN_CLICKED) {
                begin_hotkey_capture(shortcut_scroll_offset_ +
                    static_cast<std::size_t>(id - 4400));
                return 0;
            }
            if (id == 4300 && HIWORD(wparam) == BN_CLICKED) {
                if (!controller_.reset_hotkeys()) {
                    MessageBoxW(window_, L"Windows no permitió registrar uno de los atajos "
                                L"predeterminados.", L"Elite Pen", MB_OK | MB_ICONWARNING);
                }
                refresh_controls();
                return 0;
            }
            if (id == 4001 && HIWORD(wparam) == BN_CLICKED) {
                SendMessageW(capture_, BM_SETCHECK,
                    SendMessageW(capture_, BM_GETCHECK, 0, 0) == BST_CHECKED
                        ? BST_UNCHECKED : BST_CHECKED, 0);
                controller_.preferences().exclude_palette_from_capture =
                    SendMessageW(capture_, BM_GETCHECK, 0, 0) == BST_CHECKED;
                controller_.apply_capture_preference();
                controller_.save_preferences();
                return 0;
            }
            if (id == 4002 && HIWORD(wparam) == BN_CLICKED) {
                SendMessageW(confirm_clear_, BM_SETCHECK,
                    SendMessageW(confirm_clear_, BM_GETCHECK, 0, 0) == BST_CHECKED
                        ? BST_UNCHECKED : BST_CHECKED, 0);
                controller_.preferences().confirm_clear =
                    SendMessageW(confirm_clear_, BM_GETCHECK, 0, 0) == BST_CHECKED;
                controller_.save_preferences();
                return 0;
            }
            if (id == 4003 && HIWORD(wparam) == BN_CLICKED) {
                SendMessageW(start_interact_, BM_SETCHECK,
                    SendMessageW(start_interact_, BM_GETCHECK, 0, 0) == BST_CHECKED
                        ? BST_UNCHECKED : BST_CHECKED, 0);
                controller_.preferences().start_in_interact_mode =
                    SendMessageW(start_interact_, BM_GETCHECK, 0, 0) == BST_CHECKED;
                controller_.save_preferences();
                return 0;
            }
            if (id == 4008 && HIWORD(wparam) == BN_CLICKED) {
                SendMessageW(highlight_cursor_, BM_SETCHECK,
                    SendMessageW(highlight_cursor_, BM_GETCHECK, 0, 0) == BST_CHECKED
                        ? BST_UNCHECKED : BST_CHECKED, 0);
                controller_.preferences().highlight_cursor =
                    SendMessageW(highlight_cursor_, BM_GETCHECK, 0, 0) == BST_CHECKED;
                controller_.state().cursor_highlight =
                    controller_.preferences().highlight_cursor;
                controller_.invalidate_document();
                controller_.save_preferences();
                return 0;
            }
            if (id == 4009 && HIWORD(wparam) == CBN_SELCHANGE) {
                constexpr std::array<int, 4> fade_values{0, 3, 8, 15};
                const LRESULT selection = SendMessageW(fade_, CB_GETCURSEL, 0, 0);
                if (selection >= 0 && static_cast<std::size_t>(selection) < fade_values.size()) {
                    controller_.preferences().fade_seconds =
                        fade_values[static_cast<std::size_t>(selection)];
                    controller_.save_preferences();
                }
                return 0;
            }
            if (id == 4010 && HIWORD(wparam) == CBN_SELCHANGE) {
                constexpr std::array<float, 5> thickness_values{
                    2.0F, 4.0F, 7.0F, 12.0F, 20.0F};
                const LRESULT selection = SendMessageW(thickness_, CB_GETCURSEL, 0, 0);
                if (selection >= 0 &&
                    static_cast<std::size_t>(selection) < thickness_values.size()) {
                    controller_.set_thickness(
                        thickness_values[static_cast<std::size_t>(selection)]);
                    controller_.save_preferences();
                }
                return 0;
            }
            if (id == 4011 && HIWORD(wparam) == CBN_SELCHANGE) {
                const LRESULT selection = SendMessageW(palette_size_, CB_GETCURSEL, 0, 0);
                if (selection >= 0 && selection < static_cast<LRESULT>(kPaletteScales.size())) {
                    controller_.set_palette_size(static_cast<int>(selection));
                }
                return 0;
            }
            if (id == 4014 && HIWORD(wparam) == CBN_SELCHANGE) {
                const LRESULT selection = SendMessageW(control_mode_, CB_GETCURSEL, 0, 0);
                if (selection == 0 || selection == 1) {
                    controller_.set_control_mode(selection == 1
                        ? ControlMode::Linear : ControlMode::Palette);
                }
                return 0;
            }
            if ((id == 4012 || id == 4013) && HIWORD(wparam) == BN_CLICKED) {
                controller_.set_theme(id == 4013 ? AppTheme::Light : AppTheme::Dark);
                refresh_controls();
                return 0;
            }
            if (id == 4004 && HIWORD(wparam) == CBN_SELCHANGE) {
                constexpr std::array<float, 6> factors{1.5F, 2.0F, 3.0F, 4.0F, 6.0F, 8.0F};
                const LRESULT selection = SendMessageW(zoom_, CB_GETCURSEL, 0, 0);
                if (selection >= 0 && static_cast<std::size_t>(selection) < factors.size()) {
                    controller_.state().zoom_factor = factors[static_cast<std::size_t>(selection)];
                    controller_.preferences().zoom_factor = controller_.state().zoom_factor;
                    controller_.save_preferences();
                }
                return 0;
            }
            if (id == 4005 && HIWORD(wparam) == BN_CLICKED) {
                controller_.reset_palette_position();
                return 0;
            }
            if (id == 4006 && HIWORD(wparam) == CBN_SELCHANGE) {
                const LRESULT selection = SendMessageW(zoom_view_, CB_GETCURSEL, 0, 0);
                if (selection >= 0 && selection <= 2) {
                    controller_.preferences().zoom_view = static_cast<int>(selection);
                    controller_.save_preferences();
                }
                return 0;
            }
            if (id == 4015 && HIWORD(wparam) == CBN_SELCHANGE) {
                const LRESULT selection = SendMessageW(
                    zoom_lens_size_, CB_GETCURSEL, 0, 0);
                if (selection >= 0 &&
                    static_cast<std::size_t>(selection) < kZoomLensDiameters.size()) {
                    controller_.set_zoom_lens_diameter(
                        kZoomLensDiameters[static_cast<std::size_t>(selection)]);
                }
                return 0;
            }
            if (id == 4007 && HIWORD(wparam) == BN_CLICKED) {
                SendMessageW(zoom_invert_, BM_SETCHECK,
                    SendMessageW(zoom_invert_, BM_GETCHECK, 0, 0) == BST_CHECKED
                        ? BST_UNCHECKED : BST_CHECKED, 0);
                controller_.preferences().zoom_invert =
                    SendMessageW(zoom_invert_, BM_GETCHECK, 0, 0) == BST_CHECKED;
                controller_.save_preferences();
                return 0;
            }
            if (id == IDOK) {
                ShowWindow(window_, SW_HIDE);
                return 0;
            }
            if (id == IDCANCEL) {
                ShowWindow(window_, SW_HIDE);
                return 0;
            }
            break;
        }
        case WM_VSCROLL:
            if (reinterpret_cast<HWND>(lparam) == shortcut_scrollbar_) {
                const std::size_t maximum_offset = kHotkeyActionCount - kVisibleShortcutRows;
                int next = static_cast<int>(shortcut_scroll_offset_);
                switch (LOWORD(wparam)) {
                    case SB_LINEUP: --next; break;
                    case SB_LINEDOWN: ++next; break;
                    case SB_PAGEUP: next -= static_cast<int>(kVisibleShortcutRows); break;
                    case SB_PAGEDOWN: next += static_cast<int>(kVisibleShortcutRows); break;
                    case SB_THUMBPOSITION:
                    case SB_THUMBTRACK: next = HIWORD(wparam); break;
                    case SB_TOP: next = 0; break;
                    case SB_BOTTOM: next = static_cast<int>(maximum_offset); break;
                    default: return 0;
                }
                cancel_hotkey_capture();
                shortcut_scroll_offset_ = static_cast<std::size_t>(std::clamp(
                    next, 0, static_cast<int>(maximum_offset)));
                refresh_shortcut_rows();
                return 0;
            }
            break;
        case WM_MOUSEWHEEL:
            if (active_tab_ == 1) {
                const int direction = GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? -2 : 2;
                const int maximum_offset = static_cast<int>(
                    kHotkeyActionCount - kVisibleShortcutRows);
                cancel_hotkey_capture();
                shortcut_scroll_offset_ = static_cast<std::size_t>(std::clamp(
                    static_cast<int>(shortcut_scroll_offset_) + direction,
                    0, maximum_offset));
                refresh_shortcut_rows();
                return 0;
            }
            break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (capturing_hotkey_ >= 0) {
                finish_hotkey_capture(static_cast<UINT>(wparam));
                return 0;
            }
            if (wparam == VK_ESCAPE) {
                ShowWindow(window_, SW_HIDE);
                return 0;
            }
            break;
        case WM_DRAWITEM: {
            const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lparam);
            if (!item) break;
            const auto& theme = current_ui_theme();
            if (item->CtlType == ODT_STATIC && item->hwndItem == shortcuts_) {
                RECT bounds = item->rcItem;
                paint_shortcuts(item->hDC, bounds);
                return TRUE;
            }
            if (item->CtlType == ODT_STATIC && item->hwndItem == help_) {
                RECT bounds = item->rcItem;
                paint_help(item->hDC, bounds);
                return TRUE;
            }
            if (item->CtlType != ODT_BUTTON) break;
            const int id = static_cast<int>(item->CtlID);
            const bool checkbox = id == 4001 || id == 4002 || id == 4003 ||
                                  id == 4007 || id == 4008;
            const bool pressed = (item->itemState & ODS_SELECTED) != 0;
            const bool focused = (item->itemState & ODS_FOCUS) != 0;
            SetBkMode(item->hDC, TRANSPARENT);
            if (id >= 4400 && id < 4400 + static_cast<int>(kVisibleShortcutRows)) {
                FillRect(item->hDC, &item->rcItem, card_brush_);
                HBRUSH face = CreateSolidBrush(theme_colorref(
                    pressed ? theme.surface_4 : theme.surface_3));
                HPEN outline = CreatePen(PS_SOLID, focused ? 2 : 1,
                    theme_colorref(focused ? theme.violet : theme.line));
                HGDIOBJ previous_brush = SelectObject(item->hDC, face);
                HGDIOBJ previous_pen = SelectObject(item->hDC, outline);
                RoundRect(item->hDC, item->rcItem.left, item->rcItem.top,
                          item->rcItem.right, item->rcItem.bottom, 10, 10);
                SelectObject(item->hDC, previous_pen);
                SelectObject(item->hDC, previous_brush);
                DeleteObject(outline);
                DeleteObject(face);
                const int cx = (item->rcItem.left + item->rcItem.right) / 2;
                const int cy = (item->rcItem.top + item->rcItem.bottom) / 2;
                HPEN pencil = CreatePen(PS_SOLID, 2, theme_colorref(theme.violet));
                previous_pen = SelectObject(item->hDC, pencil);
                MoveToEx(item->hDC, cx - 5, cy + 5, nullptr);
                LineTo(item->hDC, cx + 5, cy - 5);
                MoveToEx(item->hDC, cx - 6, cy + 6, nullptr);
                LineTo(item->hDC, cx - 2, cy + 5);
                MoveToEx(item->hDC, cx + 3, cy - 6, nullptr);
                LineTo(item->hDC, cx + 6, cy - 3);
                SelectObject(item->hDC, previous_pen);
                DeleteObject(pencil);
                return TRUE;
            }
            if (id == 4101 || id == 4102 || id == 4104) {
                const bool active = (id == 4101 && active_tab_ == 0) ||
                                    (id == 4102 && active_tab_ == 1) ||
                                    (id == 4104 && active_tab_ == 2);
                FillRect(item->hDC, &item->rcItem, background_brush_);
                HBRUSH face = CreateSolidBrush(theme_colorref(
                    active ? (theme.light ? 0xE5E0FA : 0x29233F) : theme.surface_2));
                HPEN outline = CreatePen(PS_SOLID, active || focused ? 2 : 1,
                    theme_colorref(active || focused ? theme.violet : theme.line));
                HGDIOBJ previous_brush = SelectObject(item->hDC, face);
                HGDIOBJ previous_pen = SelectObject(item->hDC, outline);
                RoundRect(item->hDC, item->rcItem.left, item->rcItem.top,
                          item->rcItem.right, item->rcItem.bottom, 11, 11);
                SelectObject(item->hDC, previous_pen);
                SelectObject(item->hDC, previous_brush);
                DeleteObject(outline);
                DeleteObject(face);
                wchar_t value[64]{};
                GetWindowTextW(item->hwndItem, value, 64);
                SetTextColor(item->hDC, theme_colorref(
                    active ? theme.violet_strong : theme.text_soft));
                RECT text_rect = item->rcItem;
                DrawTextW(item->hDC, value, -1, &text_rect,
                          DT_CENTER | DT_SINGLELINE | DT_VCENTER);
                return TRUE;
            }
            if (id == 4012 || id == 4013) {
                const bool active = (id == 4012 && controller_.preferences().theme == AppTheme::Dark) ||
                                    (id == 4013 && controller_.preferences().theme == AppTheme::Light);
                FillRect(item->hDC, &item->rcItem, card_brush_);
                HBRUSH face = CreateSolidBrush(theme_colorref(
                    active ? (theme.light ? 0xE5E0FA : 0x29233F) : theme.surface_3));
                HPEN outline = CreatePen(PS_SOLID, active || focused ? 2 : 1,
                    theme_colorref(active || focused ? theme.violet : theme.line));
                HGDIOBJ previous_brush = SelectObject(item->hDC, face);
                HGDIOBJ previous_pen = SelectObject(item->hDC, outline);
                RoundRect(item->hDC, item->rcItem.left, item->rcItem.top,
                          item->rcItem.right, item->rcItem.bottom, 11, 11);
                SelectObject(item->hDC, previous_pen);
                SelectObject(item->hDC, previous_brush);
                DeleteObject(outline);
                DeleteObject(face);
                const int cx = item->rcItem.left + 18;
                const int cy = (item->rcItem.top + item->rcItem.bottom) / 2;
                HPEN glyph = CreatePen(PS_SOLID, 2, theme_colorref(
                    active ? theme.violet : theme.text_soft));
                previous_pen = SelectObject(item->hDC, glyph);
                previous_brush = SelectObject(item->hDC, GetStockObject(NULL_BRUSH));
                if (id == 4012) {
                    Arc(item->hDC, cx - 6, cy - 7, cx + 7, cy + 7,
                        cx + 5, cy - 5, cx + 5, cy + 5);
                } else {
                    Ellipse(item->hDC, cx - 5, cy - 5, cx + 5, cy + 5);
                    for (int ray = 0; ray < 8; ++ray) {
                        const double angle = static_cast<double>(ray) * 0.78539816339;
                        MoveToEx(item->hDC, cx + static_cast<int>(std::cos(angle) * 7.0),
                                 cy + static_cast<int>(std::sin(angle) * 7.0), nullptr);
                        LineTo(item->hDC, cx + static_cast<int>(std::cos(angle) * 9.0),
                               cy + static_cast<int>(std::sin(angle) * 9.0));
                    }
                }
                SelectObject(item->hDC, previous_brush);
                SelectObject(item->hDC, previous_pen);
                DeleteObject(glyph);
                wchar_t value[32]{};
                GetWindowTextW(item->hwndItem, value, 32);
                SetTextColor(item->hDC, theme_colorref(
                    active ? theme.violet_strong : theme.text_soft));
                RECT text_rect{item->rcItem.left + 34, item->rcItem.top,
                               item->rcItem.right - 8, item->rcItem.bottom};
                DrawTextW(item->hDC, value, -1, &text_rect,
                          DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                return TRUE;
            }
            if (checkbox) {
                FillRect(item->hDC, &item->rcItem, card_brush_);
                const int top = (item->rcItem.bottom - 18) / 2;
                RECT box{1, top, 19, top + 18};
                const bool checked = SendMessageW(item->hwndItem, BM_GETCHECK, 0, 0) == BST_CHECKED;
                HBRUSH fill = CreateSolidBrush(theme_colorref(
                    checked ? theme.violet : theme.surface_1));
                HPEN outline = CreatePen(PS_SOLID, focused ? 2 : 1,
                    theme_colorref(focused || checked ? theme.violet : theme.line));
                HGDIOBJ previous_brush = SelectObject(item->hDC, fill);
                HGDIOBJ previous_pen = SelectObject(item->hDC, outline);
                RoundRect(item->hDC, box.left, box.top, box.right, box.bottom, 6, 6);
                SelectObject(item->hDC, previous_pen);
                SelectObject(item->hDC, previous_brush);
                DeleteObject(outline);
                DeleteObject(fill);
                if (checked) {
                    HPEN check_pen = CreatePen(PS_SOLID, 2, theme_colorref(0xFFFFFF));
                    previous_pen = SelectObject(item->hDC, check_pen);
                    MoveToEx(item->hDC, 5, top + 9, nullptr);
                    LineTo(item->hDC, 9, top + 13);
                    LineTo(item->hDC, 16, top + 5);
                    SelectObject(item->hDC, previous_pen);
                    DeleteObject(check_pen);
                }
                wchar_t value[256]{};
                GetWindowTextW(item->hwndItem, value, 256);
                RECT text_rect{29, 0, item->rcItem.right, item->rcItem.bottom};
                SetTextColor(item->hDC, theme_colorref(theme.text));
                DrawTextW(item->hDC, value, -1, &text_rect,
                          DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                return TRUE;
            }
            const bool chrome = id == IDCANCEL;
            const bool primary = id == IDOK;
            FillRect(item->hDC, &item->rcItem, chrome || primary ? background_brush_ : card_brush_);
            RECT button_rect = item->rcItem;
            if (chrome) InflateRect(&button_rect, -4, -4);
            const COLORREF face_color = primary
                ? theme_colorref(theme.violet)
                : theme_colorref(pressed ? theme.surface_4 : theme.surface_3);
            HBRUSH face = CreateSolidBrush(face_color);
            HPEN outline = CreatePen(PS_SOLID, focused ? 2 : 1,
                theme_colorref(primary || focused ? theme.violet_strong : theme.line));
            HGDIOBJ previous_brush = SelectObject(item->hDC, face);
            HGDIOBJ previous_pen = SelectObject(item->hDC, outline);
            RoundRect(item->hDC, button_rect.left, button_rect.top,
                      button_rect.right, button_rect.bottom, chrome ? 12 : 10, chrome ? 12 : 10);
            SelectObject(item->hDC, previous_pen);
            SelectObject(item->hDC, previous_brush);
            DeleteObject(outline);
            DeleteObject(face);
            if (chrome) {
                HPEN close_pen = CreatePen(PS_SOLID, 2, theme_colorref(theme.text_soft));
                previous_pen = SelectObject(item->hDC, close_pen);
                const int middle_x = (button_rect.left + button_rect.right) / 2;
                const int middle_y = (button_rect.top + button_rect.bottom) / 2;
                MoveToEx(item->hDC, middle_x - 4, middle_y - 4, nullptr);
                LineTo(item->hDC, middle_x + 4, middle_y + 4);
                MoveToEx(item->hDC, middle_x + 4, middle_y - 4, nullptr);
                LineTo(item->hDC, middle_x - 4, middle_y + 4);
                SelectObject(item->hDC, previous_pen);
                DeleteObject(close_pen);
            } else {
                wchar_t value[256]{};
                GetWindowTextW(item->hwndItem, value, 256);
                SetTextColor(item->hDC, theme_colorref(primary ? 0xFFFFFF : theme.text));
                RECT text_rect = item->rcItem;
                DrawTextW(item->hDC, value, -1, &text_rect,
                          DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            }
            return TRUE;
        }
        case WM_NCHITTEST: {
            POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ScreenToClient(window_, &point);
            if (point.y >= 0 && point.y < 58 && point.x < 530) return HTCAPTION;
            break;
        }
        case WM_CLOSE:
            cancel_hotkey_capture();
            ShowWindow(window_, SW_HIDE);
            return 0;
        case WM_CTLCOLORSTATIC: {
            const auto& theme = current_ui_theme();
            const HDC dc = reinterpret_cast<HDC>(wparam);
            const HWND control = reinterpret_cast<HWND>(lparam);
            SetBkMode(dc, TRANSPARENT);
            if (control == title_) SetTextColor(dc, theme_colorref(theme.violet));
            else if (control == subtitle_ || control == shortcuts_ ||
                     control == palette_size_hint_)
                SetTextColor(dc, theme_colorref(theme.text_muted));
            else SetTextColor(dc, theme_colorref(theme.text));
            return reinterpret_cast<LRESULT>(
                control == title_ || control == subtitle_ ? background_brush_ : card_brush_);
        }
        case WM_CTLCOLORBTN: {
            const auto& theme = current_ui_theme();
            const HDC dc = reinterpret_cast<HDC>(wparam);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, theme_colorref(theme.text));
            return reinterpret_cast<LRESULT>(
                reinterpret_cast<HWND>(lparam) == close_ ? background_brush_ : card_brush_);
        }
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLOREDIT: {
            const auto& theme = current_ui_theme();
            const HDC dc = reinterpret_cast<HDC>(wparam);
            SetBkColor(dc, theme_colorref(theme.surface_2));
            SetTextColor(dc, theme_colorref(theme.text));
            return reinterpret_cast<LRESULT>(card_brush_);
        }
        case WM_ERASEBKGND: {
            RECT client{};
            GetClientRect(window_, &client);
            FillRect(reinterpret_cast<HDC>(wparam), &client, background_brush_);
            return 1;
        }
        default: break;
    }
    return WindowBase::handle_message(message, wparam, lparam);
}

bool ZoomInkWindow::initialize(GraphicsDevice& graphics) {
    // DirectComposition owns the premultiplied alpha for this full-screen
    // surface. WS_EX_LAYERED can make a transparent swap chain appear as an
    // opaque black rectangle over Magnifier on some Windows 10/11 drivers.
    // NOREDIRECTIONBITMAP is the same proven composition contract used by the
    // palette and keeps both Navigate transparency and frozen snapshots intact.
    constexpr DWORD ex_style = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                               WS_EX_NOREDIRECTIONBITMAP | WS_EX_TRANSPARENT;
    const RECT initial{0, 0, 1, 1};
    if (!create(L"ElitePen.ZoomInk", L"Anotaciones de zoom — Elite Pen",
                 ex_style, WS_POPUP, initial)) return false;
    if (!initialize_surface(graphics)) return false;
    refresh_pencil_cursor();
    // Zoom annotations are presentation content, not private chrome. Keeping
    // this full-screen surface capturable is essential for OBS and other course
    // recorders; excluding it makes Windows emit a solid black rectangle.
    SetWindowDisplayAffinity(window_, WDA_NONE);
    return true;
}

void ZoomInkWindow::refresh_pencil_cursor() {
    HCURSOR next = create_pencil_cursor(GetDpiForWindow(window_));
    if (!next) return;
    if (pencil_cursor_) DestroyCursor(pencil_cursor_);
    pencil_cursor_ = next;
}

void ZoomInkWindow::set_bounds(RECT bounds) {
    bounds_ = bounds;
    SetWindowPos(window_, nullptr, bounds.left, bounds.top,
                 std::max(1L, bounds.right - bounds.left),
                 std::max(1L, bounds.bottom - bounds.top),
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

void ZoomInkWindow::show_live(RECT bounds) {
    edit_mode_ = false;
    edit_annotating_ = false;
    set_bounds(bounds);
    set_frozen(false);
    snapshot_has_content_ = false;
    render();
    ShowWindow(window_, SW_SHOWNOACTIVATE);
}

bool ZoomInkWindow::show_edit(RECT bounds, RECT source, float factor,
                              bool annotating, HWND magnifier) {
    cancel_gesture();
    frozen_ = false;
    snapshot_.Reset();
    snapshot_has_content_ = false;
    edit_mode_ = true;
    set_bounds(bounds);
    update_edit_view(bounds, source, factor);
    if (annotating) {
        UpdateWindow(magnifier);
        DwmFlush();
        if (!capture_snapshot(magnifier, bounds)) {
            set_edit_annotating(false);
            return false;
        }
    }
    set_edit_annotating(annotating);
    if (annotating) {
        render();
        ShowWindow(window_, SW_SHOW);
    } else {
        // MANO is a real interactive magnifier, not an annotation overlay. Keep
        // the source-space document in memory, but remove this composition
        // surface entirely so it cannot obscure the app being magnified.
        ShowWindow(window_, SW_HIDE);
    }
    if (annotating) {
        SetForegroundWindow(window_);
        SetFocus(window_);
    }
    return true;
}

void ZoomInkWindow::update_edit_view(RECT bounds, RECT source, float factor) {
    if (!edit_mode_) return;
    if (!EqualRect(&bounds_, &bounds)) set_bounds(bounds);
    edit_transform_.source = {
        static_cast<float>(source.left), static_cast<float>(source.top),
        static_cast<float>(source.right), static_cast<float>(source.bottom)};
    edit_transform_.scale = std::clamp(factor, 1.0F, 8.0F);
    invalidate();
}

void ZoomInkWindow::set_edit_annotating(bool annotating) {
    if (!edit_mode_) return;
    cancel_gesture();
    edit_annotating_ = annotating;
    if (!annotating) {
        snapshot_.Reset();
        snapshot_has_content_ = false;
        ShowWindow(window_, SW_HIDE);
    }
    LONG_PTR style = GetWindowLongPtrW(window_, GWL_EXSTYLE);
    if (annotating) {
        style &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    } else {
        style |= WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
    }
    SetWindowLongPtrW(window_, GWL_EXSTYLE, style);
    SetWindowPos(window_, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                 SWP_FRAMECHANGED);
    invalidate();
}

void ZoomInkWindow::hide() {
    cancel_gesture();
    frozen_ = false;
    snapshot_.Reset();
    snapshot_has_content_ = false;
    preview_.reset();
    document_ = Document{};
    edit_document_ = Document{};
    document_cache_.reset();
    edit_document_cache_.reset();
    edit_mode_ = false;
    edit_annotating_ = false;
    ShowWindow(window_, SW_HIDE);
}

void ZoomInkWindow::set_frozen(bool frozen) {
    frozen_ = frozen;
    if (frozen) {
        edit_mode_ = false;
        edit_annotating_ = false;
    }
    LONG_PTR style = GetWindowLongPtrW(window_, GWL_EXSTYLE);
    if (frozen_ || (edit_mode_ && edit_annotating_)) {
        style &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    } else {
        cancel_gesture();
        style |= WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
        snapshot_.Reset();
    }
    SetWindowLongPtrW(window_, GWL_EXSTYLE, style);
    SetWindowPos(window_, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                 SWP_FRAMECHANGED);
    invalidate();
}

void ZoomInkWindow::bring_to_front() {
    if (!IsWindowVisible(window_)) return;
    SetWindowPos(window_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

bool ZoomInkWindow::capture_snapshot(HWND magnifier, RECT bounds) {
    const int width = std::max(1L, bounds.right - bounds.left);
    const int height = std::max(1L, bounds.bottom - bounds.top);
    BITMAPINFO information{};
    information.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    information.bmiHeader.biWidth = width;
    information.bmiHeader.biHeight = -height;
    information.bmiHeader.biPlanes = 1;
    information.bmiHeader.biBitCount = 32;
    information.bmiHeader.biCompression = BI_RGB;
    HDC screen = GetDC(nullptr);
    HDC memory = screen ? CreateCompatibleDC(screen) : nullptr;
    void* pixels = nullptr;
    HBITMAP bitmap = screen ? CreateDIBSection(screen, &information, DIB_RGB_COLORS,
                                                &pixels, nullptr, 0) : nullptr;
    if (!screen || !memory || !bitmap || !pixels) {
        if (bitmap) DeleteObject(bitmap);
        if (memory) DeleteDC(memory);
        if (screen) ReleaseDC(nullptr, screen);
        return false;
    }
    const HGDIOBJ previous = SelectObject(memory, bitmap);
    const bool ink_visible = IsWindowVisible(window_) != FALSE;
    const bool palette_visible = controller_.palette() &&
                                 IsWindowVisible(controller_.palette()->hwnd()) != FALSE;
    HWND parent = GetParent(magnifier);
    DWORD previous_affinity = WDA_NONE;
    const bool had_affinity = parent &&
        GetWindowDisplayAffinity(parent, &previous_affinity) != FALSE;
    if (ink_visible) ShowWindow(window_, SW_HIDE);
    if (palette_visible) ShowWindow(controller_.palette()->hwnd(), SW_HIDE);
    if (parent) SetWindowDisplayAffinity(parent, WDA_NONE);
    RedrawWindow(parent, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    DwmFlush();
    wchar_t synthetic_capture[4]{};
    const bool synthetic = GetEnvironmentVariableW(
        L"ELITE_PEN_QA_SYNTHETIC_CAPTURE", synthetic_capture,
        static_cast<DWORD>(std::size(synthetic_capture))) > 0;
    bool captured{};
    if (synthetic) {
        auto* values = static_cast<std::uint32_t*>(pixels);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const std::uint32_t shade = static_cast<std::uint32_t>(
                    0x24 + ((x / 48 + y / 48) % 2) * 0x18);
                values[static_cast<std::size_t>(y) *
                           static_cast<std::size_t>(width) +
                       static_cast<std::size_t>(x)] =
                    0xFF000000U | (shade << 16U) | ((shade + 8U) << 8U) |
                    (shade + 16U);
            }
        }
        captured = true;
    } else {
        captured = BitBlt(memory, 0, 0, width, height, screen,
                          bounds.left, bounds.top, SRCCOPY | CAPTUREBLT) != FALSE;
    }
    if (parent && had_affinity) SetWindowDisplayAffinity(parent, previous_affinity);
    if (ink_visible) ShowWindow(window_, frozen_ ? SW_SHOW : SW_SHOWNOACTIVATE);
    if (palette_visible) ShowWindow(controller_.palette()->hwnd(), SW_SHOWNOACTIVATE);
    DwmFlush();
    if (captured) {
        auto* values = static_cast<std::uint32_t*>(pixels);
        const std::size_t count = static_cast<std::size_t>(width) *
                                  static_cast<std::size_t>(height);
        const std::size_t stride = std::max<std::size_t>(1, count / 8192U);
        snapshot_has_content_ = false;
        for (std::size_t index = 0; index < count; index += stride) {
            if ((values[index] & 0x00FFFFFFU) != 0) {
                snapshot_has_content_ = true;
                break;
            }
        }
        if (!snapshot_has_content_) captured = false;
        for (std::size_t index = 0; index < count; ++index) values[index] |= 0xFF000000U;
        const D2D1_BITMAP_PROPERTIES1 properties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_NONE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
            96.0F, 96.0F);
        snapshot_.Reset();
        if (captured) {
            captured = SUCCEEDED(surface_.context()->CreateBitmap(
                D2D1::SizeU(static_cast<UINT>(width), static_cast<UINT>(height)), pixels,
                static_cast<UINT32>(width * 4), properties, snapshot_.GetAddressOf()));
        }
    }
    SelectObject(memory, previous);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    snapshot_has_content_ = captured && snapshot_has_content_;
    return snapshot_has_content_;
}

bool ZoomInkWindow::capture_composite(PointF first, PointF second) {
    const int left = std::clamp(
        static_cast<int>(std::floor(std::min(first.x, second.x))),
        0, static_cast<int>(surface_.width()));
    const int top = std::clamp(
        static_cast<int>(std::floor(std::min(first.y, second.y))),
        0, static_cast<int>(surface_.height()));
    const int right = std::clamp(
        static_cast<int>(std::ceil(std::max(first.x, second.x))),
        0, static_cast<int>(surface_.width()));
    const int bottom = std::clamp(
        static_cast<int>(std::ceil(std::max(first.y, second.y))),
        0, static_cast<int>(surface_.height()));
    const int width = right - left;
    const int height = bottom - top;
    if (width < 8 || height < 8 || !snapshot_ || !snapshot_has_content_) {
        if (controller_.palette()) controller_.palette()->show_notification(
            L"Captura cancelada", L"Selecciona una region visible mas grande.");
        return false;
    }

    HDC screen = GetDC(nullptr);
    BITMAPINFO information{};
    information.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    information.bmiHeader.biWidth = width;
    information.bmiHeader.biHeight = -height;
    information.bmiHeader.biPlanes = 1;
    information.bmiHeader.biBitCount = 32;
    information.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP bitmap = screen ? CreateDIBSection(
        screen, &information, DIB_RGB_COLORS, &pixels, nullptr, 0) : nullptr;
    if (screen) ReleaseDC(nullptr, screen);
    if (!bitmap || !pixels) {
        if (bitmap) DeleteObject(bitmap);
        if (controller_.palette()) controller_.palette()->show_notification(
            L"No se pudo capturar", L"Windows no entrego una imagen para el zoom.");
        return false;
    }

    ComPtr<ID2D1DeviceContext> context;
    ComPtr<ID2D1Bitmap1> target;
    ComPtr<ID2D1Bitmap1> readable;
    HRESULT result = controller_.graphics().d2d_device()->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE, context.GetAddressOf());
    if (SUCCEEDED(result)) {
        context->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
        const auto target_properties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                              D2D1_ALPHA_MODE_PREMULTIPLIED),
            96.0F, 96.0F);
        result = context->CreateBitmap(
            D2D1::SizeU(static_cast<UINT>(width), static_cast<UINT>(height)),
            nullptr, 0, target_properties, target.GetAddressOf());
    }
    if (SUCCEEDED(result)) {
        context->SetTarget(target.Get());
        context->BeginDraw();
        context->SetTransform(D2D1::Matrix3x2F::Identity());
        context->Clear(D2D1::ColorF(D2D1::ColorF::Black));
        context->DrawBitmap(
            snapshot_.Get(),
            D2D1::RectF(0.0F, 0.0F, static_cast<float>(width),
                        static_cast<float>(height)),
            1.0F, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
            D2D1::RectF(static_cast<float>(left), static_cast<float>(top),
                        static_cast<float>(right), static_cast<float>(bottom)));
        if (controller_.state().annotations_visible) {
            const Document& active_document = edit_mode_ ? edit_document_ : document_;
            const float render_scale = edit_mode_ ? edit_transform_.scale : 1.0F;
            const float offset_x = edit_mode_
                ? edit_transform_.source.left +
                    static_cast<float>(left) / render_scale
                : static_cast<float>(left);
            const float offset_y = edit_mode_
                ? edit_transform_.source.top +
                    static_cast<float>(top) / render_scale
                : static_cast<float>(top);
            const RectF viewport{
                offset_x, offset_y,
                offset_x + static_cast<float>(width) / render_scale,
                offset_y + static_cast<float>(height) / render_scale};
            DrawableRenderResources resources;
            if (!active_document.empty() &&
                !resources.initialize(controller_.graphics(), context.Get())) {
                result = E_FAIL;
            } else {
                for (const auto& drawable : active_document.items()) {
                    const RectF bounds = edit_mode_
                        ? drawable_render_bounds(drawable) : drawable.bounds();
                    if (bounds.intersects(viewport)) {
                        draw_drawable(controller_.graphics(), context.Get(), resources,
                                      drawable, offset_x, offset_y, 1.0F,
                                      render_scale, edit_mode_);
                    }
                }
            }
        }
        const HRESULT end_result = context->EndDraw();
        if (SUCCEEDED(result)) result = end_result;
        context->SetTarget(nullptr);
    }
    if (SUCCEEDED(result)) {
        const auto readable_properties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                              D2D1_ALPHA_MODE_PREMULTIPLIED),
            96.0F, 96.0F);
        result = context->CreateBitmap(
            D2D1::SizeU(static_cast<UINT>(width), static_cast<UINT>(height)),
            nullptr, 0, readable_properties, readable.GetAddressOf());
    }
    if (SUCCEEDED(result)) result = readable->CopyFromBitmap(nullptr, target.Get(), nullptr);
    if (SUCCEEDED(result)) {
        D2D1_MAPPED_RECT mapped{};
        result = readable->Map(D2D1_MAP_OPTIONS_READ, &mapped);
        if (SUCCEEDED(result)) {
            for (int row = 0; row < height; ++row) {
                std::memcpy(static_cast<std::uint8_t*>(pixels) +
                                static_cast<std::size_t>(row) *
                                    static_cast<std::size_t>(width) * 4U,
                            mapped.bits + static_cast<std::size_t>(row) * mapped.pitch,
                            static_cast<std::size_t>(width) * 4U);
            }
            readable->Unmap();
        }
    }
    if (FAILED(result)) {
        DeleteObject(bitmap);
        if (controller_.palette()) controller_.palette()->show_notification(
            L"No se pudo capturar", L"No fue posible componer la imagen congelada.");
        return false;
    }
    return controller_.publish_capture_bitmap(bitmap, width, height);
}

bool ZoomInkWindow::freeze(RECT bounds, HWND magnifier) {
    set_bounds(bounds);
    UpdateWindow(magnifier);
    DwmFlush();
    if (!capture_snapshot(magnifier, bounds)) return false;
    set_frozen(true);
    render();
    ShowWindow(window_, SW_SHOW);
    SetForegroundWindow(window_);
    SetFocus(window_);
    return true;
}

void ZoomInkWindow::clear_annotations() {
    cancel_gesture();
    Document& active = edit_mode_ ? edit_document_ : document_;
    if (active.clear()) invalidate();
}

bool ZoomInkWindow::undo() {
    Document& active = edit_mode_ ? edit_document_ : document_;
    const bool changed = active.undo();
    if (changed) invalidate();
    return changed;
}

bool ZoomInkWindow::redo() {
    Document& active = edit_mode_ ? edit_document_ : document_;
    const bool changed = active.redo();
    if (changed) invalidate();
    return changed;
}

void ZoomInkWindow::commit_text_screen(PointF position, Color color, float thickness,
                                       std::wstring text) {
    POINT point{static_cast<LONG>(std::lround(position.x)),
                static_cast<LONG>(std::lround(position.y))};
    ScreenToClient(window_, &point);
    Drawable drawable;
    drawable.kind = Tool::Text;
    drawable.color = color;
    if (edit_mode_) {
        const float view_font_size = std::clamp(
            thickness * 3.4F, 16.0F, 72.0F);
        drawable.width = annotation_length(view_font_size) / 3.4F;
    } else {
        drawable.width = thickness;
    }
    const PointF local{static_cast<float>(point.x), static_cast<float>(point.y)};
    const PointF origin = annotation_point(local);
    drawable.points.push_back(origin);
    if (edit_mode_) {
        drawable.points.push_back({
            origin.x + annotation_length(600.0F),
            origin.y + annotation_length(400.0F)});
    }
    drawable.text = std::move(text);
    (edit_mode_ ? edit_document_ : document_).add(std::move(drawable));
    invalidate();
}

PointF ZoomInkWindow::local_point(LPARAM lparam) const noexcept {
    return {static_cast<float>(GET_X_LPARAM(lparam)),
            static_cast<float>(GET_Y_LPARAM(lparam))};
}

PointF ZoomInkWindow::annotation_point(PointF local) const noexcept {
    return edit_mode_ ? edit_transform_.view_to_source(local) : local;
}

PointF ZoomInkWindow::view_point(PointF annotation) const noexcept {
    return edit_mode_ ? edit_transform_.source_to_view(annotation) : annotation;
}

float ZoomInkWindow::annotation_length(float view_length) const noexcept {
    return edit_mode_ ? edit_transform_.view_to_source_length(view_length)
                      : view_length;
}

std::optional<PointF> ZoomInkWindow::pointer_local_point(WPARAM wparam) const noexcept {
    POINTER_INFO information{};
    if (!GetPointerInfo(GET_POINTERID_WPARAM(wparam), &information)) return std::nullopt;
    return PointF{static_cast<float>(information.ptPixelLocation.x - bounds_.left),
                  static_cast<float>(information.ptPixelLocation.y - bounds_.top)};
}

void ZoomInkWindow::begin_gesture(PointF point, float pressure) {
    if (!accepts_annotations()) return;
    PointF screen_point{point.x + static_cast<float>(bounds_.left),
                        point.y + static_cast<float>(bounds_.top)};
    if (controller_.route_palette_command(screen_point)) return;
    point = annotation_point(point);
    Document& active = edit_mode_ ? edit_document_ : document_;
    const Tool tool = current_gesture_tool(controller_.state().tool);
    if (tool == Tool::Eraser) {
        active.begin_compound();
        erasing_ = true;
        if (active.erase_at(point, annotation_length(
                std::max(12.0F, controller_.state().thickness * 2.5F)))) invalidate();
        drawing_ = true;
        SetCapture(window_);
        return;
    }
    if (tool == Tool::Text) {
        controller_.begin_text(screen_point);
        return;
    }
    if (!is_drawing_tool(tool)) return;
    Drawable drawable;
    drawable.kind = tool;
    drawable.color = controller_.state().color;
    drawable.reference_scale = edit_mode_ ? edit_transform_.scale : 1.0F;
    drawable.width = annotation_length(controller_.state().effective_width() *
        std::clamp(pressure, 0.35F, 1.45F));
    drawable.points.push_back(point);
    if (tool == Tool::Line || tool == Tool::Rectangle || tool == Tool::Ellipse ||
        tool == Tool::Arrow || tool == Tool::CurvedArrow || tool == Tool::Screenshot)
        drawable.points.push_back(point);
    preview_ = std::move(drawable);
    drawing_ = true;
    SetCapture(window_);
    invalidate();
}

void ZoomInkWindow::update_gesture(PointF point, WPARAM keys) {
    if (!drawing_) return;
    point = annotation_point(point);
    Document& active = edit_mode_ ? edit_document_ : document_;
    if (erasing_) {
        if ((keys & MK_LBUTTON) != 0 && active.erase_at(
                point, annotation_length(std::max(
                    12.0F, controller_.state().thickness * 2.5F)))) invalidate();
        return;
    }
    if (!preview_) return;
    preview_->invalidate_bounds_cache();
    const Tool tool = preview_->kind;
    if (tool == Tool::Line || tool == Tool::Rectangle || tool == Tool::Ellipse ||
        tool == Tool::Arrow || tool == Tool::CurvedArrow || tool == Tool::Screenshot) {
        if ((keys & MK_SHIFT) != 0 &&
            (tool == Tool::Rectangle || tool == Tool::Ellipse)) {
            const PointF origin = preview_->points.front();
            const float dx = point.x - origin.x;
            const float dy = point.y - origin.y;
            const float size = std::max(std::abs(dx), std::abs(dy));
            point.x = origin.x + std::copysign(size, dx == 0 ? 1.0F : dx);
            point.y = origin.y + std::copysign(size, dy == 0 ? 1.0F : dy);
        }
        preview_->points.back() = point;
    } else if (distance(preview_->points.back(), point) >= annotation_length(1.0F)) {
        if (preview_->points.size() >= 8192) {
            std::vector<PointF> reduced;
            reduced.reserve(preview_->points.size() / 2 + 1);
            for (std::size_t index = 0; index < preview_->points.size(); index += 2)
                reduced.push_back(preview_->points[index]);
            if (reduced.back().x != preview_->points.back().x ||
                reduced.back().y != preview_->points.back().y)
                reduced.push_back(preview_->points.back());
            preview_->points = std::move(reduced);
        }
        preview_->points.push_back(point);
    }
    invalidate();
}

void ZoomInkWindow::finish_gesture(PointF point, WPARAM keys) {
    if (!drawing_) return;
    update_gesture(point, keys);
    drawing_ = false;
    if (GetCapture() == window_) ReleaseCapture();
    if (erasing_) {
        erasing_ = false;
        (edit_mode_ ? edit_document_ : document_).end_compound();
        controller_.update_overlay_interaction();
        invalidate();
        return;
    }
    if (!preview_) return;
    Drawable completed = std::move(*preview_);
    preview_.reset();
    if (completed.kind == Tool::Screenshot && completed.points.size() >= 2) {
        const PointF first_view = view_point(completed.points.front());
        const PointF second_view = view_point(completed.points.back());
        invalidate();
        UpdateWindow(window_);
        if (controller_.preferences().exclude_palette_from_capture) {
            capture_composite(first_view, second_view);
        } else {
            // When the user explicitly asks to include Elite Pen itself, capture
            // the presented composition so the palette and panels are preserved.
            DWORD previous_affinity = WDA_NONE;
            const bool had_affinity =
                GetWindowDisplayAffinity(window_, &previous_affinity) != FALSE;
            SetWindowDisplayAffinity(window_, WDA_NONE);
            RedrawWindow(window_, nullptr, nullptr,
                         RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
            controller_.restack_zoom();
            controller_.restack_palette();
            DwmFlush();
            controller_.capture_region(
                {first_view.x + static_cast<float>(bounds_.left),
                 first_view.y + static_cast<float>(bounds_.top)},
                {second_view.x + static_cast<float>(bounds_.left),
                 second_view.y + static_cast<float>(bounds_.top)},
                false);
            if (had_affinity) SetWindowDisplayAffinity(window_, previous_affinity);
        }
        controller_.set_tool(Tool::Interact);
        controller_.restack_zoom();
        controller_.restack_palette();
        invalidate();
        return;
    }
    if (completed.kind == Tool::Pen || completed.kind == Tool::Highlighter) {
        completed.points = simplify_path(completed.points,
            std::clamp(completed.width * 0.08F, 0.5F, 2.0F));
    }
    if (completed.points.size() >= 2 && distance(
            completed.points.front(), completed.points.back()) <
            annotation_length(2.0F))
        completed.points.resize(1);
    (edit_mode_ ? edit_document_ : document_).add(std::move(completed));
    controller_.update_overlay_interaction();
    invalidate();
}

void ZoomInkWindow::cancel_gesture() {
    if (!drawing_ && !erasing_) return;
    drawing_ = false;
    erasing_ = false;
    pointer_active_ = false;
    if (GetCapture() == window_) ReleaseCapture();
    (edit_mode_ ? edit_document_ : document_).end_compound();
    preview_.reset();
    invalidate();
}

void ZoomInkWindow::populate_edit_stress_document(std::size_t count) {
    edit_document_ = Document{};
    edit_document_cache_.reset();
    preview_.reset();
    const float origin_x = edit_transform_.source.left;
    const float origin_y = edit_transform_.source.top;
    for (std::size_t stroke = 0; stroke < count; ++stroke) {
        Drawable drawable;
        drawable.kind = stroke % 7U == 0U ? Tool::Highlighter : Tool::Pen;
        drawable.color = stroke % 5U == 0U ? kPurple : kBlue;
        drawable.width = drawable.kind == Tool::Highlighter ? 5.5F : 2.0F;
        drawable.points.reserve(24);
        const float base_x = origin_x + 20.0F +
            static_cast<float>(stroke % 100U) * 5.5F;
        const float base_y = origin_y + 20.0F +
            static_cast<float>((stroke / 100U) % 55U) * 4.5F;
        for (std::size_t sample = 0; sample < 24U; ++sample) {
            drawable.points.push_back({
                base_x + static_cast<float>(sample) * 3.5F,
                base_y + std::sin(static_cast<float>(sample) * 0.62F) * 4.5F});
        }
        edit_document_.add(std::move(drawable));
    }
    invalidate();
}

std::optional<PointF> ZoomInkWindow::first_edit_view_point() const noexcept {
    if (edit_document_.empty() || edit_document_.items().front().points.empty())
        return std::nullopt;
    return edit_transform_.source_to_view(
        edit_document_.items().front().points.front());
}

LRESULT ZoomInkWindow::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case kQaQueryZoomDocumentCountMessage:
            return static_cast<LRESULT>(item_count());
        case kQaQueryZoomEditDocumentCountMessage:
            return static_cast<LRESULT>(edit_document_.items().size());
        case kQaQueryZoomSnapshotMessage:
            return snapshot_ && snapshot_has_content_ ? 1 : 0;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_NCHITTEST: {
            if (!accepts_annotations()) return HTTRANSPARENT;
            const POINT screen_point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            if (controller_.palette() &&
                controller_.palette()->contains_screen_point(screen_point)) {
                // The palette is the authoritative command surface. Keep this
                // hit-test transparent over its rectangle even during a brief
                // topmost-order transition after the zoom snapshot is restored.
                return HTTRANSPARENT;
            }
            return HTCLIENT;
        }
        case WM_SETCURSOR:
            if (LOWORD(lparam) == HTCLIENT) {
                const Tool tool = current_gesture_tool(controller_.state().tool);
                if (tool == Tool::Text) SetCursor(LoadCursorW(nullptr, IDC_IBEAM));
                else if ((tool == Tool::Pen || tool == Tool::Highlighter) && pencil_cursor_)
                    SetCursor(pencil_cursor_);
                else SetCursor(LoadCursorW(nullptr, IDC_CROSS));
                return TRUE;
            }
            break;
        case WM_DPICHANGED:
            refresh_pencil_cursor();
            return 0;
        case WM_LBUTTONDOWN:
            if (!pointer_active_) begin_gesture(local_point(lparam));
            return 0;
        case WM_MOUSEMOVE:
            if (!pointer_active_) update_gesture(local_point(lparam), wparam);
            return 0;
        case WM_LBUTTONUP:
            if (!pointer_active_) finish_gesture(local_point(lparam), wparam);
            return 0;
        case WM_MOUSEWHEEL:
            if (edit_mode_) {
                const bool control = (GET_KEYSTATE_WPARAM(wparam) & MK_CONTROL) != 0 ||
                                     (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
                const bool shift = (GET_KEYSTATE_WPARAM(wparam) & MK_SHIFT) != 0 ||
                                   (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                if (control && shift) {
                    controller_.adjust_thickness_step(
                        GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 1 : -1);
                } else if (shift && !controller_.zoom_frozen() &&
                           controller_.preferences().zoom_view ==
                               static_cast<int>(ZoomView::Lens)) {
                    controller_.adjust_zoom_lens_diameter(
                        GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 1 : -1);
                } else {
                    controller_.execute_hotkey(
                        GET_WHEEL_DELTA_WPARAM(wparam) > 0
                            ? HotkeyAction::ZoomIn : HotkeyAction::ZoomOut);
                }
                return 0;
            }
            break;
        case WM_POINTERDOWN: {
            const UINT32 pointer_id = GET_POINTERID_WPARAM(wparam);
            POINTER_INPUT_TYPE type{};
            if (!GetPointerType(pointer_id, &type) ||
                (type != PT_PEN && type != PT_TOUCH)) return 0;
            const auto point = pointer_local_point(wparam);
            if (!point) return 0;
            float pressure = 1.0F;
            if (type == PT_PEN) {
                POINTER_PEN_INFO pen{};
                if (GetPointerPenInfo(pointer_id, &pen) &&
                    (pen.penMask & PEN_MASK_PRESSURE) != 0)
                    pressure = 0.45F + static_cast<float>(pen.pressure) / 1024.0F;
            }
            pointer_active_ = true;
            pointer_id_ = pointer_id;
            begin_gesture(*point, pressure);
            return 0;
        }
        case WM_POINTERUPDATE: {
            if (!pointer_active_ || GET_POINTERID_WPARAM(wparam) != pointer_id_) return 0;
            const auto point = pointer_local_point(wparam);
            if (point) {
                WPARAM keys = MK_LBUTTON;
                if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) keys |= MK_SHIFT;
                update_gesture(*point, keys);
            }
            return 0;
        }
        case WM_POINTERUP: {
            if (!pointer_active_ || GET_POINTERID_WPARAM(wparam) != pointer_id_) return 0;
            const auto point = pointer_local_point(wparam);
            if (point) finish_gesture(*point, 0);
            pointer_active_ = false;
            pointer_id_ = 0;
            return 0;
        }
        case WM_POINTERCAPTURECHANGED:
            pointer_active_ = false;
            pointer_id_ = 0;
            if (drawing_ && preview_) finish_gesture(
                view_point(preview_->points.back()), 0);
            else if (erasing_) cancel_gesture();
            return 0;
        case WM_CAPTURECHANGED:
            if (drawing_ && preview_) finish_gesture(
                view_point(preview_->points.back()), 0);
            else if (erasing_) cancel_gesture();
            return 0;
        case WM_KEYDOWN:
            for (std::size_t index = kGlobalHotkeyActionCount;
                 index < kHotkeyActionCount; ++index) {
                const auto action = static_cast<HotkeyAction>(index);
                if (controller_.matches_hotkey(action, wparam)) {
                    controller_.execute_hotkey(action);
                    return 0;
                }
            }
            if (wparam == VK_ESCAPE || wparam == VK_F4) {
                controller_.toggle_zoom();
                return 0;
            }
            if (edit_mode_ && controller_.preferences().zoom_view ==
                                  static_cast<int>(ZoomView::Lens) &&
                (wparam == VK_OEM_4 || wparam == VK_OEM_6)) {
                controller_.adjust_zoom_lens_diameter(
                    wparam == VK_OEM_6 ? 1 : -1);
                return 0;
            }
            break;
        case WM_RBUTTONDOWN:
            controller_.toggle_zoom();
            return 0;
        default: break;
    }
    return WindowBase::handle_message(message, wparam, lparam);
}

void ZoomInkWindow::render() {
    const bool annotations_visible = controller_.state().annotations_visible;
    const RectF viewport{0.0F, 0.0F, static_cast<float>(surface_.width()),
                         static_cast<float>(surface_.height())};
    const bool cache_ready = !annotations_visible ||
        (edit_mode_
            ? edit_document_cache_.update(
                controller_.graphics(), surface_, edit_document_)
            : document_cache_.update(controller_.graphics(), surface_, document_,
                                     0.0F, 0.0F, viewport));
    auto* context = surface_.begin_draw(D2D1::ColorF(0, 0.0F));
    if (!context) return;
    if ((frozen_ || (edit_mode_ && edit_annotating_)) && snapshot_) {
        const auto destination = D2D1::RectF(0, 0, static_cast<float>(surface_.width()),
                                             static_cast<float>(surface_.height()));
        context->DrawBitmap(snapshot_.Get(), destination, 1.0F,
                            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    }
    if (annotations_visible) {
        if (cache_ready) {
            if (edit_mode_) edit_document_cache_.draw(context, edit_transform_);
            else document_cache_.draw(context);
        } else {
            DrawableRenderResources fallback;
            if (fallback.initialize(controller_.graphics(), context)) {
                const Document& active_document = edit_mode_
                    ? edit_document_ : document_;
                const RectF active_viewport = edit_mode_
                    ? edit_transform_.source : viewport;
                for (const auto& drawable : active_document.items()) {
                    const RectF bounds = edit_mode_
                        ? drawable_render_bounds(drawable) : drawable.bounds();
                    if (bounds.intersects(active_viewport)) {
                        draw_drawable(controller_.graphics(), context, fallback,
                                      drawable,
                                      edit_mode_ ? edit_transform_.source.left : 0.0F,
                                      edit_mode_ ? edit_transform_.source.top : 0.0F,
                                      1.0F,
                                      edit_mode_ ? edit_transform_.scale : 1.0F,
                                      edit_mode_);
                    }
                }
            }
        }
        if (preview_) {
            DrawableRenderResources live;
            if (live.initialize(controller_.graphics(), context)) {
                draw_drawable(controller_.graphics(), context, live, *preview_,
                              edit_mode_ ? edit_transform_.source.left : 0.0F,
                              edit_mode_ ? edit_transform_.source.top : 0.0F,
                              0.82F,
                              edit_mode_ ? edit_transform_.scale : 1.0F,
                              edit_mode_);
            }
        }
    }
    if (frozen_) {
        const auto& theme = current_ui_theme();
        ComPtr<ID2D1SolidColorBrush> pill;
        ComPtr<ID2D1SolidColorBrush> gold;
        ComPtr<ID2D1SolidColorBrush> text;
        context->CreateSolidColorBrush(theme_color(theme.surface_1, 0.88F), pill.GetAddressOf());
        context->CreateSolidColorBrush(theme_color(theme.violet), gold.GetAddressOf());
        context->CreateSolidColorBrush(theme_color(theme.text), text.GetAddressOf());
        context->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(12, 12, 144, 42), 15, 15),
                                      pill.Get());
        context->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(25, 21, 28, 33), 1, 1),
                                      gold.Get());
        context->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(32, 21, 35, 33), 1, 1),
                                      gold.Get());
        ComPtr<IDWriteTextFormat> format;
        controller_.graphics().dwrite()->CreateTextFormat(
            L"Segoe UI Variable Text", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0F, L"es-CO",
            format.GetAddressOf());
        if (format) context->DrawTextW(L"ZOOM EN PAUSA", 13, format.Get(),
                                       D2D1::RectF(43, 17, 137, 38), text.Get());
    }
    std::wstring error;
    if (!surface_.end_draw(error)) controller_.report_runtime_error(error);
}

bool ZoomTargetWindow::initialize(GraphicsDevice& graphics) {
    constexpr DWORD ex_style = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                               WS_EX_LAYERED | WS_EX_TRANSPARENT;
    const RECT initial{0, 0, 64, 64};
    if (!create(L"ElitePen.ZoomTarget", L"Objetivo de lupa — Elite Pen",
                ex_style, WS_POPUP, initial)) return false;
    SetLayeredWindowAttributes(window_, 0, 255, LWA_ALPHA);
    if (!initialize_surface(graphics)) return false;
    // The lens target must remain visible in recordings. Excluding even a small
    // layered window can produce a black capture artifact on Windows 10.
    SetWindowDisplayAffinity(window_, WDA_NONE);
    return true;
}

void ZoomTargetWindow::show_at(POINT cursor) {
    RECT client{};
    GetClientRect(window_, &client);
    const int width = std::max(1L, client.right - client.left);
    const int height = std::max(1L, client.bottom - client.top);
    const int focus_x = static_cast<int>(std::lround(static_cast<float>(width) * 0.43F));
    const int focus_y = static_cast<int>(std::lround(static_cast<float>(height) * 0.43F));
    const bool was_visible = visible();
    SetWindowPos(window_, nullptr, cursor.x - focus_x, cursor.y - focus_y,
                 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    if (!was_visible) invalidate();
}

void ZoomTargetWindow::bring_to_front() {
    if (!visible()) return;
    SetWindowPos(window_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

LRESULT ZoomTargetWindow::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_NCHITTEST: return HTTRANSPARENT;
        case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
        default: break;
    }
    return WindowBase::handle_message(message, wparam, lparam);
}

void ZoomTargetWindow::render() {
    auto* context = surface_.begin_draw(D2D1::ColorF(0, 0.0F));
    if (!context) return;
    const float width = static_cast<float>(surface_.width());
    const float height = static_cast<float>(surface_.height());
    const float cx = width * 0.43F;
    const float cy = height * 0.43F;
    const float radius = std::min(width, height) * 0.29F;
    const auto& theme = current_ui_theme();
    ComPtr<ID2D1SolidColorBrush> shadow;
    ComPtr<ID2D1SolidColorBrush> halo;
    ComPtr<ID2D1SolidColorBrush> gold;
    ComPtr<ID2D1SolidColorBrush> blue;
    ComPtr<ID2D1SolidColorBrush> glass;
    context->CreateSolidColorBrush(theme_color(theme.shadow, theme.light ? 0.28F : 0.72F),
                                   shadow.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.light ? 0xFFFFFF : theme.text, 0.94F),
                                   halo.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.violet), gold.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.mint), blue.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.violet, 0.14F), glass.GetAddressOf());
    const auto lens = D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius);
    context->FillEllipse(lens, glass.Get());
    context->DrawEllipse(lens, shadow.Get(), 5.2F);
    context->DrawEllipse(lens, halo.Get(), 3.3F);
    context->DrawEllipse(lens, gold.Get(), 1.8F);
    const float diagonal = radius * 0.68F;
    const D2D1_POINT_2F handle_start{cx + diagonal, cy + diagonal};
    const D2D1_POINT_2F handle_finish{cx + radius * 1.42F, cy + radius * 1.42F};
    context->DrawLine(handle_start, handle_finish, shadow.Get(), 7.0F);
    context->DrawLine(handle_start, handle_finish, halo.Get(), 4.5F);
    context->DrawLine(handle_start, handle_finish, gold.Get(), 2.4F);
    context->DrawLine(D2D1::Point2F(cx - 5.0F, cy),
                      D2D1::Point2F(cx + 5.0F, cy), blue.Get(), 1.6F);
    context->DrawLine(D2D1::Point2F(cx, cy - 5.0F),
                      D2D1::Point2F(cx, cy + 5.0F), blue.Get(), 1.6F);
    context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 1.8F, 1.8F), blue.Get());
    std::wstring error;
    if (!surface_.end_draw(error)) controller_.report_runtime_error(error);
}

bool ZoomLensFrameWindow::initialize(GraphicsDevice& graphics) {
    constexpr DWORD ex_style = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                               WS_EX_NOREDIRECTIONBITMAP | WS_EX_TRANSPARENT;
    const RECT initial{0, 0, 1, 1};
    if (!create(L"ElitePen.ZoomLensFrame", L"Aro de lente — Elite Pen",
                ex_style, WS_POPUP, initial)) return false;
    if (!initialize_surface(graphics)) return false;
    // The frame is part of the presentation itself and must remain visible in
    // OBS and other monitor-capture workflows.
    SetWindowDisplayAffinity(window_, WDA_NONE);
    return true;
}

void ZoomLensFrameWindow::show_for(RECT lens_bounds) {
    constexpr int padding = 16;
    const RECT frame_bounds{
        lens_bounds.left - padding, lens_bounds.top - padding,
        lens_bounds.right + padding, lens_bounds.bottom + padding};
    const bool changed = !EqualRect(&lens_bounds_, &lens_bounds);
    const bool was_visible = visible();
    if (!changed && was_visible) return;
    lens_bounds_ = lens_bounds;
    SetWindowPos(window_, nullptr, frame_bounds.left, frame_bounds.top,
                 std::max(1L, frame_bounds.right - frame_bounds.left),
                 std::max(1L, frame_bounds.bottom - frame_bounds.top),
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    if (changed || !was_visible) invalidate();
}

void ZoomLensFrameWindow::bring_to_front() {
    if (!visible()) return;
    SetWindowPos(window_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

LRESULT ZoomLensFrameWindow::handle_message(UINT message, WPARAM wparam,
                                             LPARAM lparam) {
    switch (message) {
        case WM_NCHITTEST: return HTTRANSPARENT;
        case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
        default: break;
    }
    return WindowBase::handle_message(message, wparam, lparam);
}

void ZoomLensFrameWindow::render() {
    auto* context = surface_.begin_draw(D2D1::ColorF(0, 0.0F));
    if (!context) return;
    constexpr float padding = 16.0F;
    const float width = static_cast<float>(surface_.width());
    const float height = static_cast<float>(surface_.height());
    const float radius = std::max(1.0F, (std::min(width, height) - 2.0F * padding) / 2.0F);
    const auto lens = D2D1::Ellipse(D2D1::Point2F(width / 2.0F, height / 2.0F),
                                    radius, radius);
    const auto& theme = current_ui_theme();
    ComPtr<ID2D1SolidColorBrush> shadow;
    ComPtr<ID2D1SolidColorBrush> aura;
    ComPtr<ID2D1SolidColorBrush> rim;
    ComPtr<ID2D1SolidColorBrush> glass;
    context->CreateSolidColorBrush(
        theme_color(theme.shadow, theme.light ? 0.14F : 0.34F), shadow.GetAddressOf());
    context->CreateSolidColorBrush(
        theme_color(theme.violet, theme.light ? 0.10F : 0.16F), aura.GetAddressOf());
    context->CreateSolidColorBrush(
        theme_color(theme.surface_1, theme.light ? 0.88F : 0.92F), rim.GetAddressOf());
    context->CreateSolidColorBrush(
        theme_color(theme.light ? 0xFFFFFF : theme.violet, theme.light ? 0.018F : 0.024F),
        glass.GetAddressOf());
    const D2D1_GRADIENT_STOP stops[] = {
        {0.0F, theme_color(theme.light ? 0xFFFFFF : theme.text, 0.92F)},
        {0.42F, theme_color(theme.violet, 0.92F)},
        {1.0F, theme_color(theme.mint, 0.84F)}};
    const auto sheen = linear_gradient(
        context, D2D1::Point2F(padding, padding),
        D2D1::Point2F(width - padding, height - padding), stops,
        static_cast<UINT>(std::size(stops)));

    context->FillEllipse(lens, glass.Get());
    context->DrawEllipse(lens, shadow.Get(), 12.0F);
    context->DrawEllipse(lens, aura.Get(), 8.0F);
    context->DrawEllipse(lens, rim.Get(), 4.5F);
    if (sheen) context->DrawEllipse(lens, sheen.Get(), 2.0F);

    std::wstring error;
    if (!surface_.end_draw(error)) controller_.report_runtime_error(error);
}

bool ZoomEditToolbarWindow::initialize(GraphicsDevice& graphics) {
    constexpr DWORD ex_style = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                               WS_EX_LAYERED;
    const RECT initial{0, 0, 430, 58};
    if (!create(L"ElitePen.ZoomEditToolbar",
                L"Zoom editable — Navegar — Elite Pen",
                ex_style, WS_POPUP, initial)) return false;
    SetLayeredWindowAttributes(window_, 0, 255, LWA_ALPHA);
    if (!initialize_surface(graphics)) return false;
    SetWindowDisplayAffinity(window_, WDA_NONE);
    return true;
}

void ZoomEditToolbarWindow::show_for(RECT zoom_bounds, float factor,
                                     ZoomEditState state) {
    factor_ = factor;
    state_ = state;
    const float dpi_scale = static_cast<float>(GetDpiForWindow(window_)) / 96.0F;
    const int available = std::max(280L, zoom_bounds.right - zoom_bounds.left - 16L);
    const int width = std::min(
        static_cast<int>(std::lround(430.0F * dpi_scale)), available);
    const int height = static_cast<int>(std::lround(58.0F * dpi_scale));
    int left = zoom_bounds.left +
        ((zoom_bounds.right - zoom_bounds.left) - width) / 2;
    int top = zoom_bounds.top + static_cast<int>(std::lround(14.0F * dpi_scale));
    RECT candidate{left, top, left + width, top + height};
    if (controller_.palette() &&
        IsWindowVisible(controller_.palette()->hwnd()) != FALSE) {
        RECT palette{};
        GetWindowRect(controller_.palette()->hwnd(), &palette);
        RECT overlap{};
        if (IntersectRect(&overlap, &candidate, &palette)) {
            top = zoom_bounds.bottom - height -
                static_cast<int>(std::lround(14.0F * dpi_scale));
        }
    }
    SetWindowPos(window_, HWND_TOPMOST, left, top, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetWindowTextW(window_, state_ == ZoomEditState::Annotate
        ? L"Zoom editable — Anotar — Elite Pen"
        : L"Zoom editable — Navegar — Elite Pen");
    invalidate();
}

void ZoomEditToolbarWindow::bring_to_front() {
    if (!visible()) return;
    SetWindowPos(window_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

bool ZoomEditToolbarWindow::contains_screen_point(POINT point) const noexcept {
    RECT bounds{};
    return window_ && GetWindowRect(window_, &bounds) &&
           PtInRect(&bounds, point) != FALSE;
}

ZoomEditToolbarWindow::Command ZoomEditToolbarWindow::command_at(
        POINT client) const noexcept {
    RECT bounds{};
    GetClientRect(window_, &bounds);
    const float width = static_cast<float>(std::max(1L, bounds.right));
    const float x = static_cast<float>(client.x) / width;
    if (x < 0.105F) return Command::ZoomOut;
    if (x < 0.285F) return Command::None;
    if (x < 0.390F) return Command::ZoomIn;
    if (x < 0.625F) return Command::Navigate;
    if (x < 0.875F) return Command::Annotate;
    return Command::Close;
}

void ZoomEditToolbarWindow::execute(Command command) {
    switch (command) {
        case Command::ZoomOut: owner_.adjust_zoom(-0.25F); break;
        case Command::ZoomIn: owner_.adjust_zoom(0.25F); break;
        case Command::Navigate: owner_.set_edit_state(ZoomEditState::Navigate); break;
        case Command::Annotate: owner_.set_edit_state(ZoomEditState::Annotate); break;
        case Command::Close: owner_.toggle_edit_mode(); break;
        case Command::None: break;
    }
}

LRESULT ZoomEditToolbarWindow::handle_message(UINT message, WPARAM wparam,
                                               LPARAM lparam) {
    switch (message) {
        case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
        case WM_NCHITTEST: return HTCLIENT;
        case WM_SETCURSOR:
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        case WM_MOUSEMOVE: {
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window_, 0};
            TrackMouseEvent(&tracking);
            const Command next = command_at(
                {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
            if (next != hovered_) {
                hovered_ = next;
                invalidate();
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            hovered_ = Command::None;
            invalidate();
            return 0;
        case WM_LBUTTONUP:
            execute(command_at({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)}));
            return 0;
        case WM_MOUSEWHEEL:
            owner_.adjust_zoom(GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 0.25F : -0.25F);
            return 0;
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) {
                owner_.toggle_edit_mode();
                return 0;
            }
            break;
        default: break;
    }
    return WindowBase::handle_message(message, wparam, lparam);
}

void ZoomEditToolbarWindow::render() {
    auto* context = surface_.begin_draw(D2D1::ColorF(0, 0.0F));
    if (!context) return;
    const float width = static_cast<float>(surface_.width());
    const float height = static_cast<float>(surface_.height());
    const auto& theme = current_ui_theme();
    ComPtr<ID2D1SolidColorBrush> shadow;
    ComPtr<ID2D1SolidColorBrush> panel;
    ComPtr<ID2D1SolidColorBrush> border;
    ComPtr<ID2D1SolidColorBrush> text;
    ComPtr<ID2D1SolidColorBrush> soft;
    ComPtr<ID2D1SolidColorBrush> accent;
    ComPtr<ID2D1SolidColorBrush> active;
    ComPtr<ID2D1SolidColorBrush> hover;
    context->CreateSolidColorBrush(theme_color(theme.shadow, 0.34F), shadow.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.surface_1, 0.96F), panel.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.line, 0.78F), border.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.text), text.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.text_soft), soft.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.violet), accent.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.violet, 0.18F), active.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.violet, 0.10F), hover.GetAddressOf());
    const auto outer = D2D1::RoundedRect(D2D1::RectF(3.0F, 4.0F, width - 3.0F,
                                                     height - 2.0F),
                                         (height - 6.0F) * 0.5F,
                                         (height - 6.0F) * 0.5F);
    context->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(5.0F, 7.0F, width - 1.0F, height),
                          (height - 7.0F) * 0.5F, (height - 7.0F) * 0.5F),
        shadow.Get());
    context->FillRoundedRectangle(outer, panel.Get());
    context->DrawRoundedRectangle(outer, border.Get(), 1.0F);

    const auto command_rect = [width, height](Command command) {
        float left{};
        float right{};
        switch (command) {
            case Command::ZoomOut: left = 0.018F; right = 0.105F; break;
            case Command::ZoomIn: left = 0.292F; right = 0.385F; break;
            case Command::Navigate: left = 0.402F; right = 0.617F; break;
            case Command::Annotate: left = 0.632F; right = 0.865F; break;
            case Command::Close: left = 0.888F; right = 0.978F; break;
            case Command::None: return D2D1::RectF();
        }
        return D2D1::RectF(width * left, 9.0F, width * right, height - 7.0F);
    };
    for (const Command command : {Command::ZoomOut, Command::ZoomIn,
             Command::Navigate, Command::Annotate, Command::Close}) {
        const bool selected =
            (command == Command::Navigate && state_ == ZoomEditState::Navigate) ||
            (command == Command::Annotate && state_ == ZoomEditState::Annotate);
        if (selected || hovered_ == command) {
            const auto rectangle = command_rect(command);
            context->FillRoundedRectangle(D2D1::RoundedRect(
                rectangle, (height - 16.0F) * 0.5F, (height - 16.0F) * 0.5F),
                selected ? active.Get() : hover.Get());
        }
    }

    ComPtr<IDWriteTextFormat> regular;
    ComPtr<IDWriteTextFormat> compact;
    controller_.graphics().dwrite()->CreateTextFormat(
        L"Segoe UI Variable Display", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        std::clamp(height * 0.28F, 12.0F, 17.0F), L"es-CO",
        regular.GetAddressOf());
    controller_.graphics().dwrite()->CreateTextFormat(
        L"Segoe UI Variable Text", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        std::clamp(height * 0.21F, 10.0F, 13.0F), L"es-CO",
        compact.GetAddressOf());
    if (regular) {
        regular->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        regular->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        context->DrawTextW(L"−", 1, regular.Get(), command_rect(Command::ZoomOut),
                           soft.Get());
        context->DrawTextW(L"+", 1, regular.Get(), command_rect(Command::ZoomIn),
                           soft.Get());
        context->DrawTextW(L"×", 1, regular.Get(), command_rect(Command::Close),
                           soft.Get());
    }
    if (compact) {
        compact->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        compact->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        wchar_t percentage[16]{};
        swprintf_s(percentage, L"%d%%", static_cast<int>(std::lround(factor_ * 100.0F)));
        context->DrawTextW(percentage, static_cast<UINT32>(wcslen(percentage)),
                           compact.Get(), D2D1::RectF(width * 0.105F, 8.0F,
                                                     width * 0.285F, height - 6.0F),
                           text.Get());
        context->DrawTextW(L"MANO", 4, compact.Get(), command_rect(Command::Navigate),
                           state_ == ZoomEditState::Navigate ? accent.Get() : soft.Get());
        context->DrawTextW(L"LÁPIZ", 5, compact.Get(), command_rect(Command::Annotate),
                           state_ == ZoomEditState::Annotate ? accent.Get() : soft.Get());
    }
    context->DrawLine(D2D1::Point2F(width * 0.395F, 16.0F),
                      D2D1::Point2F(width * 0.395F, height - 14.0F), border.Get(), 1.0F);
    context->DrawLine(D2D1::Point2F(width * 0.878F, 16.0F),
                      D2D1::Point2F(width * 0.878F, height - 14.0F), border.Get(), 1.0F);
    std::wstring error;
    if (!surface_.end_draw(error)) controller_.report_runtime_error(error);
}

bool ZoomWindow::load_magnification() {
    if (magnification_module_) return true;
    magnification_module_ = LoadLibraryW(L"Magnification.dll");
    if (!magnification_module_) return false;
    mag_initialize_ = reinterpret_cast<MagInitializePointer>(
        GetProcAddress(magnification_module_, "MagInitialize"));
    mag_uninitialize_ = reinterpret_cast<MagUninitializePointer>(
        GetProcAddress(magnification_module_, "MagUninitialize"));
    mag_set_source_ = reinterpret_cast<MagSetWindowSourcePointer>(
        GetProcAddress(magnification_module_, "MagSetWindowSource"));
    mag_set_transform_ = reinterpret_cast<MagSetWindowTransformPointer>(
        GetProcAddress(magnification_module_, "MagSetWindowTransform"));
    mag_set_filter_ = reinterpret_cast<MagSetWindowFilterListPointer>(
        GetProcAddress(magnification_module_, "MagSetWindowFilterList"));
    mag_set_color_effect_ = reinterpret_cast<MagSetColorEffectPointer>(
        GetProcAddress(magnification_module_, "MagSetColorEffect"));
    return mag_initialize_ && mag_uninitialize_ && mag_set_source_ && mag_set_transform_;
}

ZoomWindow* ZoomWindow::click_hook_owner_ = nullptr;

LRESULT CALLBACK ZoomWindow::click_hook_proc(int code, WPARAM wparam, LPARAM lparam) {
    ZoomWindow* zoom = click_hook_owner_;
    if (code == HC_ACTION && zoom && zoom->active_ && !zoom->frozen() &&
        zoom->edit_state_ != ZoomEditState::Navigate &&
        (wparam == WM_LBUTTONDOWN || wparam == WM_LBUTTONUP)) {
        const auto* information = reinterpret_cast<const MSLLHOOKSTRUCT*>(lparam);
        if (information && wparam == WM_LBUTTONDOWN) {
            PaletteWindow* palette = zoom->controller_.palette();
            if ((!palette || !palette->contains_screen_point(information->pt)) &&
                !zoom->toolbar_contains(information->pt) &&
                !zoom->click_freeze_pending_) {
                zoom->click_freeze_pending_ = true;
                return 1;
            }
        }
        if (wparam == WM_LBUTTONUP && zoom->click_freeze_pending_) {
            PostMessageW(zoom->window_, kZoomClickFreezeMessage, 0, 0);
            return 1;
        }
    }
    return CallNextHookEx(zoom ? zoom->click_hook_ : nullptr, code, wparam, lparam);
}

bool ZoomWindow::install_click_hook() {
    if (click_hook_) return true;
    click_hook_owner_ = this;
    click_hook_ = SetWindowsHookExW(WH_MOUSE_LL, click_hook_proc,
                                    GetModuleHandleW(nullptr), 0);
    if (!click_hook_ && click_hook_owner_ == this) click_hook_owner_ = nullptr;
    return click_hook_ != nullptr;
}

void ZoomWindow::uninstall_click_hook() {
    if (click_hook_) {
        UnhookWindowsHookEx(click_hook_);
        click_hook_ = nullptr;
    }
    click_freeze_pending_ = false;
    if (click_hook_owner_ == this) click_hook_owner_ = nullptr;
}

void ZoomWindow::remember_foreground_target() {
    HWND candidate = GetForegroundWindow();
    if (!candidate || !IsWindow(candidate)) return;
    if (const HWND root = GetAncestor(candidate, GA_ROOT)) candidate = root;
    DWORD process_id{};
    GetWindowThreadProcessId(candidate, &process_id);
    if (process_id != GetCurrentProcessId()) foreground_before_zoom_ = candidate;
}

bool ZoomWindow::start_recordable_output() {
    stop_recordable_output();
    // Windows' Magnification control is composed after both OBS monitor-capture
    // paths and is therefore recorded as black. While OBS is present, display a
    // live DWM thumbnail of the focused application instead. WGC records this
    // normal desktop composition while the native magnifier remains the fallback
    // for ordinary use and unsupported windows.
    if (!obs_is_running()) return false;
    if (!window_ || !foreground_before_zoom_ ||
        !IsWindow(foreground_before_zoom_) ||
        !IsWindowVisible(foreground_before_zoom_) ||
        IsIconic(foreground_before_zoom_)) {
        return false;
    }
    BOOL cloaked = FALSE;
    if (SUCCEEDED(DwmGetWindowAttribute(
            foreground_before_zoom_, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) &&
        cloaked) {
        return false;
    }
    if (FAILED(DwmRegisterThumbnail(
            window_, foreground_before_zoom_, &recordable_thumbnail_)) ||
        !recordable_thumbnail_) {
        recordable_thumbnail_ = nullptr;
        return false;
    }
    if (FAILED(DwmQueryThumbnailSourceSize(
            recordable_thumbnail_, &recordable_source_size_)) ||
        recordable_source_size_.cx <= 0 || recordable_source_size_.cy <= 0 ||
        !GetWindowRect(foreground_before_zoom_, &recordable_source_bounds_)) {
        stop_recordable_output();
        return false;
    }
    recordable_output_ = true;
    ShowWindow(magnifier_, SW_HIDE);
    return true;
}

void ZoomWindow::stop_recordable_output() {
    recordable_output_ = false;
    if (recordable_thumbnail_) {
        DwmUnregisterThumbnail(recordable_thumbnail_);
        recordable_thumbnail_ = nullptr;
    }
    recordable_source_size_ = {};
    recordable_source_bounds_ = {};
    if (magnifier_) ShowWindow(magnifier_, SW_SHOWNA);
}

void ZoomWindow::update_recordable_output() {
    if (!recordable_output_ || !recordable_thumbnail_) return;
    RECT current_bounds{};
    if (!IsWindow(foreground_before_zoom_) ||
        !GetWindowRect(foreground_before_zoom_, &current_bounds)) {
        stop_recordable_output();
        return;
    }
    recordable_source_bounds_ = current_bounds;
    const int window_width = std::max(
        1L, recordable_source_bounds_.right - recordable_source_bounds_.left);
    const int window_height = std::max(
        1L, recordable_source_bounds_.bottom - recordable_source_bounds_.top);
    const double scale_x = static_cast<double>(recordable_source_size_.cx) /
                           static_cast<double>(window_width);
    const double scale_y = static_cast<double>(recordable_source_size_.cy) /
                           static_cast<double>(window_height);
    RECT source{
        static_cast<LONG>(std::lround(
            static_cast<double>(source_rect_.left - recordable_source_bounds_.left) *
            scale_x)),
        static_cast<LONG>(std::lround(
            static_cast<double>(source_rect_.top - recordable_source_bounds_.top) *
            scale_y)),
        static_cast<LONG>(std::lround(
            static_cast<double>(source_rect_.right - recordable_source_bounds_.left) *
            scale_x)),
        static_cast<LONG>(std::lround(
            static_cast<double>(source_rect_.bottom - recordable_source_bounds_.top) *
            scale_y))};
    const LONG desired_width = std::clamp(
        source.right - source.left, 1L, recordable_source_size_.cx);
    const LONG desired_height = std::clamp(
        source.bottom - source.top, 1L, recordable_source_size_.cy);
    source.left = std::clamp(
        source.left, 0L, recordable_source_size_.cx - desired_width);
    source.top = std::clamp(
        source.top, 0L, recordable_source_size_.cy - desired_height);
    source.right = source.left + desired_width;
    source.bottom = source.top + desired_height;

    const RECT destination{
        0, 0,
        std::max(1L, zoom_rect_.right - zoom_rect_.left),
        std::max(1L, zoom_rect_.bottom - zoom_rect_.top)};
    DWM_THUMBNAIL_PROPERTIES properties{};
    properties.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_RECTSOURCE |
                         DWM_TNP_VISIBLE | DWM_TNP_OPACITY |
                         DWM_TNP_SOURCECLIENTAREAONLY;
    properties.rcDestination = destination;
    properties.rcSource = source;
    properties.opacity = 255;
    properties.fVisible = TRUE;
    properties.fSourceClientAreaOnly = FALSE;
    if (FAILED(DwmUpdateThumbnailProperties(recordable_thumbnail_, &properties)))
    {
        stop_recordable_output();
        source_initialized_ = false;
    }
}

void ZoomWindow::set_edit_passthrough(bool enabled) {
    if (!window_ || !magnifier_) return;
    auto update_style = [enabled](HWND target) {
        LONG_PTR style = GetWindowLongPtrW(target, GWL_EXSTYLE);
        if (enabled) {
            style |= WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
        } else {
            style &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
        }
        SetWindowLongPtrW(target, GWL_EXSTYLE, style);
        SetWindowPos(target, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                     SWP_FRAMECHANGED);
    };

    // A disabled Magnifier child is skipped by hit testing while it continues
    // to render. The transparent root then exposes the real application below.
    EnableWindow(magnifier_, enabled ? FALSE : TRUE);
    update_style(magnifier_);
    update_style(window_);

    if (enabled && foreground_before_zoom_ &&
        IsWindow(foreground_before_zoom_) &&
        IsWindowVisible(foreground_before_zoom_)) {
        SetForegroundWindow(foreground_before_zoom_);
    }
}

LRESULT CALLBACK ZoomWindow::magnifier_subclass(
        HWND window, UINT message, WPARAM wparam, LPARAM lparam,
        UINT_PTR subclass_id, DWORD_PTR reference_data) {
    auto* zoom = reinterpret_cast<ZoomWindow*>(reference_data);
    if (zoom && zoom->edit_state_ == ZoomEditState::Navigate &&
        message == WM_NCHITTEST) {
        return HTTRANSPARENT;
    }
    if (zoom && (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) &&
        zoom->active_ && !zoom->frozen() &&
        zoom->edit_state_ != ZoomEditState::Navigate) {
        SetFocus(zoom->window_);
        zoom->toggle_freeze();
        return 0;
    }
    if (message == WM_NCDESTROY)
        RemoveWindowSubclass(window, magnifier_subclass, subclass_id);
    return DefSubclassProc(window, message, wparam, lparam);
}

ZoomWindow::~ZoomWindow() {
    stop_recordable_output();
    uninstall_click_hook();
    if (magnifier_)
        RemoveWindowSubclass(magnifier_, magnifier_subclass, 1);
    if (lens_cursor_) DestroyCursor(lens_cursor_);
    if (initialized_ && mag_uninitialize_) mag_uninitialize_();
    if (magnification_module_) FreeLibrary(magnification_module_);
}

bool ZoomWindow::initialize(GraphicsDevice& graphics) {
    if (!load_magnification() || !mag_initialize_()) return false;
    initialized_ = true;
    RECT initial{0, 0, 1, 1};
    if (!create(L"ElitePen.Zoom", L"Zoom — Elite Pen",
                WS_EX_TOPMOST | WS_EX_TOOLWINDOW, WS_POPUP, initial)) return false;
    magnifier_ = CreateWindowW(L"Magnifier", L"Elite Pen Magnifier",
                               WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, window_, nullptr,
                               GetModuleHandleW(nullptr), nullptr);
    if (!magnifier_) return false;
    if (!SetWindowSubclass(magnifier_, magnifier_subclass, 1,
                           reinterpret_cast<DWORD_PTR>(this))) return false;
    lens_cursor_dpi_ = GetDpiForWindow(window_);
    lens_cursor_ = create_zoom_lens_cursor(lens_cursor_dpi_);
    ink_ = std::make_unique<ZoomInkWindow>(controller_);
    if (!ink_->initialize(graphics)) return false;
    target_ = std::make_unique<ZoomTargetWindow>(controller_);
    if (!target_->initialize(graphics)) return false;
    lens_frame_ = std::make_unique<ZoomLensFrameWindow>(controller_);
    if (!lens_frame_->initialize(graphics)) return false;
    edit_toolbar_ = std::make_unique<ZoomEditToolbarWindow>(controller_, *this);
    if (!edit_toolbar_->initialize(graphics)) return false;
    // Zoom presentation is intentionally capturable. Native Magnifier remains
    // the general fallback; an OBS-aware DWM path is selected at activation.
    SetWindowDisplayAffinity(window_, WDA_NONE);
    return true;
}

bool ZoomWindow::show_zoom() {
    if (!initialized_ || active_) return initialized_;
    POINT cursor{};
    GetCursorPos(&cursor);
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    monitor_rect_ = info.rcMonitor;
    tool_before_zoom_ = controller_.state().tool;
    foreground_before_zoom_ = nullptr;
    remember_foreground_target();
    start_recordable_output();
    set_edit_passthrough(false);
    if (mag_set_filter_) {
        std::vector<HWND> excluded{window_};
        if (target_) excluded.push_back(target_->hwnd());
        if (lens_frame_) excluded.push_back(lens_frame_->hwnd());
        if (edit_toolbar_) excluded.push_back(edit_toolbar_->hwnd());
        if (controller_.palette()) excluded.push_back(controller_.palette()->hwnd());
        mag_set_filter_(magnifier_, MW_FILTERMODE_EXCLUDE,
                        static_cast<int>(excluded.size()), excluded.data());
    }
    active_ = true;
    edit_state_ = ZoomEditState::Off;
    if (lens_frame_) lens_frame_->hide();
    if (edit_toolbar_) edit_toolbar_->hide();
    overview_ = false;
    entry_animation_anchor_ = cursor;
    presented_factor_ = 1.0F;
    entry_animation_started_ = std::chrono::steady_clock::now();
    entry_animation_active_ =
        static_cast<ZoomView>(controller_.preferences().zoom_view) ==
            ZoomView::Fullscreen &&
        controller_.state().zoom_factor > 1.001F;
    source_initialized_ = false;
    install_click_hook();
    SetTimer(window_, 1, 16, nullptr);
    refresh_source();
    apply_color_effect();
    ShowWindow(window_, SW_SHOW);
    controller_.update_overlay_interaction();
    SetForegroundWindow(window_);
    SetFocus(window_);
    return true;
}

void ZoomWindow::hide_zoom() {
    if (!active_) return;
    controller_.cancel_pending_text();
    set_edit_passthrough(false);
    active_ = false;
    entry_animation_active_ = false;
    presented_factor_ = 1.0F;
    source_initialized_ = false;
    uninstall_click_hook();
    KillTimer(window_, 1);
    update_lens_cursor(false);
    if (target_) target_->hide();
    if (lens_frame_) lens_frame_->hide();
    if (edit_toolbar_) edit_toolbar_->hide();
    edit_state_ = ZoomEditState::Off;
    if (ink_) ink_->hide();
    ShowWindow(window_, SW_HIDE);
    stop_recordable_output();
    controller_.state().tool = tool_before_zoom_;
    controller_.update_overlay_interaction();
    controller_.invalidate_all();
}

bool ZoomWindow::toggle_freeze() {
    if (!active_ || !ink_) return false;
    finish_entry_animation();
    controller_.commit_pending_text();
    if (edit_active()) {
        set_edit_passthrough(false);
        edit_state_ = ZoomEditState::Off;
        if (edit_toolbar_) edit_toolbar_->hide();
        ink_->show_live(zoom_rect_);
        source_initialized_ = false;
        install_click_hook();
        SetTimer(window_, 1, 16, nullptr);
        refresh_source();
    }
    if (ink_->frozen()) {
        ink_->show_live(zoom_rect_);
        source_initialized_ = false;
        install_click_hook();
        SetTimer(window_, 1, 16, nullptr);
        refresh_source();
        controller_.update_overlay_interaction();
        SetForegroundWindow(window_);
        SetFocus(window_);
        return true;
    }
    refresh_source();
    uninstall_click_hook();
    KillTimer(window_, 1);
    update_lens_cursor(false);
    if (target_) target_->hide();
    UpdateWindow(magnifier_);
    DwmFlush();
    if (!ink_->freeze(zoom_rect_, magnifier_)) {
        install_click_hook();
        SetTimer(window_, 1, 16, nullptr);
        if (controller_.palette()) controller_.palette()->show_notification(
            L"No se pudo congelar el zoom",
            L"Windows no entrego la imagen ampliada. El zoom sigue activo.");
        return false;
    }
    controller_.set_tool(Tool::Pen);
    return true;
}

void ZoomWindow::toggle_edit_mode() {
    if (!active_ || !ink_) return;
    finish_entry_animation();
    if (edit_active()) {
        controller_.commit_pending_text();
        set_edit_passthrough(false);
        edit_state_ = ZoomEditState::Off;
        if (edit_toolbar_) edit_toolbar_->hide();
        ink_->show_live(zoom_rect_);
        source_initialized_ = false;
        install_click_hook();
        SetTimer(window_, 1, 16, nullptr);
        refresh_source();
        controller_.update_overlay_interaction();
        SetForegroundWindow(window_);
        SetFocus(window_);
        return;
    }
    if (ink_->frozen()) {
        if (!toggle_freeze()) return;
    }
    refresh_source();
    edit_state_ = ZoomEditState::Navigate;
    uninstall_click_hook();
    SetTimer(window_, 1, 16, nullptr);
    ink_->show_edit(zoom_rect_, source_rect_,
                    overview_ ? 1.0F : controller_.state().zoom_factor,
                    false, magnifier_);
    if (edit_toolbar_) edit_toolbar_->show_for(
        zoom_rect_, overview_ ? 1.0F : controller_.state().zoom_factor,
        edit_state_);
    controller_.update_overlay_interaction();
    set_edit_passthrough(true);
}

void ZoomWindow::set_edit_state(ZoomEditState state) {
    if (!active_ || !ink_ || state == ZoomEditState::Off) {
        if (state == ZoomEditState::Off && edit_active()) toggle_edit_mode();
        return;
    }
    if (!edit_active()) {
        toggle_edit_mode();
        if (!edit_active()) return;
    }
    if (state == ZoomEditState::Annotate) {
        remember_foreground_target();
        set_edit_passthrough(false);
        refresh_source();
        edit_anchor_ = {
            source_rect_.left + (source_rect_.right - source_rect_.left) / 2,
            source_rect_.top + (source_rect_.bottom - source_rect_.top) / 2};
        edit_state_ = state;
        uninstall_click_hook();
        KillTimer(window_, 1);
        update_lens_cursor(false);
        if (target_) target_->hide();
        // Capture only the magnified content. Explicitly hiding the toolbar
        // also protects older Windows 10 builds where display-affinity
        // exclusion can be unavailable or ignored by a screen DC.
        if (edit_toolbar_) edit_toolbar_->hide();
        if (!ink_->show_edit(
                zoom_rect_, source_rect_,
                overview_ ? 1.0F : controller_.state().zoom_factor,
                true, magnifier_)) {
            edit_state_ = ZoomEditState::Navigate;
            uninstall_click_hook();
            SetTimer(window_, 1, 16, nullptr);
            source_initialized_ = false;
            refresh_source();
            set_edit_passthrough(true);
            if (controller_.palette()) controller_.palette()->show_notification(
                L"No se pudo congelar Zoom editable",
                L"Windows no entregó el cuadro ampliado. Puedes seguir navegando.");
        }
        if (controller_.state().tool == Tool::Interact ||
            controller_.state().tool == Tool::Zoom) {
            if (edit_state_ != ZoomEditState::Annotate) {
                if (edit_toolbar_) edit_toolbar_->show_for(
                    zoom_rect_, overview_ ? 1.0F : controller_.state().zoom_factor,
                    edit_state_);
                controller_.update_overlay_interaction();
                return;
            }
            controller_.set_tool(Tool::Pen);
        }
    } else {
        // Text is an in-place annotation. Finish it while the source-space
        // document is still the active destination, before returning to MANO.
        controller_.commit_pending_text();
        edit_state_ = state;
        ink_->set_edit_annotating(false);
        source_initialized_ = false;
        uninstall_click_hook();
        SetTimer(window_, 1, 16, nullptr);
        refresh_source();
        set_edit_passthrough(true);
    }
    if (edit_toolbar_) edit_toolbar_->show_for(
        zoom_rect_, overview_ ? 1.0F : controller_.state().zoom_factor,
        edit_state_);
    controller_.update_overlay_interaction();
}

void ZoomWindow::adjust_zoom(float delta) {
    if (!active_) return;
    finish_entry_animation();
    if (edit_state_ == ZoomEditState::Annotate)
        set_edit_state(ZoomEditState::Navigate);
    controller_.state().zoom_factor = std::clamp(
        controller_.state().zoom_factor + delta, 1.25F, 8.0F);
    overview_ = false;
    source_initialized_ = false;
    refresh_source();
    controller_.preferences().zoom_factor = controller_.state().zoom_factor;
    controller_.save_preferences();
    if (edit_toolbar_ && edit_active()) edit_toolbar_->show_for(
        zoom_rect_, controller_.state().zoom_factor, edit_state_);
}

void ZoomWindow::set_lens_diameter(int diameter) {
    diameter = std::clamp(diameter, 280, 960);
    if (controller_.preferences().zoom_lens_diameter == diameter) return;
    controller_.preferences().zoom_lens_diameter = diameter;
    controller_.save_preferences();
    if (!active_ || frozen() ||
        static_cast<ZoomView>(controller_.preferences().zoom_view) != ZoomView::Lens) {
        return;
    }
    if (edit_state_ == ZoomEditState::Annotate)
        set_edit_state(ZoomEditState::Navigate);
    source_initialized_ = false;
    refresh_source();
    controller_.update_overlay_interaction();
}

void ZoomWindow::adjust_lens_diameter(int direction) {
    if (direction == 0) return;
    const int current = controller_.preferences().zoom_lens_diameter;
    int next = current;
    if (direction > 0) {
        const auto candidate = std::upper_bound(
            kZoomLensDiameters.begin(), kZoomLensDiameters.end(), current);
        next = candidate == kZoomLensDiameters.end()
            ? kZoomLensDiameters.back() : *candidate;
    } else {
        const auto candidate = std::lower_bound(
            kZoomLensDiameters.begin(), kZoomLensDiameters.end(), current);
        next = candidate == kZoomLensDiameters.begin()
            ? kZoomLensDiameters.front() : *(candidate - 1);
    }
    set_lens_diameter(next);
}

void ZoomWindow::execute_action(HotkeyAction action) {
    if (!active_) return;
    switch (action) {
        case HotkeyAction::ZoomFreeze:
            toggle_freeze();
            break;
        case HotkeyAction::ZoomFullscreen:
            finish_entry_animation();
            if (edit_state_ == ZoomEditState::Annotate)
                set_edit_state(ZoomEditState::Navigate);
            controller_.preferences().zoom_view = static_cast<int>(ZoomView::Fullscreen);
            controller_.save_preferences();
            refresh_source();
            controller_.update_overlay_interaction();
            break;
        case HotkeyAction::ZoomLens:
            finish_entry_animation();
            if (edit_state_ == ZoomEditState::Annotate)
                set_edit_state(ZoomEditState::Navigate);
            controller_.preferences().zoom_view = static_cast<int>(ZoomView::Lens);
            controller_.save_preferences();
            refresh_source();
            controller_.update_overlay_interaction();
            break;
        case HotkeyAction::ZoomDocked:
            finish_entry_animation();
            if (edit_state_ == ZoomEditState::Annotate)
                set_edit_state(ZoomEditState::Navigate);
            controller_.preferences().zoom_view = static_cast<int>(ZoomView::Docked);
            controller_.save_preferences();
            refresh_source();
            controller_.update_overlay_interaction();
            break;
        case HotkeyAction::ZoomCycleView:
            if (edit_active()) {
                // Space is the quick, non-destructive way back to MANO. Entering
                // annotation remains an intentional click or LAPIZ command so a
                // navigation keystroke can never freeze the view by accident.
                if (edit_state_ == ZoomEditState::Annotate)
                    set_edit_state(ZoomEditState::Navigate);
            } else {
                cycle_view();
            }
            break;
        case HotkeyAction::ZoomInvert:
            controller_.preferences().zoom_invert = !controller_.preferences().zoom_invert;
            controller_.save_preferences();
            apply_color_effect();
            break;
        case HotkeyAction::ZoomOverview:
            finish_entry_animation();
            if (edit_state_ == ZoomEditState::Annotate)
                set_edit_state(ZoomEditState::Navigate);
            overview_ = !overview_;
            refresh_source();
            break;
        case HotkeyAction::ZoomIn:
            adjust_zoom(0.25F);
            break;
        case HotkeyAction::ZoomOut:
            adjust_zoom(-0.25F);
            break;
        case HotkeyAction::ZoomEdit:
            toggle_edit_mode();
            break;
        default:
            break;
    }
}

void ZoomWindow::bring_to_front() {
    if (!active_) return;
    // Keep a deterministic topmost stack. Raising the native Magnifier root
    // after the ink makes completed strokes appear to vanish even though they
    // remain in the zoom document.
    SetWindowPos(window_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (ink_) ink_->bring_to_front();
    if (lens_frame_) lens_frame_->bring_to_front();
    if (target_) target_->bring_to_front();
    if (edit_toolbar_) edit_toolbar_->bring_to_front();
    if (controller_.palette() && IsWindowVisible(controller_.palette()->hwnd())) {
        SetWindowPos(controller_.palette()->hwnd(), HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void ZoomWindow::invalidate_ink() {
    if (ink_) ink_->invalidate();
}

void ZoomWindow::clear_annotations() {
    if (ink_) ink_->clear_annotations();
}

bool ZoomWindow::undo() { return ink_ && ink_->undo(); }

bool ZoomWindow::redo() { return ink_ && ink_->redo(); }

void ZoomWindow::commit_text_screen(PointF position, Color color, float thickness,
                                    std::wstring text) {
    if (ink_ && ink_->accepts_annotations())
        ink_->commit_text_screen(position, color, thickness, std::move(text));
}

void ZoomWindow::apply_color_effect() {
    if (!mag_set_color_effect_ || !magnifier_) return;
    MAGCOLOREFFECT effect{};
    if (controller_.preferences().zoom_invert) {
        const MAGCOLOREFFECT inverted{{
            {-1.0F, 0, 0, 0, 0},
            {0, -1.0F, 0, 0, 0},
            {0, 0, -1.0F, 0, 0},
            {0, 0, 0, 1.0F, 0},
            {1.0F, 1.0F, 1.0F, 0, 1.0F}}};
        effect = inverted;
    } else {
        const MAGCOLOREFFECT identity{{
            {1.0F, 0, 0, 0, 0},
            {0, 1.0F, 0, 0, 0},
            {0, 0, 1.0F, 0, 0},
            {0, 0, 0, 1.0F, 0},
            {0, 0, 0, 0, 1.0F}}};
        effect = identity;
    }
    mag_set_color_effect_(magnifier_, &effect);
}

void ZoomWindow::cycle_view() {
    finish_entry_animation();
    controller_.preferences().zoom_view =
        (controller_.preferences().zoom_view + 1) % 3;
    controller_.save_preferences();
    refresh_source();
    controller_.update_overlay_interaction();
}

void ZoomWindow::apply_view_regions(ZoomView view, int width, int height) {
    if (region_view_ == static_cast<int>(view) && region_width_ == width &&
        region_height_ == height) return;
    region_view_ = static_cast<int>(view);
    region_width_ = width;
    region_height_ = height;
    const bool circular = view == ZoomView::Lens;
    auto apply = [circular, width, height](HWND target) {
        if (!target) return;
        if (!circular) {
            SetWindowRgn(target, nullptr, TRUE);
            return;
        }
        HRGN region = CreateEllipticRgn(0, 0, width + 1, height + 1);
        if (!region) return;
        // After a successful SetWindowRgn call Windows owns the region.
        if (!SetWindowRgn(target, region, TRUE)) DeleteObject(region);
    };
    apply(window_);
    if (ink_) apply(ink_->hwnd());
}

void ZoomWindow::finish_entry_animation() {
    if (!entry_animation_active_) return;
    entry_animation_active_ = false;
    presented_factor_ = overview_ ? 1.0F : controller_.state().zoom_factor;
    // Force the next refresh to present the exact configured factor before an
    // immediate P, E, wheel or view command acts on the zoomed frame.
    source_initialized_ = false;
}

void ZoomWindow::update_lens_cursor(bool active) {
    if (!active) {
        if (lens_cursor_active_ && GetCursor() == lens_cursor_)
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        lens_cursor_active_ = false;
        return;
    }
    const UINT dpi = GetDpiForWindow(window_);
    if (!lens_cursor_ || dpi != lens_cursor_dpi_) {
        HCURSOR replacement = create_zoom_lens_cursor(dpi);
        if (replacement) {
            if (lens_cursor_) DestroyCursor(lens_cursor_);
            lens_cursor_ = replacement;
            lens_cursor_dpi_ = dpi;
        }
    }
    if (lens_cursor_) {
        SetCursor(lens_cursor_);
        lens_cursor_active_ = true;
    }
}

void ZoomWindow::apply_theme() {
    const bool restore_cursor = lens_cursor_active_;
    update_lens_cursor(false);
    if (lens_cursor_) DestroyCursor(lens_cursor_);
    lens_cursor_ = nullptr;
    lens_cursor_dpi_ = 0;
    if (restore_cursor) update_lens_cursor(true);
    if (ink_) ink_->invalidate();
    if (target_) target_->invalidate();
    if (lens_frame_) lens_frame_->invalidate();
    if (edit_toolbar_) edit_toolbar_->invalidate();
}

void ZoomWindow::refresh_source() {
    if (!active_ || (ink_ && ink_->frozen())) return;
    const bool edit_locked = edit_state_ == ZoomEditState::Annotate;
    POINT cursor{};
    if (edit_locked) {
        cursor = edit_anchor_;
    } else if (!GetCursorPos(&cursor)) {
        if (source_initialized_) {
            cursor = last_source_cursor_;
        } else {
            const int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
            const int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
            cursor = {left + GetSystemMetrics(SM_CXVIRTUALSCREEN) / 2,
                      top + GetSystemMetrics(SM_CYVIRTUALSCREEN) / 2};
        }
    }
    const auto view = static_cast<ZoomView>(controller_.preferences().zoom_view);
    const float target_factor = overview_ ? 1.0F : controller_.state().zoom_factor;
    float factor = target_factor;
    bool entry_frame = false;
    if (entry_animation_active_ && view == ZoomView::Fullscreen && !overview_) {
        entry_frame = true;
        cursor = entry_animation_anchor_;
        const auto elapsed = std::chrono::steady_clock::now() -
                             entry_animation_started_;
        const float progress = std::chrono::duration<float>(elapsed).count() /
            std::chrono::duration<float>(kZoomEntryDuration).count();
        factor = zoom_entry_factor(1.0F, target_factor, progress);
        if (progress >= 1.0F) {
            factor = target_factor;
            entry_animation_active_ = false;
        }
    } else if (entry_animation_active_) {
        entry_animation_active_ = false;
    }
    presented_factor_ = factor;
    if (source_initialized_ && cursor.x == last_source_cursor_.x &&
        cursor.y == last_source_cursor_.y &&
        static_cast<int>(view) == last_source_view_ &&
        factor == last_source_factor_ && overview_ == last_source_overview_) {
        return;
    }
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    monitor_rect_ = info.rcMonitor;
    update_lens_cursor(view == ZoomView::Lens && !edit_locked);
    const int monitor_width = monitor_rect_.right - monitor_rect_.left;
    const int monitor_height = monitor_rect_.bottom - monitor_rect_.top;
    RECT zoom_rect = edit_locked ? zoom_rect_ : monitor_rect_;
    if (!edit_locked && view == ZoomView::Lens) {
        const int maximum_diameter = std::max(
            1, std::min(monitor_width, monitor_height) - 48);
        const int minimum_diameter = std::min(280, maximum_diameter);
        const int lens_diameter = std::clamp(
            controller_.preferences().zoom_lens_diameter,
            minimum_diameter, maximum_diameter);
        const int lens_width = lens_diameter;
        const int lens_height = lens_diameter;
        int left = cursor.x + 36;
        int top = cursor.y + 36;
        if (left + lens_width > monitor_rect_.right) left = cursor.x - lens_width - 36;
        if (top + lens_height > monitor_rect_.bottom) top = cursor.y - lens_height - 36;
        left = std::clamp(left, static_cast<int>(monitor_rect_.left),
                          static_cast<int>(monitor_rect_.right) - lens_width);
        top = std::clamp(top, static_cast<int>(monitor_rect_.top),
                         static_cast<int>(monitor_rect_.bottom) - lens_height);
        zoom_rect = {left, top, left + lens_width, top + lens_height};
    } else if (!edit_locked && view == ZoomView::Docked) {
        const int dock_height = std::min(360, std::max(220, monitor_height / 3));
        zoom_rect.bottom = zoom_rect.top + dock_height;
    }
    const int zoom_width = zoom_rect.right - zoom_rect.left;
    const int zoom_height = zoom_rect.bottom - zoom_rect.top;
    const bool geometry_changed = !source_initialized_ ||
                                  !EqualRect(&zoom_rect_, &zoom_rect);
    zoom_rect_ = zoom_rect;
    if (geometry_changed) {
        // Resize independently from the transient topmost ordering. Using a
        // sibling as hWndInsertAfter can make Windows accept the Z-order part
        // while leaving a newly-created Magnifier root at its 1x1 bootstrap
        // size. Restack the surfaces explicitly after geometry is established.
        SetWindowPos(window_, HWND_TOPMOST, zoom_rect.left, zoom_rect.top,
                     zoom_width, zoom_height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        SetWindowPos(magnifier_, nullptr, 0, 0, zoom_width, zoom_height,
                     SWP_NOZORDER | SWP_SHOWWINDOW);
        apply_view_regions(view, zoom_width, zoom_height);
    }
    if (lens_frame_) {
        if (view == ZoomView::Lens) lens_frame_->show_for(zoom_rect_);
        else lens_frame_->hide();
    }

    if (!recordable_output_ &&
        (!source_initialized_ || factor != last_source_factor_)) {
        MAGTRANSFORM transform{{{factor, 0, 0}, {0, factor, 0}, {0, 0, 1}}};
        mag_set_transform_(magnifier_, &transform);
    }
    const int source_width = std::max(1, static_cast<int>(
        static_cast<float>(zoom_width) / factor));
    const int source_height = std::max(1, static_cast<int>(
        static_cast<float>(zoom_height) / factor));
    int left = cursor.x - source_width / 2;
    int top = cursor.y - source_height / 2;
    left = std::clamp(left, static_cast<int>(monitor_rect_.left),
                      static_cast<int>(monitor_rect_.right) - source_width);
    top = std::clamp(top, static_cast<int>(monitor_rect_.top),
                     static_cast<int>(monitor_rect_.bottom) - source_height);
    source_rect_ = RECT{left, top, left + source_width, top + source_height};
    if (recordable_output_) {
        update_recordable_output();
    } else {
        mag_set_source_(magnifier_, source_rect_);
        InvalidateRect(magnifier_, nullptr, TRUE);
    }
    if (ink_ && IsWindowVisible(ink_->hwnd())) {
        if (edit_active()) {
            ink_->update_edit_view(zoom_rect_, source_rect_, factor);
        } else if (geometry_changed) {
            ink_->set_bounds(zoom_rect_);
        }
    }
    if (target_) {
        if (view == ZoomView::Lens && !edit_locked) {
            // The source rectangle can be clamped at monitor edges. Its center,
            // not the unclamped pointer, is the pixel represented at the center
            // of the magnified output.
            const POINT source_focus{
                source_rect_.left + (source_rect_.right - source_rect_.left) / 2,
                source_rect_.top + (source_rect_.bottom - source_rect_.top) / 2};
            target_->show_at(source_focus);
        }
        else target_->hide();
    }
    source_initialized_ = true;
    last_source_cursor_ = cursor;
    last_source_factor_ = factor;
    last_source_view_ = static_cast<int>(view);
    last_source_overview_ = overview_;
    if (edit_toolbar_ && edit_active())
        edit_toolbar_->show_for(zoom_rect_, factor, edit_state_);
    if (geometry_changed) bring_to_front();
    // The transition stays anchored to the invocation point. Once the final
    // frame lands, invalidate the cursor cache so live following resumes on the
    // next 16 ms tick even when the pointer moved during those few frames.
    if (entry_frame && !entry_animation_active_) source_initialized_ = false;
}

LRESULT ZoomWindow::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_NCHITTEST:
            if (edit_state_ == ZoomEditState::Navigate) return HTTRANSPARENT;
            break;
        case kQaQueryZoomFrozenMessage:
            return frozen() ? 1 : 0;
        case kQaQueryZoomSourceFocusXMessage:
            return source_rect_.left + (source_rect_.right - source_rect_.left) / 2;
        case kQaQueryZoomSourceFocusYMessage:
            return source_rect_.top + (source_rect_.bottom - source_rect_.top) / 2;
        case kQaQueryZoomViewMessage:
            return controller_.preferences().zoom_view;
        case kQaQueryZoomGeometryWidthMessage:
            return zoom_rect_.right - zoom_rect_.left;
        case kQaQueryZoomGeometryHeightMessage:
            return zoom_rect_.bottom - zoom_rect_.top;
        case kQaQueryZoomLensDiameterMessage:
            return controller_.preferences().zoom_lens_diameter;
        case kQaQueryZoomFactorMessage:
            return static_cast<LRESULT>(std::lround(
                controller_.state().zoom_factor * 100.0F));
        case kQaQueryZoomPresentedFactorMessage:
            return static_cast<LRESULT>(std::lround(presented_factor_ * 100.0F));
        case kQaQueryZoomEntryAnimationMessage:
            return entry_animation_active_ ? 1 : 0;
        case kQaQueryZoomEditStateMessage:
            return static_cast<LRESULT>(edit_state_);
        case kQaQueryZoomEditDocumentCountMessage:
            return ink_ ? static_cast<LRESULT>(ink_->item_count()) : 0;
        case kQaSetZoomEditStateMessage:
            if (wparam == 0) {
                if (edit_active()) toggle_edit_mode();
            } else {
                set_edit_state(wparam == 2 ? ZoomEditState::Annotate
                                           : ZoomEditState::Navigate);
            }
            return static_cast<LRESULT>(edit_state_);
        case kQaPopulateZoomEditDocumentMessage:
            if (ink_ && edit_active()) {
                ink_->populate_edit_stress_document(
                    std::clamp<std::size_t>(static_cast<std::size_t>(wparam),
                                            1U, 20000U));
                return static_cast<LRESULT>(ink_->item_count());
            }
            return 0;
        case kQaToggleZoomFreezeMessage:
            return toggle_freeze() ? 1 : 0;
        case kZoomClickFreezeMessage:
            click_freeze_pending_ = false;
            if (active_ && !frozen() &&
                edit_state_ != ZoomEditState::Navigate) toggle_freeze();
            return 0;
        case kQaQueryZoomEditFirstViewXMessage:
            if (ink_) {
                if (const auto point = ink_->first_edit_view_point())
                    return static_cast<LRESULT>(std::lround(point->x));
            }
            return std::numeric_limits<LRESULT>::min();
        case kQaQueryZoomEditFirstViewYMessage:
            if (ink_) {
                if (const auto point = ink_->first_edit_view_point())
                    return static_cast<LRESULT>(std::lround(point->y));
            }
            return std::numeric_limits<LRESULT>::min();
        case kQaNudgeZoomEditSourceMessage:
            if (ink_ && edit_active()) {
                const int width = source_rect_.right - source_rect_.left;
                const int height = source_rect_.bottom - source_rect_.top;
                int left = source_rect_.left + static_cast<int>(wparam);
                int top = source_rect_.top + static_cast<int>(lparam);
                left = std::clamp(left, static_cast<int>(monitor_rect_.left),
                                  static_cast<int>(monitor_rect_.right) - width);
                top = std::clamp(top, static_cast<int>(monitor_rect_.top),
                                 static_cast<int>(monitor_rect_.bottom) - height);
                source_rect_ = {left, top, left + width, top + height};
                // QA isolates Elite Pen's added source-space render cost. The
                // native Magnifier update is already exercised by refresh_source
                // and must not be charged again to the annotation cache budget.
                ink_->update_edit_view(
                    zoom_rect_, source_rect_,
                    overview_ ? 1.0F : controller_.state().zoom_factor);
                return 1;
            }
            return 0;
        case WM_TIMER: refresh_source(); return 0;
        case WM_DISPLAYCHANGE:
            source_initialized_ = false;
            refresh_source();
            return 0;
        case WM_MOUSEWHEEL: {
            const bool control = (GET_KEYSTATE_WPARAM(wparam) & MK_CONTROL) != 0 ||
                                 (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            const bool shift = (GET_KEYSTATE_WPARAM(wparam) & MK_SHIFT) != 0 ||
                               (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            if (control && shift) {
                controller_.adjust_thickness_step(
                    GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 1 : -1);
                return 0;
            }
            if (shift && !frozen() &&
                static_cast<ZoomView>(controller_.preferences().zoom_view) ==
                    ZoomView::Lens) {
                adjust_lens_diameter(
                    GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 1 : -1);
                return 0;
            }
            adjust_zoom(GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 0.25F : -0.25F);
            return 0;
        }
        case WM_PARENTNOTIFY:
            if (LOWORD(wparam) == WM_LBUTTONDOWN && !frozen() &&
                edit_state_ != ZoomEditState::Navigate) {
                toggle_freeze();
                return 0;
            }
            break;
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE || wparam == VK_F4) {
                controller_.toggle_zoom();
                return 0;
            }
            for (std::size_t index = kGlobalHotkeyActionCount;
                 index < kHotkeyActionCount; ++index) {
                const auto action = static_cast<HotkeyAction>(index);
                if (controller_.matches_hotkey(action, wparam)) {
                    execute_action(action);
                    return 0;
                }
            }
            if (wparam == 'M') { cycle_view(); return 0; }
            if (wparam == VK_OEM_4) { adjust_lens_diameter(-1); return 0; }
            if (wparam == VK_OEM_6) { adjust_lens_diameter(1); return 0; }
            if (wparam == VK_ADD) { execute_action(HotkeyAction::ZoomIn); return 0; }
            if (wparam == VK_SUBTRACT) { execute_action(HotkeyAction::ZoomOut); return 0; }
            break;
        case WM_KILLFOCUS:
            // Keep zoom active; global shortcut or Esc returns safely.
            return 0;
        case WM_RBUTTONDOWN:
            controller_.toggle_zoom();
            return 0;
        default: break;
    }
    return WindowBase::handle_message(message, wparam, lparam);
}

BOOL CALLBACK Controller::collect_monitor(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
    auto* rectangles = reinterpret_cast<std::vector<RECT>*>(data);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)) rectangles->push_back(info.rcMonitor);
    return TRUE;
}

void Controller::create_overlays(std::wstring& error) {
    std::vector<RECT> rectangles;
    EnumDisplayMonitors(nullptr, nullptr, &Controller::collect_monitor,
                        reinterpret_cast<LPARAM>(&rectangles));
    if (rectangles.empty()) {
        rectangles.push_back({0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)});
    }
    for (const auto& rectangle : rectangles) {
        auto overlay = std::make_unique<OverlayWindow>(*this, rectangle);
        if (!overlay->initialize(graphics_)) {
            error = L"No se pudo crear una superposicion para uno de los monitores.";
            return;
        }
        overlays_.push_back(std::move(overlay));
    }
}

bool Controller::initialize(std::wstring& error) {
    preferences_ = preferences_store_.load();
    g_ui_theme = preferences_.theme;
    state_.color = preferences_.color;
    state_.thickness = preferences_.thickness;
    state_.zoom_factor = preferences_.zoom_factor;
    state_.tool = preferences_.start_in_interact_mode ? Tool::Interact : Tool::Pen;
    state_.cursor_highlight = preferences_.highlight_cursor;
    if (!graphics_.initialize(error)) return false;
    create_overlays(error);
    if (!error.empty()) return false;
    palette_ = std::make_unique<PaletteWindow>(*this);
    if (!palette_->initialize(graphics_)) {
        error = L"No se pudo crear la paleta de Elite Pen.";
        return false;
    }
    if (preferences_.has_palette_position) {
        POINT saved{preferences_.palette_x, preferences_.palette_y};
        HMONITOR monitor = MonitorFromPoint(saved, MONITOR_DEFAULTTONULL);
        if (monitor) {
            MONITORINFO info{};
            info.cbSize = sizeof(info);
            GetMonitorInfoW(monitor, &info);
            const int x = std::clamp(preferences_.palette_x,
                                     static_cast<int>(info.rcWork.left),
                                     static_cast<int>(info.rcWork.right) - palette_->pixel_width());
            const int y = std::clamp(preferences_.palette_y,
                                     static_cast<int>(info.rcWork.top),
                                     static_cast<int>(info.rcWork.bottom) - palette_->pixel_height());
            SetWindowPos(palette_->hwnd(), HWND_TOPMOST, x, y, 0, 0,
                         SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }
    colors_ = std::make_unique<ColorWindow>(*this);
    if (!colors_->initialize(graphics_)) {
        error = L"No se pudo crear el selector de colores.";
        return false;
    }
    tools_ = std::make_unique<ToolWindow>(*this);
    if (!tools_->initialize(graphics_)) {
        error = L"No se pudo crear el selector de herramientas.";
        return false;
    }
    text_input_ = std::make_unique<TextInputWindow>(*this);
    if (!text_input_->initialize(graphics_)) {
        error = L"No se pudo crear el editor de texto.";
        return false;
    }
    settings_ = std::make_unique<SettingsWindow>(*this);
    if (!settings_->initialize()) {
        error = L"No se pudo crear la ventana de configuracion.";
        return false;
    }
    zoom_ = std::make_unique<ZoomWindow>(*this);
    if (!zoom_->initialize(graphics_)) {
        // Zoom failure is isolated: drawing remains available and an error is shown on use.
        zoom_.reset();
    }
    invalidate_all();
    return true;
}

int Controller::message_loop() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

void Controller::shutdown() {
    if (shutting_down_) return;
    shutting_down_ = true;
    save_palette_position();
    preferences_.zoom_factor = state_.zoom_factor;
    preferences_.thickness = state_.thickness;
    preferences_.color = state_.color;
    preferences_store_.save(preferences_);
    if (zoom_) zoom_->hide_zoom();
    settings_.reset();
    text_input_.reset();
    tools_.reset();
    colors_.reset();
    overlays_.clear();
    palette_.reset();
    zoom_.reset();
}

void Controller::invalidate_document() {
    for (const auto& overlay : overlays_) overlay->invalidate();
}

void Controller::invalidate_preview() {
    if (!preview_) {
        invalidate_document();
        return;
    }
    const RectF bounds = preview_->bounds();
    for (const auto& overlay : overlays_) {
        const RECT& monitor = overlay->monitor_rect();
        const RectF viewport{static_cast<float>(monitor.left),
                             static_cast<float>(monitor.top),
                             static_cast<float>(monitor.right),
                             static_cast<float>(monitor.bottom)};
        if (bounds.intersects(viewport)) overlay->invalidate();
    }
}

void Controller::invalidate_all() {
    invalidate_document();
    if (palette_) palette_->invalidate();
    if (zoom_) zoom_->invalidate_ink();
}

void Controller::restack_palette() {
    if (!palette_) return;
    SetWindowPos(palette_->hwnd(), HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void Controller::update_overlay_interaction() {
    for (const auto& overlay : overlays_) overlay->update_interaction();
    restack_zoom();
    // Every overlay is topmost while drawing, so explicitly restore the palette
    // above them. Its controls must remain selectable in every tool mode.
    restack_palette();
}

bool Controller::route_palette_command(PointF screen_point) {
    if (!palette_) return false;
    POINT point{static_cast<LONG>(std::lround(screen_point.x)),
                static_cast<LONG>(std::lround(screen_point.y))};
    if (!palette_->contains_screen_point(point)) return false;
    ScreenToClient(palette_->hwnd(), &point);
    const bool activated = palette_->activate_command_at(point);
    if (activated) update_overlay_interaction();
    return activated;
}

void Controller::set_tool(Tool tool) {
    if (text_input_ && text_input_->active() && tool != Tool::Text) text_input_->commit();
    if (tool == Tool::Zoom) {
        toggle_zoom();
        return;
    }
    state_.tool = tool;
    close_panels();
    update_overlay_interaction();
    if (palette_) palette_->invalidate();
}

void Controller::set_color(Color color) {
    state_.color = color;
    preferences_.color = color;
    if (state_.tool == Tool::Interact || state_.tool == Tool::Eraser) set_tool(Tool::Pen);
    if (text_input_) text_input_->update_style(state_.color, state_.thickness);
    if (palette_) palette_->invalidate();
}

void Controller::set_thickness(float thickness) {
    state_.thickness = thickness;
    preferences_.thickness = thickness;
    if (text_input_) text_input_->update_style(state_.color, state_.thickness);
    if (palette_) palette_->invalidate();
}

void Controller::adjust_thickness_step(int direction) {
    float next = state_.thickness;
    if (direction > 0) {
        const auto found = std::upper_bound(
            kThicknessSteps.begin(), kThicknessSteps.end(), state_.thickness + 0.001F);
        next = found == kThicknessSteps.end() ? kThicknessSteps.back() : *found;
    } else if (direction < 0) {
        const auto found = std::lower_bound(
            kThicknessSteps.begin(), kThicknessSteps.end(), state_.thickness - 0.001F);
        next = found == kThicknessSteps.begin() ? kThicknessSteps.front() : *(found - 1);
    }
    if (std::abs(next - state_.thickness) < 0.001F) return;
    set_thickness(next);
    save_preferences();
}

void Controller::toggle_visibility() {
    if (text_input_ && text_input_->active()) text_input_->commit();
    state_.annotations_visible = !state_.annotations_visible;
    invalidate_all();
}

void Controller::toggle_whiteboard() {
    if (text_input_ && text_input_->active()) text_input_->commit();
    state_.whiteboard = !state_.whiteboard;
    if (state_.whiteboard) state_.blackboard = false;
    if (state_.whiteboard && state_.color == Color{255, 255, 255, 255}) {
        state_.color = kBlack;
        preferences_.color = kBlack;
    }
    if (state_.whiteboard && state_.tool == Tool::Interact) state_.tool = Tool::Pen;
    if (text_input_) text_input_->update_style(state_.color, state_.thickness);
    update_overlay_interaction();
    invalidate_all();
}

void Controller::toggle_blackboard() {
    if (text_input_ && text_input_->active()) text_input_->commit();
    state_.blackboard = !state_.blackboard;
    if (state_.blackboard) state_.whiteboard = false;
    if (state_.blackboard && state_.color == kBlack) {
        state_.color = kYellow;
        preferences_.color = kYellow;
    }
    if (state_.blackboard && state_.tool == Tool::Interact) state_.tool = Tool::Pen;
    if (text_input_) text_input_->update_style(state_.color, state_.thickness);
    update_overlay_interaction();
    invalidate_all();
}

void Controller::clear_document() {
    if (text_input_ && text_input_->active()) text_input_->cancel();
    if (zoom_ && zoom_->active()) {
        if (zoom_->annotations_empty()) return;
        if (preferences_.confirm_clear &&
            MessageBoxW(palette_ ? palette_->hwnd() : nullptr,
                        L"¿Limpiar las anotaciones de esta sesión de zoom? "
                        L"Podrás deshacer la acción.",
                        L"Limpiar zoom — Elite Pen",
                        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
        zoom_->clear_annotations();
        return;
    }
    if (state_.document.empty() && transient_drawables_.empty()) return;
    if (preferences_.confirm_clear &&
        MessageBoxW(palette_ ? palette_->hwnd() : nullptr,
                    L"¿Limpiar todas las anotaciones? Podras deshacer la accion.",
                    L"Limpiar — Elite Pen", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) {
        return;
    }
    const bool cleared = state_.document.clear();
    const bool cleared_transient = !transient_drawables_.empty();
    transient_drawables_.clear();
    if (cleared || cleared_transient) invalidate_document();
}

void Controller::undo() {
    if (zoom_ && zoom_->active()) {
        zoom_->undo();
        return;
    }
    if (state_.document.undo()) invalidate_document();
}

void Controller::redo() {
    if (zoom_ && zoom_->active()) {
        zoom_->redo();
        return;
    }
    if (state_.document.redo()) invalidate_document();
}

void Controller::toggle_zoom() {
    if (text_input_ && text_input_->active()) text_input_->commit();
    if (!zoom_) {
        MessageBoxW(palette_ ? palette_->hwnd() : nullptr,
                    L"La ampliacion nativa de Windows no esta disponible en este equipo.",
                    L"Elite Pen", MB_OK | MB_ICONWARNING);
        return;
    }
    if (zoom_->active()) zoom_->hide_zoom();
    else zoom_->show_zoom();
    if (palette_) {
        SetWindowPos(palette_->hwnd(), HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        palette_->invalidate();
    }
}

void Controller::toggle_zoom_freeze() {
    if (zoom_ && zoom_->active()) zoom_->toggle_freeze();
}

void Controller::toggle_color_panel() {
    if (!colors_ || !palette_) return;
    const bool was_visible = colors_->visible();
    close_panels();
    if (!was_visible) colors_->toggle_near(palette_->hwnd());
}

void Controller::toggle_tool_panel() {
    if (!tools_ || !palette_) return;
    const bool was_visible = tools_->visible() && !tools_->geometry_only();
    close_panels();
    if (!was_visible) tools_->toggle_near(palette_->hwnd());
}

void Controller::toggle_geometry_panel() {
    if (!tools_ || !palette_) return;
    const bool was_visible = tools_->visible() && tools_->geometry_only();
    close_panels();
    if (!was_visible) tools_->toggle_geometry_near(palette_->hwnd());
}

void Controller::execute_hotkey(HotkeyAction action) {
    switch (action) {
        case HotkeyAction::Interact:
            set_tool(state_.tool == Tool::Interact ? Tool::Pen : Tool::Interact);
            break;
        case HotkeyAction::Visibility: toggle_visibility(); break;
        case HotkeyAction::Whiteboard: toggle_whiteboard(); break;
        case HotkeyAction::Undo: undo(); break;
        case HotkeyAction::Redo: redo(); break;
        case HotkeyAction::Clear: clear_document(); break;
        case HotkeyAction::Zoom: toggle_zoom(); break;
        case HotkeyAction::Blackboard: toggle_blackboard(); break;
        case HotkeyAction::Pen: set_tool(Tool::Pen); break;
        case HotkeyAction::Highlighter: set_tool(Tool::Highlighter); break;
        case HotkeyAction::Eraser: set_tool(Tool::Eraser); break;
        case HotkeyAction::Line: set_tool(Tool::Line); break;
        case HotkeyAction::Rectangle: set_tool(Tool::Rectangle); break;
        case HotkeyAction::Ellipse: set_tool(Tool::Ellipse); break;
        case HotkeyAction::Arrow: set_tool(Tool::Arrow); break;
        case HotkeyAction::CurvedArrow: set_tool(Tool::CurvedArrow); break;
        case HotkeyAction::Text: set_tool(Tool::Text); break;
        case HotkeyAction::Screenshot: set_tool(Tool::Screenshot); break;
        case HotkeyAction::ColorPanel: toggle_color_panel(); break;
        case HotkeyAction::GeometryPanel: toggle_geometry_panel(); break;
        case HotkeyAction::ToolPanel: toggle_tool_panel(); break;
        case HotkeyAction::Settings: show_settings_window(); break;
        case HotkeyAction::PaletteCollapse:
            if (palette_) palette_->set_collapsed(!palette_->collapsed());
            break;
        case HotkeyAction::ColorBlack: set_color(kBlack); break;
        case HotkeyAction::ColorYellow: set_color(kYellow); break;
        case HotkeyAction::ColorBlue: set_color(kBlue); break;
        case HotkeyAction::ColorRed: set_color(kRed); break;
        case HotkeyAction::ColorGreen: set_color(kGreen); break;
        case HotkeyAction::ColorPurple: set_color(kPurple); break;
        case HotkeyAction::ColorPanelAlternate: toggle_color_panel(); break;
        default:
            if (zoom_) zoom_->execute_action(action);
            break;
    }
}

bool Controller::matches_hotkey(HotkeyAction action, WPARAM virtual_key) const {
    const std::size_t index = static_cast<std::size_t>(action);
    if (index >= preferences_.hotkeys.size()) return false;
    const auto binding = preferences_.hotkeys[index];
    if (binding.virtual_key == 0 || binding.virtual_key != virtual_key) return false;
    UINT modifiers = 0;
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) modifiers |= MOD_CONTROL;
    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) modifiers |= MOD_SHIFT;
    if ((GetKeyState(VK_MENU) & 0x8000) != 0) modifiers |= MOD_ALT;
    if ((GetKeyState(VK_LWIN) & 0x8000) != 0 ||
        (GetKeyState(VK_RWIN) & 0x8000) != 0) modifiers |= MOD_WIN;
    return modifiers == binding.modifiers;
}

void Controller::close_panels() {
    if (colors_) colors_->hide();
    if (tools_) tools_->hide();
}

void Controller::stop_transient_mode() {
    close_panels();
    if (text_input_ && text_input_->active()) {
        text_input_->cancel();
        set_tool(Tool::Interact);
        return;
    }
    if (zoom_ && zoom_->active()) {
        zoom_->hide_zoom();
        return;
    }
    if (state_.whiteboard || state_.blackboard) {
        state_.whiteboard = false;
        state_.blackboard = false;
        for (const auto& overlay : overlays_) overlay->cancel_gesture();
        set_tool(Tool::Interact);
        return;
    }
    for (const auto& overlay : overlays_) overlay->cancel_gesture();
    set_tool(Tool::Interact);
}

void Controller::begin_text(PointF position) {
    close_panels();
    if (text_input_) text_input_->show_at(position, state_.color, state_.thickness);
}

void Controller::commit_pending_text() {
    if (text_input_ && text_input_->active()) text_input_->commit();
}

void Controller::cancel_pending_text() {
    if (text_input_ && text_input_->active()) text_input_->cancel();
}

void Controller::commit_text(PointF position, Color color, float thickness,
                             std::wstring text) {
    if (zoom_ && zoom_->active() && zoom_->accepts_annotations()) {
        zoom_->commit_text_screen(position, color, thickness, std::move(text));
        return;
    }
    Drawable drawable;
    drawable.kind = Tool::Text;
    drawable.color = color;
    drawable.width = thickness;
    drawable.points.push_back(position);
    drawable.text = std::move(text);
    commit_drawable(std::move(drawable));
}

void Controller::commit_drawable(Drawable drawable) {
    if (preferences_.fade_seconds > 0) {
        transient_drawables_.push_back({std::move(drawable),
            monotonic_milliseconds() + static_cast<std::uint64_t>(preferences_.fade_seconds) * 1000U});
        invalidate_document();
    } else {
        state_.document.add(std::move(drawable));
        invalidate_document();
    }
}

void Controller::populate_stress_document(std::size_t count) {
    if (!state_.document.empty()) state_.document.clear();
    transient_drawables_.clear();
    preview_.reset();
    state_.tool = Tool::Pen;
    state_.annotations_visible = true;
    for (std::size_t stroke = 0; stroke < count; ++stroke) {
        Drawable drawable;
        drawable.kind = stroke % 7 == 0 ? Tool::Highlighter : Tool::Pen;
        drawable.color = stroke % 5 == 0 ? kPurple : kBlue;
        drawable.width = drawable.kind == Tool::Highlighter ? 11.0F : 4.0F;
        drawable.points.reserve(24);
        const float base_x = 240.0F + static_cast<float>(stroke % 100) * 11.0F;
        const float base_y = 180.0F + static_cast<float>((stroke / 100) % 55) * 9.0F;
        for (std::size_t sample = 0; sample < 24; ++sample) {
            const float x = base_x + static_cast<float>(sample) * 7.0F;
            const float y = base_y + std::sin(static_cast<float>(sample) * 0.62F) * 9.0F;
            drawable.points.push_back({x, y});
        }
        state_.document.add(std::move(drawable));
    }
    update_overlay_interaction();
    invalidate_document();
}

void Controller::update_transient_ink() {
    if (transient_drawables_.empty()) return;
    const std::uint64_t now = monotonic_milliseconds();
    const auto old_size = transient_drawables_.size();
    std::erase_if(transient_drawables_, [now](const TransientDrawable& item) {
        return item.expires_at_ms <= now;
    });
    if (transient_drawables_.size() != old_size || !transient_drawables_.empty()) {
        invalidate_document();
    }
}

bool Controller::capture_region(PointF first, PointF second,
                                bool allow_synthetic_capture) {
    const int left = static_cast<int>(std::floor(std::min(first.x, second.x)));
    const int top = static_cast<int>(std::floor(std::min(first.y, second.y)));
    const int right = static_cast<int>(std::ceil(std::max(first.x, second.x)));
    const int bottom = static_cast<int>(std::ceil(std::max(first.y, second.y)));
    const int width = right - left;
    const int height = bottom - top;
    if (width < 8 || height < 8) {
        if (palette_) palette_->show_notification(L"Captura cancelada",
                                                   L"Selecciona una region mas grande.");
        return false;
    }

    HDC screen = GetDC(nullptr);
    HDC memory = CreateCompatibleDC(screen);
    BITMAPINFO information{};
    information.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    information.bmiHeader.biWidth = width;
    information.bmiHeader.biHeight = -height;
    information.bmiHeader.biPlanes = 1;
    information.bmiHeader.biBitCount = 32;
    information.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(screen, &information, DIB_RGB_COLORS,
                                      &pixels, nullptr, 0);
    if (!screen || !memory || !bitmap || !pixels) {
        if (bitmap) DeleteObject(bitmap);
        if (memory) DeleteDC(memory);
        if (screen) ReleaseDC(nullptr, screen);
        if (palette_) palette_->show_notification(L"No se pudo capturar",
                                                   L"Windows no entrego una superficie de captura.");
        return false;
    }
    HGDIOBJ previous = SelectObject(memory, bitmap);
    wchar_t synthetic_capture[4]{};
    const bool use_synthetic_capture = allow_synthetic_capture && GetEnvironmentVariableW(
        L"ELITE_PEN_QA_SYNTHETIC_CAPTURE", synthetic_capture,
        static_cast<DWORD>(std::size(synthetic_capture))) > 0;
    bool captured = false;
    if (use_synthetic_capture) {
        auto* destination = static_cast<std::uint32_t*>(pixels);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const std::uint8_t blue = static_cast<std::uint8_t>(40 + (x * 120 / width));
                const std::uint8_t green = static_cast<std::uint8_t>(55 + (y * 110 / height));
                const std::uint8_t red_channel = static_cast<std::uint8_t>(80 +
                    ((x + y) * 90 / std::max(width + height, 1)));
                destination[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                            static_cast<std::size_t>(x)] =
                    0xFF000000U | (static_cast<std::uint32_t>(red_channel) << 16U) |
                    (static_cast<std::uint32_t>(green) << 8U) | blue;
            }
        }
        captured = true;
    } else {
        captured = BitBlt(memory, 0, 0, width, height, screen, left, top,
                          SRCCOPY | CAPTUREBLT) != FALSE;
        if (!captured) {
            captured = capture_desktop_duplication(graphics_, left, top,
                                                    width, height, pixels);
        }
    }

    SelectObject(memory, previous);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    if (!captured) {
        DeleteObject(bitmap);
        if (palette_) palette_->show_notification(L"No se pudo capturar",
                                                   L"La copia de pantalla fallo.");
        return false;
    }
    return publish_capture_bitmap(bitmap, width, height);
}

bool Controller::publish_capture_bitmap(HBITMAP bitmap, int width, int height) {
    if (!bitmap || width <= 0 || height <= 0) return false;
    std::wstring saved_path;
    std::filesystem::path directory;
    wchar_t qa_directory[32768]{};
    const DWORD qa_length = GetEnvironmentVariableW(
        L"ELITE_PEN_QA_CAPTURE_DIR", qa_directory,
        static_cast<DWORD>(std::size(qa_directory)));
    if (qa_length > 0 && qa_length < std::size(qa_directory)) {
        directory = qa_directory;
    } else {
        PWSTR pictures = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, KF_FLAG_CREATE,
                                           nullptr, &pictures))) {
            directory = std::filesystem::path(pictures) / L"Elite Pen Captures";
            CoTaskMemFree(pictures);
        }
    }
    if (!directory.empty()) {
        std::error_code directory_error;
        std::filesystem::create_directories(directory, directory_error);
        if (!directory_error) {
            SYSTEMTIME time{};
            GetLocalTime(&time);
            wchar_t filename[96]{};
            swprintf_s(filename, std::size(filename),
                       L"Elite Pen %04u-%02u-%02u %02u-%02u-%02u-%03u.png",
                       time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute,
                       time.wSecond, time.wMilliseconds);
            const std::filesystem::path destination = directory / filename;
            ComPtr<IWICImagingFactory> factory;
            ComPtr<IWICStream> stream;
            ComPtr<IWICBitmapEncoder> encoder;
            ComPtr<IWICBitmapFrameEncode> frame;
            ComPtr<IPropertyBag2> properties;
            ComPtr<IWICBitmap> source_bitmap;
            ComPtr<IWICFormatConverter> converter;
            HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
            if (SUCCEEDED(result)) result = factory->CreateStream(stream.GetAddressOf());
            if (SUCCEEDED(result)) result = stream->InitializeFromFilename(
                destination.c_str(), GENERIC_WRITE);
            if (SUCCEEDED(result)) result = factory->CreateEncoder(
                GUID_ContainerFormatPng, nullptr, encoder.GetAddressOf());
            if (SUCCEEDED(result)) result = encoder->Initialize(
                stream.Get(), WICBitmapEncoderNoCache);
            if (SUCCEEDED(result)) result = encoder->CreateNewFrame(
                frame.GetAddressOf(), properties.GetAddressOf());
            if (SUCCEEDED(result)) result = frame->Initialize(properties.Get());
            if (SUCCEEDED(result)) result = frame->SetSize(
                static_cast<UINT>(width), static_cast<UINT>(height));
            WICPixelFormatGUID pixel_format = GUID_WICPixelFormat32bppBGRA;
            if (SUCCEEDED(result)) result = frame->SetPixelFormat(&pixel_format);
            if (SUCCEEDED(result)) result = factory->CreateBitmapFromHBITMAP(
                bitmap, nullptr, WICBitmapIgnoreAlpha, source_bitmap.GetAddressOf());
            if (SUCCEEDED(result)) result = factory->CreateFormatConverter(
                converter.GetAddressOf());
            if (SUCCEEDED(result)) result = converter->Initialize(
                source_bitmap.Get(), pixel_format, WICBitmapDitherTypeNone,
                nullptr, 0.0, WICBitmapPaletteTypeCustom);
            if (SUCCEEDED(result)) result = frame->WriteSource(converter.Get(), nullptr);
            if (SUCCEEDED(result)) result = frame->Commit();
            if (SUCCEEDED(result)) result = encoder->Commit();
            if (SUCCEEDED(result)) saved_path = destination.wstring();
        }
    }

    const std::size_t pixel_bytes = static_cast<std::size_t>(width) *
                                    static_cast<std::size_t>(height) * 4U;
    HGLOBAL dib = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + pixel_bytes);
    if (dib) {
        auto* header = static_cast<BITMAPINFOHEADER*>(GlobalLock(dib));
        if (header) {
            *header = {};
            header->biSize = sizeof(BITMAPINFOHEADER);
            header->biWidth = width;
            header->biHeight = height;
            header->biPlanes = 1;
            header->biBitCount = 32;
            header->biCompression = BI_RGB;
            header->biSizeImage = static_cast<DWORD>(pixel_bytes);
            HDC screen = GetDC(nullptr);
            const int copied_rows = screen ? GetDIBits(
                screen, bitmap, 0, static_cast<UINT>(height),
                reinterpret_cast<std::uint8_t*>(header) + sizeof(BITMAPINFOHEADER),
                reinterpret_cast<BITMAPINFO*>(header), DIB_RGB_COLORS) : 0;
            if (screen) ReleaseDC(nullptr, screen);
            GlobalUnlock(dib);
            if (copied_rows != height) {
                GlobalFree(dib);
                dib = nullptr;
            }
        } else {
            GlobalFree(dib);
            dib = nullptr;
        }
    }

    bool copied = false;
    bool clipboard_owns_bitmap = false;
    if (OpenClipboard(palette_ ? palette_->hwnd() : nullptr)) {
        EmptyClipboard();
        if (dib && SetClipboardData(CF_DIB, dib) != nullptr) {
            dib = nullptr;  // Clipboard owns the global block.
            copied = true;
        }
        clipboard_owns_bitmap = SetClipboardData(CF_BITMAP, bitmap) != nullptr;
        copied = copied || clipboard_owns_bitmap;
        CloseClipboard();
    }
    if (dib) GlobalFree(dib);
    if (!clipboard_owns_bitmap) DeleteObject(bitmap);

    std::wstring message;
    if (copied && !saved_path.empty()) message = L"Copiada y guardada en " + saved_path;
    else if (copied) message = L"Copiada al portapapeles.";
    else if (!saved_path.empty()) message = L"Guardada en " + saved_path;
    else message = L"No se pudo guardar ni copiar la captura.";
    if (palette_) palette_->show_notification(L"Captura de Elite Pen", message);
    return copied || !saved_path.empty();
}

void Controller::rebuild_overlays() {
    if (zoom_ && zoom_->active()) zoom_->hide_zoom();
    preview_.reset();
    state_.document.end_compound();
    overlays_.clear();
    std::wstring error;
    create_overlays(error);
    if (!error.empty()) report_runtime_error(error);
    if (palette_) SetWindowPos(palette_->hwnd(), HWND_TOPMOST, 0, 0, 0, 0,
                               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    invalidate_all();
}

void Controller::report_runtime_error(const std::wstring& message) {
    OutputDebugStringW((L"Elite Pen: " + message + L"\n").c_str());
}

void Controller::request_exit() {
    if (palette_) PostMessageW(palette_->hwnd(), kExitMessage, 0, 0);
}

void Controller::save_palette_position() {
    if (!palette_ || !palette_->hwnd()) return;
    RECT bounds{};
    if (!GetWindowRect(palette_->hwnd(), &bounds)) return;
    preferences_.has_palette_position = true;
    preferences_.palette_x = bounds.left;
    preferences_.palette_y = bounds.top;
}

void Controller::show_settings_window() {
    if (text_input_ && text_input_->active()) text_input_->commit();
    close_panels();
    if (settings_) settings_->show_settings();
}

void Controller::apply_capture_preference() {
    const DWORD affinity = preferences_.exclude_palette_from_capture
        ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE;
    if (palette_ && palette_->hwnd()) SetWindowDisplayAffinity(palette_->hwnd(), affinity);
    if (colors_ && colors_->hwnd()) SetWindowDisplayAffinity(colors_->hwnd(), affinity);
    if (tools_ && tools_->hwnd()) SetWindowDisplayAffinity(tools_->hwnd(), affinity);
    if (text_input_ && text_input_->hwnd()) SetWindowDisplayAffinity(text_input_->hwnd(), affinity);
}

void Controller::set_palette_size(int size) {
    const int clamped = std::clamp(size, 0, static_cast<int>(kPaletteScales.size()) - 1);
    preferences_.palette_size = clamped;
    if (palette_) palette_->apply_size(clamped);
    save_palette_position();
    save_preferences();
}

void Controller::set_control_mode(ControlMode mode) {
    if (preferences_.control_mode == mode) return;
    preferences_.control_mode = mode;
    close_panels();
    if (palette_) palette_->apply_mode(mode);
    save_palette_position();
    save_preferences();
}

void Controller::set_zoom_lens_diameter(int diameter) {
    diameter = std::clamp(diameter, 280, 960);
    if (zoom_) {
        zoom_->set_lens_diameter(diameter);
    } else {
        preferences_.zoom_lens_diameter = diameter;
        save_preferences();
    }
}

void Controller::adjust_zoom_lens_diameter(int direction) {
    if (zoom_) zoom_->adjust_lens_diameter(direction);
}

void Controller::set_theme(AppTheme theme) {
    if (preferences_.theme == theme && g_ui_theme == theme) return;
    preferences_.theme = theme;
    g_ui_theme = theme;
    save_preferences();
    if (settings_) settings_->apply_theme();
    if (palette_) palette_->invalidate();
    if (colors_) colors_->invalidate();
    if (tools_) tools_->invalidate();
    if (text_input_) text_input_->invalidate();
    if (zoom_) zoom_->apply_theme();
    if (state_.cursor_highlight) invalidate_document();
    restack_zoom();
}

bool Controller::set_hotkey_binding(HotkeyAction action, HotkeyBinding binding) {
    const std::size_t index = static_cast<std::size_t>(action);
    if (index >= preferences_.hotkeys.size() || !palette_) return false;
    const bool global = index < kGlobalHotkeyActionCount;
    for (std::size_t current = 0; binding.virtual_key != 0 &&
         current < preferences_.hotkeys.size(); ++current) {
        const bool current_global = current < kGlobalHotkeyActionCount;
        if (current != index && global == current_global &&
            preferences_.hotkeys[current] == binding) return false;
    }
    auto candidate = preferences_.hotkeys;
    candidate[index] = binding;
    if (!palette_->apply_hotkeys(candidate)) return false;
    preferences_.hotkeys = candidate;
    save_preferences();
    return true;
}

bool Controller::reset_hotkeys() {
    if (!palette_ || !palette_->apply_hotkeys(kDefaultHotkeys)) return false;
    preferences_.hotkeys = kDefaultHotkeys;
    save_preferences();
    return true;
}

void Controller::reset_palette_position() {
    if (!palette_) return;
    POINT origin{0, 0};
    HMONITOR monitor = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    SetWindowPos(palette_->hwnd(), HWND_TOPMOST, info.rcWork.left + 28,
                 info.rcWork.top + 28, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    save_palette_position();
    save_preferences();
}

void Controller::save_preferences() {
    preferences_.zoom_factor = state_.zoom_factor;
    preferences_.thickness = state_.thickness;
    preferences_.color = state_.color;
    preferences_store_.save(preferences_);
}

bool Controller::zoom_active() const noexcept {
    return zoom_ && zoom_->active();
}

bool Controller::zoom_frozen() const noexcept {
    return zoom_ && zoom_->frozen();
}

void Controller::restack_zoom() {
    if (zoom_ && zoom_->active()) zoom_->bring_to_front();
}

}  // namespace

struct Application::Impl {
    Controller controller;
    HANDLE instance_mutex{};

    ~Impl() {
        controller.shutdown();
        if (instance_mutex) CloseHandle(instance_mutex);
    }
};

Application::Application() : impl_(std::make_unique<Impl>()) {}
Application::~Application() = default;

int Application::run() {
    std::wstring instance_name = L"Local\\PowerEliteStudio.ElitePen.Singleton.v1";
    wchar_t qa_instance[96]{};
    const DWORD qa_length = GetEnvironmentVariableW(
        L"ELITE_PEN_QA_INSTANCE_ID", qa_instance,
        static_cast<DWORD>(std::size(qa_instance)));
    if (qa_length > 0 &&
        qa_length < static_cast<DWORD>(std::size(qa_instance))) {
        instance_name += L".";
        for (DWORD index = 0; index < qa_length; ++index) {
            const wchar_t value = qa_instance[index];
            if ((value >= L'a' && value <= L'z') ||
                (value >= L'A' && value <= L'Z') ||
                (value >= L'0' && value <= L'9') || value == L'-' ||
                value == L'_') {
                instance_name.push_back(value);
            }
        }
    }
    impl_->instance_mutex = CreateMutexW(nullptr, TRUE,
                                         instance_name.c_str());
    if (!impl_->instance_mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND palette = FindWindowW(L"ElitePen.Palette", nullptr);
        if (palette) {
            ShowWindow(palette, SW_SHOWNOACTIVATE);
            SetWindowPos(palette, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return 0;
    }

    std::wstring error;
    if (!impl_->controller.initialize(error)) {
        MessageBoxW(nullptr, error.c_str(), L"Elite Pen no pudo iniciar",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    return impl_->controller.message_loop();
}

}  // namespace elite_pen::win
