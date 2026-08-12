#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace elite_pen {

struct PointF {
    float x{};
    float y{};
};

struct RectF {
    float left{};
    float top{};
    float right{};
    float bottom{};

    [[nodiscard]] float width() const noexcept { return right - left; }
    [[nodiscard]] float height() const noexcept { return bottom - top; }
    [[nodiscard]] bool contains(PointF p) const noexcept {
        return p.x >= left && p.x <= right && p.y >= top && p.y <= bottom;
    }
    [[nodiscard]] bool intersects(const RectF& other) const noexcept {
        return right >= other.left && left <= other.right &&
               bottom >= other.top && top <= other.bottom;
    }
};

// Maps between the pixels presented by a zoom viewport and the desktop pixels
// used as its source. Keeping this transform in the platform-independent core
// makes anchored zoom annotations deterministic on negative-coordinate and
// multi-monitor desktops as well as at fractional zoom factors.
struct ZoomViewportTransform {
    RectF source{};
    float scale{1.0F};

    [[nodiscard]] PointF view_to_source(PointF point) const noexcept;
    [[nodiscard]] PointF source_to_view(PointF point) const noexcept;
    [[nodiscard]] float view_to_source_length(float length) const noexcept;
    [[nodiscard]] float source_to_view_length(float length) const noexcept;
};

struct Color {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{255};

    constexpr bool operator==(const Color&) const noexcept = default;
    [[nodiscard]] constexpr std::uint32_t argb() const noexcept {
        return (static_cast<std::uint32_t>(a) << 24U) |
               (static_cast<std::uint32_t>(r) << 16U) |
               (static_cast<std::uint32_t>(g) << 8U) |
               static_cast<std::uint32_t>(b);
    }
};

inline constexpr Color kBlack{24, 24, 27, 255};
inline constexpr Color kYellow{255, 190, 45, 255};
inline constexpr Color kBlue{31, 136, 229, 255};
inline constexpr Color kRed{239, 68, 68, 255};
inline constexpr Color kGreen{34, 197, 94, 255};
inline constexpr Color kPurple{139, 92, 246, 255};

enum class Tool : std::uint8_t {
    Interact,
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
    Zoom
};

[[nodiscard]] const wchar_t* tool_name(Tool tool) noexcept;
[[nodiscard]] bool is_drawing_tool(Tool tool) noexcept;
[[nodiscard]] Tool gesture_tool(Tool selected, bool shift, bool control,
                                bool alt, bool tab) noexcept;

struct Drawable {
    Tool kind{Tool::Pen};
    Color color{kBlack};
    float width{7.0F};
    float reference_scale{1.0F};
    std::vector<PointF> points;
    std::wstring text;

    [[nodiscard]] RectF bounds() const noexcept;
    void invalidate_bounds_cache() noexcept { bounds_cached_ = false; }

private:
    mutable RectF cached_bounds_{};
    mutable bool bounds_cached_{};
};

[[nodiscard]] float distance(PointF a, PointF b) noexcept;
[[nodiscard]] float distance_to_segment(PointF p, PointF a, PointF b) noexcept;

struct CubicBezier {
    PointF start{};
    PointF control1{};
    PointF control2{};
    PointF end{};
};

struct ArrowHead {
    PointF left{};
    PointF right{};
};

[[nodiscard]] CubicBezier curved_arrow_bezier(
    PointF start, PointF end, float reference_scale = 1.0F) noexcept;
[[nodiscard]] PointF cubic_bezier_point(const CubicBezier& curve, float t) noexcept;
[[nodiscard]] ArrowHead arrow_head_points(PointF before, PointF end,
                                          float width,
                                          float reference_scale = 1.0F) noexcept;
[[nodiscard]] bool hit_test(const Drawable& item, PointF point, float tolerance = 3.0F) noexcept;
[[nodiscard]] std::vector<PointF> simplify_path(const std::vector<PointF>& input,
                                                float epsilon);

struct IndexedDrawable {
    std::size_t index{};
    Drawable drawable;
};

enum class DocumentChangeKind : std::uint8_t {
    None,
    Append,
    Insert,
    Remove,
    Clear,
    Rebuild,
};

class Document {
public:
    explicit Document(std::size_t history_limit = 256);

    [[nodiscard]] const std::vector<Drawable>& items() const noexcept { return items_; }
    [[nodiscard]] bool empty() const noexcept { return items_.empty(); }
    [[nodiscard]] bool can_undo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] bool can_redo() const noexcept { return !redo_.empty(); }
    [[nodiscard]] std::size_t history_size() const noexcept { return undo_.size(); }
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    [[nodiscard]] DocumentChangeKind last_change_kind() const noexcept {
        return last_change_kind_;
    }
    [[nodiscard]] std::size_t last_change_index() const noexcept {
        return last_change_index_;
    }

    void add(Drawable drawable);
    bool erase_at(PointF point, float tolerance = 6.0F);
    void begin_compound();
    void end_compound();
    bool clear();
    bool undo();
    bool redo();

private:
    enum class OperationKind { Add, Remove, Clear };
    struct Operation {
        OperationKind kind{OperationKind::Add};
        std::vector<IndexedDrawable> entries;
    };

    void commit(Operation operation);
    void apply(Operation& operation);
    void revert(Operation& operation);
    void changed(DocumentChangeKind kind, std::size_t index = 0) noexcept {
        ++revision_;
        last_change_kind_ = kind;
        last_change_index_ = index;
    }

    std::vector<Drawable> items_;
    std::deque<Operation> undo_;
    std::deque<Operation> redo_;
    std::size_t history_limit_;
    std::uint64_t revision_{};
    DocumentChangeKind last_change_kind_{DocumentChangeKind::None};
    std::size_t last_change_index_{};
    bool compound_active_{};
    std::optional<Operation> compound_operation_;
};

struct AppState {
    Document document;
    Tool tool{Tool::Pen};
    Color color{kBlack};
    float thickness{4.0F};
    bool annotations_visible{true};
    bool whiteboard{false};
    bool blackboard{false};
    bool cursor_highlight{false};
    float zoom_factor{2.0F};

    [[nodiscard]] float effective_width() const noexcept {
        return tool == Tool::Highlighter ? thickness * 2.6F : thickness;
    }
};

}  // namespace elite_pen
