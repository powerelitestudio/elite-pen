#include "elite_pen/core.hpp"

#include <iterator>
#include <limits>

namespace elite_pen {

namespace {

constexpr float kMinimumZoomScale = 0.001F;

float safe_zoom_scale(float scale) noexcept {
    return std::isfinite(scale) && scale >= kMinimumZoomScale
        ? scale : 1.0F;
}

}  // namespace

PointF ZoomViewportTransform::view_to_source(PointF point) const noexcept {
    const float factor = safe_zoom_scale(scale);
    return {source.left + point.x / factor,
            source.top + point.y / factor};
}

PointF ZoomViewportTransform::source_to_view(PointF point) const noexcept {
    const float factor = safe_zoom_scale(scale);
    return {(point.x - source.left) * factor,
            (point.y - source.top) * factor};
}

float ZoomViewportTransform::view_to_source_length(float length) const noexcept {
    return length / safe_zoom_scale(scale);
}

float ZoomViewportTransform::source_to_view_length(float length) const noexcept {
    return length * safe_zoom_scale(scale);
}

float zoom_entry_factor(float start, float target, float progress) noexcept {
    if (!std::isfinite(start)) start = 1.0F;
    if (!std::isfinite(target)) target = start;
    progress = std::isfinite(progress) ? std::clamp(progress, 0.0F, 1.0F) : 1.0F;
    const float remaining = 1.0F - progress;
    const float eased = 1.0F - remaining * remaining * remaining;
    return start + (target - start) * eased;
}

const wchar_t* tool_name(Tool tool) noexcept {
    switch (tool) {
        case Tool::Interact: return L"Interactuar";
        case Tool::Pen: return L"Lapiz";
        case Tool::Highlighter: return L"Resaltador";
        case Tool::Eraser: return L"Borrador";
        case Tool::Line: return L"Linea";
        case Tool::Rectangle: return L"Rectangulo";
        case Tool::Ellipse: return L"Elipse";
        case Tool::Arrow: return L"Flecha";
        case Tool::CurvedArrow: return L"Flecha curva";
        case Tool::Text: return L"Texto";
        case Tool::Screenshot: return L"Captura";
        case Tool::Zoom: return L"Zoom";
    }
    return L"Herramienta";
}

bool is_drawing_tool(Tool tool) noexcept {
    return tool != Tool::Interact && tool != Tool::Zoom;
}

Tool gesture_tool(Tool selected, bool shift, bool control,
                  bool /*alt*/, bool tab) noexcept {
    // Modifier gestures are intentionally scoped to freehand Pen. Explicitly
    // selected tools retain their own behavior (for example Shift keeps a
    // manually selected rectangle square), while Eraser/Text remain protected.
    if (selected != Tool::Pen) return selected;
    if (control && shift) return Tool::Arrow;
    if (shift && tab) return Tool::CurvedArrow;
    if (tab) return Tool::Ellipse;
    if (control) return Tool::Rectangle;
    if (shift) return Tool::Line;
    return selected;
}

float distance(PointF a, PointF b) noexcept {
    return std::hypot(a.x - b.x, a.y - b.y);
}

float distance_to_segment(PointF p, PointF a, PointF b) noexcept {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float denominator = dx * dx + dy * dy;
    if (denominator <= std::numeric_limits<float>::epsilon()) {
        return distance(p, a);
    }
    const float t = std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / denominator,
                               0.0F, 1.0F);
    return distance(p, {a.x + t * dx, a.y + t * dy});
}

CubicBezier curved_arrow_bezier(PointF start, PointF end,
                                float reference_scale) noexcept {
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float length = std::hypot(dx, dy);
    if (length <= std::numeric_limits<float>::epsilon()) {
        return {start, start, end, end};
    }
    const PointF normal{-dy / length, dx / length};
    const float bend = std::min(
        length * 0.28F, 110.0F / safe_zoom_scale(reference_scale));
    return {
        start,
        {start.x + dx * 0.30F + normal.x * bend,
         start.y + dy * 0.30F + normal.y * bend},
        {start.x + dx * 0.70F + normal.x * bend,
         start.y + dy * 0.70F + normal.y * bend},
        end
    };
}

