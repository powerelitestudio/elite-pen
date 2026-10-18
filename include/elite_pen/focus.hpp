#pragma once

#include "elite_pen/core.hpp"

namespace elite_pen {

inline constexpr std::size_t kFocusZoneCount = 8;
inline constexpr std::size_t kFocusMaskCount = 8;

struct FocusScene {
    RectF area{};
    std::array<RectF, kFocusMaskCount> masks{};
    std::size_t mask_count{};
};

[[nodiscard]] inline bool valid_focus_rect(RectF rect) noexcept {
    return std::isfinite(rect.left) && std::isfinite(rect.top) &&
           std::isfinite(rect.right) && std::isfinite(rect.bottom) &&
           std::abs(rect.left) <= 1000000 && std::abs(rect.top) <= 1000000 &&
           std::abs(rect.right) <= 1000000 && std::abs(rect.bottom) <= 1000000 &&
           rect.width() >= 8 && rect.height() >= 8;
}

[[nodiscard]] inline RectF ordered_focus_rect(PointF a, PointF b) noexcept {
    return {std::min(a.x, b.x), std::min(a.y, b.y),
            std::max(a.x, b.x), std::max(a.y, b.y)};
}

// Preserve content proportions while fitting the requested area, including
// narrow selections, negative-coordinate displays and disconnected monitors.
[[nodiscard]] inline RectF fit_focus_source(RectF area, RectF monitor) noexcept {
    if (!valid_focus_rect(monitor)) return {};
    if (!valid_focus_rect(area)) return monitor;
    const float scale = std::clamp(std::min(monitor.width() / area.width(),
                                           monitor.height() / area.height()), 1.0F, 8.0F);
    const float width = monitor.width() / scale;
    const float height = monitor.height() / scale;
    const float left = std::clamp((area.left + area.right - width) * 0.5F,
                                  monitor.left, monitor.right - width);
    const float top = std::clamp((area.top + area.bottom - height) * 0.5F,
                                 monitor.top, monitor.bottom - height);
    return {left, top, left + width, top + height};
}

[[nodiscard]] inline RectF clamp_focus_area(RectF area, RectF monitor) noexcept {
    if (!valid_focus_rect(area) || !valid_focus_rect(monitor)) return {};
    const float width = std::min(area.width(), monitor.width());
    const float height = std::min(area.height(), monitor.height());
    const float left = std::clamp(area.left, monitor.left, monitor.right - width);
    const float top = std::clamp(area.top, monitor.top, monitor.bottom - height);
    return {left, top, left + width, top + height};
}

// Smoothstep has zero speed at both ends. Interpolating same-aspect boxes
// animates pan and magnification together without stretching the source.
[[nodiscard]] inline RectF interpolate_focus(RectF from, RectF to, float progress) noexcept {
    const float t = std::isfinite(progress) ? std::clamp(progress, 0.0F, 1.0F) : 1.0F;
    const float eased = t * t * (3.0F - 2.0F * t);
    const auto mix = [eased](float a, float b) { return a + (b - a) * eased; };
    return {mix(from.left, to.left), mix(from.top, to.top),
            mix(from.right, to.right), mix(from.bottom, to.bottom)};
}

}  // namespace elite_pen
