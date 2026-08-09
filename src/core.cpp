#include "elite_pen/core.hpp"

#include <limits>

namespace elite_pen {

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

CubicBezier curved_arrow_bezier(PointF start, PointF end) noexcept {
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float length = std::hypot(dx, dy);
    if (length <= std::numeric_limits<float>::epsilon()) {
        return {start, start, end, end};
    }
    const PointF normal{-dy / length, dx / length};
    const float bend = std::min(length * 0.28F, 110.0F);
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
        const auto curve = curved_arrow_bezier(points.front(), points.back());
        for (const PointF point : {curve.control1, curve.control2}) {
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
    if (kind == Tool::Text) {
        result.right = std::max(result.right, result.left + 400.0F);
        result.bottom = std::max(result.bottom, result.top + width * 6.0F);
    }
    cached_bounds_ = result;
    bounds_cached_ = true;
    return cached_bounds_;
}

namespace {

bool path_hit(const Drawable& item, PointF point, float tolerance) noexcept {
    if (item.points.size() == 1) {
        return distance(item.points.front(), point) <= tolerance + item.width * 0.5F;
    }
    const float radius = tolerance + item.width * 0.5F;
    for (std::size_t i = 1; i < item.points.size(); ++i) {
        if (distance_to_segment(point, item.points[i - 1], item.points[i]) <= radius) {
            return true;
        }
    }
    return false;
}

bool curved_arrow_hit(const Drawable& item, PointF point, float tolerance) noexcept {
    if (item.points.size() < 2) return false;
    const auto curve = curved_arrow_bezier(item.points.front(), item.points.back());
    const float radius = tolerance + item.width * 0.5F;
    PointF previous = curve.start;
    constexpr int samples = 32;
    for (int index = 1; index <= samples; ++index) {
        const PointF current = cubic_bezier_point(
            curve, static_cast<float>(index) / static_cast<float>(samples));
        if (distance_to_segment(point, previous, current) <= radius) return true;
        previous = current;
    }
    return false;
}

void rdp(const std::vector<PointF>& input, std::size_t first, std::size_t last,
         float epsilon, std::vector<bool>& keep) {
    if (last <= first + 1) return;
    float maximum = 0.0F;
    std::size_t selected = first;
    for (std::size_t index = first + 1; index < last; ++index) {
        const float value = distance_to_segment(input[index], input[first], input[last]);
        if (value > maximum) {
            maximum = value;
            selected = index;
        }
    }
    if (maximum > epsilon) {
        keep[selected] = true;
        rdp(input, first, selected, epsilon, keep);
        rdp(input, selected, last, epsilon, keep);
    }
}

}  // namespace

bool hit_test(const Drawable& item, PointF point, float tolerance) noexcept {
    if (item.points.empty()) return false;
    if (!item.bounds().contains(point)) return false;

    if (item.kind == Tool::Text) return item.bounds().contains(point);
    if (item.kind == Tool::CurvedArrow) return curved_arrow_hit(item, point, tolerance);
    if (item.kind == Tool::Pen || item.kind == Tool::Highlighter ||
        item.kind == Tool::Line || item.kind == Tool::Arrow) {
        return path_hit(item, point, tolerance);
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
        const float normalized = std::sqrt(((point.x - cx) * (point.x - cx)) / (rx * rx) +
                                           ((point.y - cy) * (point.y - cy)) / (ry * ry));
        const float normalized_tolerance = radius / std::max(rx, ry);
        return std::abs(normalized - 1.0F) <= normalized_tolerance;
    }
    return path_hit(item, point, tolerance);
}

std::vector<PointF> simplify_path(const std::vector<PointF>& input, float epsilon) {
    if (input.size() < 3 || epsilon <= 0.0F) return input;
    std::vector<bool> keep(input.size(), false);
    keep.front() = true;
    keep.back() = true;
    rdp(input, 0, input.size() - 1, epsilon, keep);
    std::vector<PointF> output;
    output.reserve(input.size());
    for (std::size_t index = 0; index < input.size(); ++index) {
        if (keep[index]) output.push_back(input[index]);
    }
    return output;
}

Document::Document(std::size_t history_limit)
    : history_limit_(std::max<std::size_t>(history_limit, 1)) {}

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
                                             operation.entries.begin(), operation.entries.end());
        return;
    }
    apply(operation);
    undo_.push_back(std::move(operation));
    redo_.clear();
    if (undo_.size() > history_limit_) {
        undo_.erase(undo_.begin(), undo_.begin() + static_cast<std::ptrdiff_t>(undo_.size() - history_limit_));
    }
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
    undo_.push_back(std::move(*compound_operation_));
    compound_operation_.reset();
    if (undo_.size() > history_limit_) {
        undo_.erase(undo_.begin(), undo_.begin() +
            static_cast<std::ptrdiff_t>(undo_.size() - history_limit_));
    }
}

void Document::add(Drawable drawable) {
    if (drawable.points.empty()) return;
    Operation operation;
    operation.kind = OperationKind::Add;
    operation.entries.push_back({items_.size(), std::move(drawable)});
    commit(std::move(operation));
}

bool Document::erase_at(PointF point, float tolerance) {
    for (std::size_t reverse = items_.size(); reverse > 0; --reverse) {
        const std::size_t index = reverse - 1;
        if (hit_test(items_[index], point, tolerance)) {
            Operation operation;
            operation.kind = OperationKind::Remove;
            operation.entries.push_back({index, items_[index]});
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
        operation.entries.push_back({index, items_[index]});
    }
    commit(std::move(operation));
    return true;
}

void Document::apply(const Operation& operation) {
    if (operation.kind == OperationKind::Add) {
        for (const auto& entry : operation.entries) {
            items_.insert(items_.begin() + static_cast<std::ptrdiff_t>(std::min(entry.index, items_.size())),
                          entry.drawable);
        }
    } else if (operation.kind == OperationKind::Remove) {
        for (auto iterator = operation.entries.rbegin(); iterator != operation.entries.rend(); ++iterator) {
            if (iterator->index < items_.size()) {
                items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(iterator->index));
            }
        }
    } else {
        items_.clear();
    }
}

void Document::revert(const Operation& operation) {
    if (operation.kind == OperationKind::Add) {
        for (auto iterator = operation.entries.rbegin(); iterator != operation.entries.rend(); ++iterator) {
            if (iterator->index < items_.size()) {
                items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(iterator->index));
            }
        }
    } else {
        for (const auto& entry : operation.entries) {
            items_.insert(items_.begin() + static_cast<std::ptrdiff_t>(std::min(entry.index, items_.size())),
                          entry.drawable);
        }
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