PointF cubic_bezier_point(const CubicBezier& curve, float t) noexcept {
    t = std::clamp(t, 0.0F, 1.0F);
    const float inverse = 1.0F - t;
    const float a = inverse * inverse * inverse;
    const float b = 3.0F * inverse * inverse * t;
    const float c = 3.0F * inverse * t * t;
    const float d = t * t * t;
    return {
        curve.start.x * a + curve.control1.x * b + curve.control2.x * c + curve.end.x * d,
        curve.start.y * a + curve.control1.y * b + curve.control2.y * c + curve.end.y * d
    };
}

ArrowHead arrow_head_points(PointF before, PointF end, float width,
                            float reference_scale) noexcept {
    const float angle = std::atan2(end.y - before.y, end.x - before.x);
    const float scale = safe_zoom_scale(reference_scale);
    const float size = std::clamp(
        width * 3.2F, 12.0F / scale, 38.0F / scale);
    constexpr float spread = 0.62F;
    return {
        {end.x - size * std::cos(angle - spread),
         end.y - size * std::sin(angle - spread)},
        {end.x - size * std::cos(angle + spread),
         end.y - size * std::sin(angle + spread)}
    };
}

RectF Drawable::bounds() const noexcept {
    if (bounds_cached_) return cached_bounds_;
    if (points.empty()) return {};
    RectF result{points[0].x, points[0].y, points[0].x, points[0].y};
    for (const auto& point : points) {
        result.left = std::min(result.left, point.x);
        result.top = std::min(result.top, point.y);
        result.right = std::max(result.right, point.x);
        result.bottom = std::max(result.bottom, point.y);
    }
    if (kind == Tool::CurvedArrow && points.size() >= 2) {
        const auto curve = curved_arrow_bezier(
            points.front(), points.back(), reference_scale);
        for (const PointF point : {curve.control1, curve.control2}) {
            result.left = std::min(result.left, point.x);
            result.top = std::min(result.top, point.y);
            result.right = std::max(result.right, point.x);
            result.bottom = std::max(result.bottom, point.y);
        }
    }
    if ((kind == Tool::Arrow || kind == Tool::CurvedArrow) && points.size() >= 2) {
        const PointF before = kind == Tool::CurvedArrow
            ? curved_arrow_bezier(
                points.front(), points.back(), reference_scale).control2
            : points[points.size() - 2];
        const auto head = arrow_head_points(
            before, points.back(), width, reference_scale);
        for (const PointF point : {head.left, head.right}) {
            result.left = std::min(result.left, point.x);
            result.top = std::min(result.top, point.y);
            result.right = std::max(result.right, point.x);
            result.bottom = std::max(result.bottom, point.y);
        }
    }
    const float padding = std::max(width, 3.0F) * 1.5F;
    result.left -= padding;
    result.top -= padding;
    result.right += padding;
    result.bottom += padding;
    if (kind == Tool::Text && points.size() == 1) {
        result.right = std::max(result.right, result.left + 400.0F);
        result.bottom = std::max(result.bottom, result.top + width * 6.0F);
    }
    cached_bounds_ = result;
    bounds_cached_ = true;
    return cached_bounds_;
}

