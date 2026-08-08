#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
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

struct Drawable {
    Tool kind{Tool::Pen};
    Color color{kBlack};
    float width{7.0F};
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
[[nodiscard]] bool hit_test(const Drawable& item, PointF point, float tolerance = 3.0F) noexcept;
[[nodiscard]] std::vector<PointF> simplify_path(const std::vector<PointF>& input,
                                                float epsilon);

struct IndexedDrawable {
    std::size_t index{};
    Drawable drawable;
};

class Document {
public:
    explicit Document(std::size_t history_limit = 256);

    [[nodiscard]] const std::vector<Drawable>& items() const noexcept { return items_; }
    [[nodiscard]] bool empty() const noexcept { return items_.empty(); }
    [[nodiscard]] bool can_undo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] bool can_redo() const noexcept { return !redo_.empty(); }
    [[nodiscard]] std::size_t history_size() const noexcept { return undo_.size(); }

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
    void apply(const Operation& operation);
    void revert(const Operation& operation);

    std::vector<Drawable> items_;
    std::vector<Operation> undo_;
    std::vector<Operation> redo_;
    std::size_t history_limit_;
    bool compound_active_{};
    std::optional<Operation> compound_operation_;
};

struct AppState {
    Document document;
    Tool tool{Tool::Pen};
    Color color{kBlack};
    float thickness{7.0F};
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
