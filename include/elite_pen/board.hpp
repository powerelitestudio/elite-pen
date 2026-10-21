#pragma once

#include "elite_pen/core.hpp"
#include <filesystem>
#include <memory>
#include <span>

namespace elite_pen {

// A board has its own objects, camera and history. Desktop documents never enter it.
struct BoardImage {
    std::uint32_t width{}, height{};
    std::vector<std::uint8_t> png;
};

struct BoardObject {
    std::uint64_t id{};
    Drawable ink;
    std::vector<float> pressures;
    std::shared_ptr<const BoardImage> image;
    RectF image_bounds{};
    bool locked{};
    std::wstring font; // Empty for legacy text; otherwise a Board-only text style.
    float font_size{28};
    RectF text_bounds{};
    [[nodiscard]] RectF bounds() const noexcept {
        return image ? image_bounds : (!font.empty() ? text_bounds : ink.bounds());
    }
};

using BoardObjects = std::vector<std::shared_ptr<const BoardObject>>;
using BoardIds = std::vector<std::uint64_t>;

struct BoardCamera {
    PointF origin{};
    float zoom{1.0F};
    [[nodiscard]] PointF to_world(PointF view) const noexcept;
    [[nodiscard]] PointF to_view(PointF world) const noexcept;
    void pan(PointF screen_delta) noexcept;
    void zoom_at(PointF view, float factor) noexcept;
    void frame(RectF bounds, PointF viewport) noexcept;
};

[[nodiscard]] float board_pressure_width(float pressure) noexcept;
[[nodiscard]] bool valid_board_object(const BoardObject &object) noexcept;
[[nodiscard]] BoardObject transform_board_object(const BoardObject &object, RectF from, RectF to);

class BoardDocument {
  public:
    [[nodiscard]] const BoardObjects &objects() const noexcept { return objects_; }
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    [[nodiscard]] bool append_only() const noexcept { return append_only_; }
    [[nodiscard]] bool can_undo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] bool can_redo() const noexcept { return !redo_.empty(); }
    [[nodiscard]] RectF bounds() const noexcept;
    [[nodiscard]] std::shared_ptr<const BoardObject> hit(PointF point, float tolerance = 5) const;
    [[nodiscard]] BoardIds in_area(RectF area) const;
    [[nodiscard]] BoardIds all_ids() const;
    [[nodiscard]] RectF selection_bounds(std::span<const std::uint64_t> ids) const;
    bool add(BoardObject object);
    bool replace(std::span<const std::uint64_t> ids, BoardObject object);
    bool erase(PointF point, float tolerance);
    bool remove(std::span<const std::uint64_t> ids);
    bool transform(std::span<const std::uint64_t> ids, RectF from, RectF to);
    bool duplicate(std::span<const std::uint64_t> ids);
    bool lock(std::span<const std::uint64_t> ids, bool locked);
    bool reorder(std::span<const std::uint64_t> ids, bool front);
    bool clear();
    bool undo();
    bool redo();
    void restore(BoardObjects objects);
    void begin_compound();
    void end_compound();

  private:
    void commit(BoardObjects next, bool append = false);
    BoardObjects objects_;
    std::vector<BoardObjects> undo_, redo_;
    std::uint64_t revision_{}, next_id_{1};
    bool append_only_{}, compound_{};
    std::optional<BoardObjects> compound_before_;
};

struct BoardFile {
    BoardObjects objects;
    BoardCamera camera;
    bool dark{};
    std::optional<Color> background;
    bool auto_text{};
    std::wstring text_font{L"Palitoon"};
    float text_size{28};
    bool pressure{true};
    bool grid{};
    bool panel_visible{true};
    std::uint32_t panel_scale{100};
    float pen_width{4};
    Color pen_color{kBlack};
    std::string source_path; // UTF-8, recovery metadata only; never opened implicitly.
};

// Versioned, checksummed and self-contained; images are embedded once per asset.
[[nodiscard]] std::vector<std::uint8_t> encode_board(const BoardFile &file);
[[nodiscard]] bool decode_board(std::span<const std::uint8_t> bytes, BoardFile &file, std::string &error);
[[nodiscard]] bool read_board(const std::filesystem::path &path, BoardFile &file, std::string &error,
                              bool allow_previous = true);
[[nodiscard]] bool write_board(const std::filesystem::path &path, const BoardFile &file, std::string &error);

} // namespace elite_pen