namespace {

float squared_distance(PointF a, PointF b) noexcept {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

float squared_distance_to_segment(PointF point, PointF start, PointF end) noexcept {
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float denominator = dx * dx + dy * dy;
    if (denominator <= std::numeric_limits<float>::epsilon()) {
        return squared_distance(point, start);
    }
    const float projection = std::clamp(
        ((point.x - start.x) * dx + (point.y - start.y) * dy) / denominator,
        0.0F, 1.0F);
    const PointF closest{start.x + projection * dx, start.y + projection * dy};
    return squared_distance(point, closest);
}

bool path_hit(const Drawable& item, PointF point, float tolerance) noexcept {
    const float radius = tolerance + item.width * 0.5F;
    const float radius_squared = radius * radius;
    if (item.points.size() == 1) {
        return squared_distance(item.points.front(), point) <= radius_squared;
    }
    for (std::size_t i = 1; i < item.points.size(); ++i) {
        if (squared_distance_to_segment(
                point, item.points[i - 1], item.points[i]) <= radius_squared) {
            return true;
        }
    }
    return false;
}

bool curved_arrow_hit(const Drawable& item, PointF point, float tolerance) noexcept {
    if (item.points.size() < 2) return false;
    const auto curve = curved_arrow_bezier(
        item.points.front(), item.points.back(), item.reference_scale);
    const float radius = tolerance + item.width * 0.5F;
    const float radius_squared = radius * radius;
    PointF previous = curve.start;
    const int samples = std::clamp(
        static_cast<int>(std::ceil(distance(curve.start, curve.end) / 8.0F)), 24, 128);
    bool curve_hit = false;
    for (int index = 1; index <= samples; ++index) {
        const PointF current = cubic_bezier_point(
            curve, static_cast<float>(index) / static_cast<float>(samples));
        if (squared_distance_to_segment(point, previous, current) <= radius_squared) {
            curve_hit = true;
            break;
        }
        previous = current;
    }
    if (curve_hit) return true;
    const auto head = arrow_head_points(
        curve.control2, curve.end, item.width, item.reference_scale);
    return squared_distance_to_segment(point, curve.end, head.left) <= radius_squared ||
           squared_distance_to_segment(point, curve.end, head.right) <= radius_squared;
}

}  // namespace

bool hit_test(const Drawable& item, PointF point, float tolerance) noexcept {
    if (item.points.empty()) return false;
    if (!item.bounds().contains(point)) return false;

    if (item.kind == Tool::Text) return item.bounds().contains(point);
    if (item.kind == Tool::CurvedArrow) return curved_arrow_hit(item, point, tolerance);
    if (item.kind == Tool::Pen || item.kind == Tool::Highlighter ||
        item.kind == Tool::Line || item.kind == Tool::Arrow) {
        if (path_hit(item, point, tolerance)) return true;
        if (item.kind == Tool::Arrow && item.points.size() >= 2) {
            const float radius = tolerance + item.width * 0.5F;
            const float radius_squared = radius * radius;
            const auto head = arrow_head_points(item.points[item.points.size() - 2],
                                                item.points.back(), item.width,
                                                item.reference_scale);
            return squared_distance_to_segment(point, item.points.back(), head.left) <=
                       radius_squared ||
                   squared_distance_to_segment(point, item.points.back(), head.right) <=
                       radius_squared;
        }
        return false;
    }
    if ((item.kind == Tool::Rectangle || item.kind == Tool::Ellipse) &&
        item.points.size() >= 2) {
        const PointF a = item.points.front();
        const PointF b = item.points.back();
        const RectF rect{std::min(a.x, b.x), std::min(a.y, b.y),
                         std::max(a.x, b.x), std::max(a.y, b.y)};
        const float radius = tolerance + item.width * 0.5F;
        if (item.kind == Tool::Rectangle) {
            const bool near_horizontal = point.x >= rect.left - radius &&
                point.x <= rect.right + radius &&
                (std::abs(point.y - rect.top) <= radius ||
                 std::abs(point.y - rect.bottom) <= radius);
            const bool near_vertical = point.y >= rect.top - radius &&
                point.y <= rect.bottom + radius &&
                (std::abs(point.x - rect.left) <= radius ||
                 std::abs(point.x - rect.right) <= radius);
            return near_horizontal || near_vertical;
        }
        const float rx = std::max(rect.width() * 0.5F, 0.01F);
        const float ry = std::max(rect.height() * 0.5F, 0.01F);
        const float cx = rect.left + rx;
        const float cy = rect.top + ry;
        const float dx = point.x - cx;
        const float dy = point.y - cy;
        const float normalized = std::sqrt((dx * dx) / (rx * rx) + (dy * dy) / (ry * ry));
        if (normalized <= std::numeric_limits<float>::epsilon()) return false;
        const float gradient = std::sqrt((dx * dx) / (rx * rx * rx * rx) +
                                         (dy * dy) / (ry * ry * ry * ry)) / normalized;
        const float approximate_distance = gradient > std::numeric_limits<float>::epsilon()
            ? std::abs(normalized - 1.0F) / gradient
            : std::numeric_limits<float>::max();
        return approximate_distance <= radius;
    }
    return path_hit(item, point, tolerance);
}

std::vector<PointF> simplify_path(const std::vector<PointF>& input, float epsilon) {
    if (input.size() < 3 || epsilon <= 0.0F) return input;
    std::vector<std::uint8_t> keep(input.size(), 0);
    keep.front() = true;
    keep.back() = true;
    std::vector<std::pair<std::size_t, std::size_t>> ranges;
    ranges.reserve(64);
    ranges.emplace_back(0, input.size() - 1);
    const float epsilon_squared = epsilon * epsilon;
    while (!ranges.empty()) {
        const auto [first, last] = ranges.back();
        ranges.pop_back();
        if (last <= first + 1) continue;
        float maximum_squared = 0.0F;
        std::size_t selected = first;
        for (std::size_t index = first + 1; index < last; ++index) {
            const float value = squared_distance_to_segment(
                input[index], input[first], input[last]);
            if (value > maximum_squared) {
                maximum_squared = value;
                selected = index;
            }
        }
        if (maximum_squared > epsilon_squared) {
            keep[selected] = 1;
            ranges.emplace_back(selected, last);
            ranges.emplace_back(first, selected);
        }
    }
    std::vector<PointF> output;
    output.reserve(input.size());
    for (std::size_t index = 0; index < input.size(); ++index) {
        if (keep[index]) output.push_back(input[index]);
    }
    return output;
}

Document::Document(std::size_t history_limit)
    : history_limit_(std::max<std::size_t>(history_limit, 1)) {
    items_.reserve(256);
}

void Document::commit(Operation operation) {
    if (compound_active_ && operation.kind == OperationKind::Remove) {
        apply(operation);
        if (!compound_operation_) {
            compound_operation_ = Operation{OperationKind::Remove, {}};
            redo_.clear();
        }
        for (auto& entry : operation.entries) {
            std::size_t original_index = entry.index;
            for (const auto& removed : compound_operation_->entries) {
                if (removed.index <= original_index) ++original_index;
            }
            entry.index = original_index;
        }
        compound_operation_->entries.insert(compound_operation_->entries.end(),
            std::make_move_iterator(operation.entries.begin()),
            std::make_move_iterator(operation.entries.end()));
        return;
    }
    apply(operation);
    undo_.push_back(std::move(operation));
    redo_.clear();
    while (undo_.size() > history_limit_) undo_.pop_front();
}

void Document::begin_compound() {
    if (compound_active_) return;
    compound_active_ = true;
    compound_operation_.reset();
}

void Document::end_compound() {
    if (!compound_active_) return;
    compound_active_ = false;
    if (!compound_operation_ || compound_operation_->entries.empty()) {
        compound_operation_.reset();
        return;
    }
    std::sort(compound_operation_->entries.begin(), compound_operation_->entries.end(),
              [](const IndexedDrawable& left, const IndexedDrawable& right) {
                  return left.index < right.index;
              });
    undo_.push_back(std::move(*compound_operation_));
    compound_operation_.reset();
    while (undo_.size() > history_limit_) undo_.pop_front();
}

void Document::add(Drawable drawable) {
    if (drawable.points.empty()) return;
    Operation operation;
    operation.kind = OperationKind::Add;
    operation.entries.reserve(1);
    operation.entries.push_back({items_.size(), std::move(drawable)});
    commit(std::move(operation));
}

bool Document::erase_at(PointF point, float tolerance) {
    for (std::size_t reverse = items_.size(); reverse > 0; --reverse) {
        const std::size_t index = reverse - 1;
        if (hit_test(items_[index], point, tolerance)) {
            Operation operation;
            operation.kind = OperationKind::Remove;
            operation.entries.reserve(1);
            operation.entries.push_back({index, {}});
            commit(std::move(operation));
            return true;
        }
    }
    return false;
}

bool Document::clear() {
    if (items_.empty()) return false;
    Operation operation;
    operation.kind = OperationKind::Clear;
    operation.entries.reserve(items_.size());
    for (std::size_t index = 0; index < items_.size(); ++index) {
        operation.entries.push_back({index, {}});
    }
    commit(std::move(operation));
    return true;
}

void Document::apply(Operation& operation) {
    if (operation.kind == OperationKind::Add) {
        const bool append = operation.entries.size() == 1 &&
            operation.entries.front().index == items_.size();
        const std::size_t change_index = operation.entries.empty()
            ? items_.size() : operation.entries.front().index;
        for (auto& entry : operation.entries) {
            const std::size_t index = std::min(entry.index, items_.size());
            if (index == items_.size()) items_.push_back(std::move(entry.drawable));
            else items_.insert(items_.begin() + static_cast<std::ptrdiff_t>(index),
                               std::move(entry.drawable));
        }
        changed(append ? DocumentChangeKind::Append : DocumentChangeKind::Rebuild,
                change_index);
    } else if (operation.kind == OperationKind::Remove) {
        const bool single = operation.entries.size() == 1;
        const std::size_t change_index = single ? operation.entries.front().index : 0;
        for (auto iterator = operation.entries.rbegin(); iterator != operation.entries.rend(); ++iterator) {
            if (iterator->index < items_.size()) {
                iterator->drawable = std::move(items_[iterator->index]);
                items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(iterator->index));
            }
        }
        changed(single ? DocumentChangeKind::Remove : DocumentChangeKind::Rebuild,
                change_index);
    } else {
        const std::size_t count = std::min(operation.entries.size(), items_.size());
        for (std::size_t index = 0; index < count; ++index) {
            operation.entries[index].drawable = std::move(items_[index]);
        }
        items_.clear();
        changed(DocumentChangeKind::Clear);
    }
}

