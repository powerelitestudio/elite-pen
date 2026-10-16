#pragma once

#include "preferences.hpp"

#include <windows.h>

#include <array>
#include <cmath>

namespace elite_pen::win {

inline constexpr int kPaletteDesignWidth = 290;
inline constexpr int kPaletteDesignHeight = 280;
inline constexpr int kLinearDesignWidth = 76;
inline constexpr int kLinearDesignHeight = 676;
inline constexpr int kLinearCollapsedHeight = 82;
inline constexpr int kGeometryPanelDesignWidth = 366;
inline constexpr int kGeometryPanelDesignHeight = 232;
inline constexpr RECT kGeometrySizeBounds{120, 8, 205, 36};
inline constexpr RECT kGeometryPinBounds{211, 8, 305, 36};
inline constexpr RECT kGeometryCloseBounds{313, 8, 349, 36};
inline constexpr RECT kToolsFiguresBounds{260, 8, 349, 36};

inline float geometry_panel_scale(int index) noexcept {
    return kGeometryPanelScales[static_cast<std::size_t>(std::clamp(
        index, 0, static_cast<int>(kGeometryPanelScales.size()) - 1))];
}

inline SIZE geometry_panel_pixel_size(int index) noexcept {
    const float scale = geometry_panel_scale(index);
    return {static_cast<LONG>(std::lround(kGeometryPanelDesignWidth * scale)),
             static_cast<LONG>(std::lround(kGeometryPanelDesignHeight * scale))};
}

inline POINT geometry_panel_logical_point(POINT point, int index) noexcept {
    const float scale = geometry_panel_scale(index);
    return {static_cast<LONG>(std::floor(static_cast<float>(point.x) / scale)),
             static_cast<LONG>(std::floor(static_cast<float>(point.y) / scale))};
}

enum class LinearAction {
    Visibility,
    Interact,
    ToolPanel,
    Highlighter,
    Eraser,
    Geometry,
    Text,
    Board,
    Zoom,
    Undo,
    Redo,
    Clear,
    Settings,
};

struct LinearItem {
    LinearAction action;
    RECT bounds;
    const wchar_t* tooltip;
};

inline constexpr RECT kLinearCollapseBounds{22, 10, 54, 48};
inline constexpr RECT kLinearThicknessBounds{9, 550, 67, 592};
inline constexpr std::array<POINT, 5> kLinearThicknessPoints{{
    {16, 571}, {27, 571}, {38, 571}, {49, 571}, {60, 571}
}};
inline constexpr std::array<POINT, 6> kLinearColorPoints{{
    {20, 614}, {38, 614}, {56, 614}, {20, 634}, {38, 634}, {56, 634}
}};
inline constexpr POINT kLinearMoreColorsPoint{38, 657};

inline constexpr std::array<LinearItem, 13> kLinearItems{{
    {LinearAction::Visibility,  {10,  56, 66,  90}, L"Ocultar o mostrar anotaciones"},
    {LinearAction::Interact,    {10,  94, 66, 128}, L"Alternar entre cursor y lapiz"},
    {LinearAction::ToolPanel,   {10, 132, 66, 166}, L"Abrir todas las herramientas"},
    {LinearAction::Highlighter, {10, 170, 66, 204}, L"Resaltador"},
    {LinearAction::Eraser,      {10, 208, 66, 242}, L"Borrador selectivo"},
    {LinearAction::Geometry,    {10, 246, 66, 280}, L"Figuras geometricas"},
    {LinearAction::Text,        {10, 284, 66, 318}, L"Texto directo sobre la pantalla"},
    {LinearAction::Board,       {10, 322, 66, 356}, L"Pizarra blanca; clic derecho: negra"},
    {LinearAction::Zoom,        {10, 360, 66, 394}, L"Zoom"},
    {LinearAction::Undo,        {10, 398, 66, 432}, L"Deshacer"},
    {LinearAction::Redo,        {10, 436, 66, 470}, L"Rehacer"},
    {LinearAction::Clear,       {10, 474, 66, 508}, L"Limpiar anotaciones"},
    {LinearAction::Settings,    {10, 512, 66, 546}, L"Configuracion"},
}};

inline constexpr int control_design_width(ControlMode mode) noexcept {
    return mode == ControlMode::Linear ? kLinearDesignWidth : kPaletteDesignWidth;
}

inline constexpr int control_design_height(ControlMode mode) noexcept {
    return mode == ControlMode::Linear ? kLinearDesignHeight : kPaletteDesignHeight;
}

inline int control_pixel_width(ControlMode mode, float scale, bool collapsed) noexcept {
    const float factor = collapsed && mode == ControlMode::Palette ? 0.30F : 1.0F;
    return std::max(1, static_cast<int>(std::lround(
        static_cast<float>(control_design_width(mode)) * scale * factor)));
}

inline int control_pixel_height(ControlMode mode, float scale, bool collapsed) noexcept {
    const int logical_height = collapsed && mode == ControlMode::Linear
        ? kLinearCollapsedHeight : control_design_height(mode);
    const float factor = collapsed && mode == ControlMode::Palette ? 0.30F : 1.0F;
    return std::max(1, static_cast<int>(std::lround(
        static_cast<float>(logical_height) * scale * factor)));
}

inline POINT control_position_in_work_area(POINT desired, RECT work_area,
                                           int width, int height) noexcept {
    // A giant linear bar can exceed a short monitor. Keep its upper-left corner
    // reachable without passing an inverted range to std::clamp or changing scale.
    return {
        std::clamp(desired.x, work_area.left,
                   std::max(work_area.left, work_area.right - width)),
        std::clamp(desired.y, work_area.top,
                   std::max(work_area.top, work_area.bottom - height))
    };
}

inline bool point_in_rect(POINT point, RECT bounds) noexcept {
    return point.x >= bounds.left && point.x <= bounds.right &&
           point.y >= bounds.top && point.y <= bounds.bottom;
}

}  // namespace elite_pen::win