void Document::revert(Operation& operation) {
    if (operation.kind == OperationKind::Add) {
        const bool single = operation.entries.size() == 1;
        const std::size_t change_index = single ? operation.entries.front().index : 0;
        for (auto iterator = operation.entries.rbegin(); iterator != operation.entries.rend(); ++iterator) {
            if (iterator->index < items_.size()) {
                iterator->drawable = std::move(items_[iterator->index]);
                items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(iterator->index));
            }
        }
        changed(single ? DocumentChangeKind::Remove : DocumentChangeKind::Rebuild,
                change_index);
    } else if (operation.kind == OperationKind::Clear && items_.empty()) {
        items_.reserve(operation.entries.size());
        for (auto& entry : operation.entries) {
            items_.push_back(std::move(entry.drawable));
        }
        changed(DocumentChangeKind::Rebuild);
    } else {
        const bool single = operation.entries.size() == 1;
        const std::size_t change_index = single ? operation.entries.front().index : 0;
        for (auto& entry : operation.entries) {
            items_.insert(items_.begin() + static_cast<std::ptrdiff_t>(std::min(entry.index, items_.size())),
                          std::move(entry.drawable));
        }
        changed(single ? DocumentChangeKind::Insert : DocumentChangeKind::Rebuild,
                change_index);
    }
}

bool Document::undo() {
    if (undo_.empty()) return false;
    Operation operation = std::move(undo_.back());
    undo_.pop_back();
    revert(operation);
    redo_.push_back(std::move(operation));
    return true;
}

bool Document::redo() {
    if (redo_.empty()) return false;
    Operation operation = std::move(redo_.back());
    redo_.pop_back();
    apply(operation);
    undo_.push_back(std::move(operation));
    return true;
}

}  // namespace elite_pen
