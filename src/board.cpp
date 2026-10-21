#include "elite_pen/board.hpp"
#include <bit>
#include <fstream>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#ifdef _WIN32
#include <windows.h>
#endif

namespace elite_pen {
namespace {
constexpr std::size_t kMaxFile = 128U * 1024U * 1024U;
constexpr std::size_t kMaxPoints = 2000000;
bool finite(float value) { return std::isfinite(value) && std::abs(value) <= 10000000.0F; }
bool valid_font(const std::wstring &font) {
    return !font.empty() && font.size() <= 128 &&
           std::all_of(font.begin(), font.end(), [](wchar_t c) { return c >= 32 && c != 127; });
}
bool valid_rect(RectF r) {
    return finite(r.left) && finite(r.top) && finite(r.right) && finite(r.bottom) && r.width() > 0 &&
           r.height() > 0;
}
class IdSelection {
  public:
    explicit IdSelection(std::span<const std::uint64_t> ids) : ids_(ids) {
        if (ids.size() > 16)
            indexed_.insert(ids.begin(), ids.end());
    }
    bool contains(std::uint64_t id) const {
        return ids_.size() > 16 ? indexed_.contains(id)
                                : std::find(ids_.begin(), ids_.end(), id) != ids_.end();
    }

  private:
    std::span<const std::uint64_t> ids_;
    std::unordered_set<std::uint64_t> indexed_;
};
bool within_save_budget(const BoardObjects &objects) {
    if (objects.size() > 100000)
        return false;
    std::size_t points{}, bytes{};
    std::unordered_set<const BoardImage *> images;
    for (const auto &object : objects) {
        points += object->ink.points.size();
        bytes += 128 + object->ink.points.size() * 12 + object->ink.text.size() * 4 + object->font.size() * 4;
        if (object->image && images.insert(object->image.get()).second)
            bytes += object->image->png.size();
        if (points > kMaxPoints || bytes > 120U * 1024U * 1024U)
            return false;
    }
    return true;
}
RectF combine(RectF a, RectF b) {
    return {std::min(a.left, b.left), std::min(a.top, b.top), std::max(a.right, b.right),
            std::max(a.bottom, b.bottom)};
}
void keep_history(std::vector<BoardObjects> &history, BoardObjects state) {
    if (history.size() >= 128)
        history.erase(history.begin());
    history.push_back(std::move(state));
}
std::uint64_t digest(std::span<const std::uint8_t> bytes) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (auto byte : bytes) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    return hash;
}
struct Writer {
    std::vector<std::uint8_t> data;
    void number(std::uint64_t value, int count) {
        for (int n = 0; n < count; ++n)
            data.push_back(static_cast<std::uint8_t>(value >> (n * 8)));
    }
    void real(float f) { number(std::bit_cast<std::uint32_t>(f), 4); }
    void bytes(std::span<const std::uint8_t> bytes) {
        if (bytes.size() > kMaxFile || data.size() > kMaxFile - bytes.size())
            throw std::runtime_error("La pizarra supera el límite de 128 MiB.");
        data.insert(data.end(), bytes.begin(), bytes.end());
    }
    void text(const std::wstring &value) {
        number(value.size(), 4);
        for (auto c : value)
            number(static_cast<std::uint32_t>(c), 4);
    }
    void rect(RectF r) {
        real(r.left);
        real(r.top);
        real(r.right);
        real(r.bottom);
    }
};
struct Reader {
    std::span<const std::uint8_t> data;
    std::size_t cursor{};
    std::uint64_t number(int count) {
        if (static_cast<std::size_t>(count) > data.size() - cursor)
            throw std::runtime_error("Archivo incompleto.");
        std::uint64_t value{};
        for (int n = 0; n < count; ++n)
            value |= static_cast<std::uint64_t>(data[cursor++]) << (n * 8);
        return value;
    }
    std::size_t count(std::size_t max) {
        const auto n = number(4);
        if (n > max)
            throw std::runtime_error("Tamaño no admitido.");
        return static_cast<std::size_t>(n);
    }
    float real() { return std::bit_cast<float>(static_cast<std::uint32_t>(number(4))); }
    std::wstring text() {
        std::wstring value;
        const auto length = count(128);
        for (std::size_t i = 0; i < length; ++i) {
            const auto c = number(4);
            if (c > 0xffff)
                throw std::runtime_error("Fuente no válida.");
            value.push_back(static_cast<wchar_t>(c));
        }
        if (!valid_font(value))
            throw std::runtime_error("Fuente no válida.");
        return value;
    }
    Color color() {
        const auto c = static_cast<std::uint32_t>(number(4));
        return {static_cast<std::uint8_t>(c >> 16), static_cast<std::uint8_t>(c >> 8),
                static_cast<std::uint8_t>(c), static_cast<std::uint8_t>(c >> 24)};
    }
    RectF rect() {
        const float l = real(), t = real(), r = real(), b = real();
        return {l, t, r, b};
    }
    std::vector<std::uint8_t> bytes(std::size_t count) {
        if (count > data.size() - cursor)
            throw std::runtime_error("Datos incompletos.");
        std::vector<std::uint8_t> result(data.begin() + static_cast<std::ptrdiff_t>(cursor),
                                         data.begin() + static_cast<std::ptrdiff_t>(cursor + count));
        cursor += count;
        return result;
    }
};
} // namespace

float board_pressure_width(float pressure) noexcept {
    return 0.2F + 0.8F * std::sqrt(std::clamp(std::isfinite(pressure) ? pressure : 0.5F, 0.0F, 1.0F));
}
PointF BoardCamera::to_world(PointF p) const noexcept {
    return {origin.x + p.x / zoom, origin.y + p.y / zoom};
}
PointF BoardCamera::to_view(PointF p) const noexcept {
    return {(p.x - origin.x) * zoom, (p.y - origin.y) * zoom};
}
void BoardCamera::pan(PointF d) noexcept {
    origin.x = std::clamp(origin.x - d.x / zoom, -9000000.0F, 9000000.0F);
    origin.y = std::clamp(origin.y - d.y / zoom, -9000000.0F, 9000000.0F);
}
void BoardCamera::zoom_at(PointF view, float factor) noexcept {
    if (!std::isfinite(factor) || factor <= 0)
        return;
    const auto fixed = to_world(view);
    zoom = std::clamp(zoom * factor, 0.03125F, 32.0F);
    origin = {fixed.x - view.x / zoom, fixed.y - view.y / zoom};
    origin.x = std::clamp(origin.x, -9000000.0F, 9000000.0F);
    origin.y = std::clamp(origin.y, -9000000.0F, 9000000.0F);
}
void BoardCamera::frame(RectF box, PointF viewport) noexcept {
    if (!valid_rect(box) || viewport.x < 100 || viewport.y < 100) {
        origin = {};
        zoom = 1;
        return;
    }
    zoom = std::clamp(std::min((viewport.x - 80) / box.width(), (viewport.y - 80) / box.height()), 0.03125F,
                      8.0F);
    origin = {(box.left + box.right - viewport.x / zoom) * 0.5F,
              (box.top + box.bottom - viewport.y / zoom) * 0.5F};
}
bool valid_board_object(const BoardObject &o) noexcept {
    if (!o.font.empty() &&
        (o.image || o.ink.kind != Tool::Text || !valid_font(o.font) || !std::isfinite(o.font_size) ||
         o.font_size < 1 || o.font_size > 10000 || !valid_rect(o.text_bounds)))
        return false;
    if (o.image)
        return valid_rect(o.image_bounds) && o.image->width && o.image->height && o.image->width <= 16384 &&
               o.image->height <= 16384 &&
               static_cast<std::uint64_t>(o.image->width) * o.image->height <= 64000000 &&
               o.image->png.size() >= 8 && o.image->png.size() <= 64U * 1024U * 1024U &&
               std::equal(o.image->png.begin(), o.image->png.begin() + 8,
                          std::array<std::uint8_t, 8>{137, 80, 78, 71, 13, 10, 26, 10}.begin());
    if (static_cast<std::uint8_t>(o.ink.kind) > static_cast<std::uint8_t>(Tool::Emoji) ||
        !is_drawing_tool(o.ink.kind) || o.ink.kind == Tool::Screenshot || o.ink.kind == Tool::Eraser ||
        o.ink.points.empty() || o.ink.points.size() > kMaxPoints || !std::isfinite(o.ink.width) ||
        o.ink.width < 0.01F || o.ink.width > 10000 || !std::isfinite(o.ink.reference_scale) ||
        o.ink.reference_scale <= 0 || o.ink.reference_scale > 10000 || o.ink.text.size() > 1000000)
        return false;
    if (!o.pressures.empty() && o.pressures.size() != o.ink.points.size())
        return false;
    for (auto p : o.ink.points)
        if (!finite(p.x) || !finite(p.y))
            return false;
    for (auto p : o.pressures)
        if (!std::isfinite(p) || p < 0 || p > 1)
            return false;
    return true;
}
BoardObject transform_board_object(const BoardObject &object, RectF from, RectF to) {
    BoardObject result = object;
    if (!valid_rect(from) || !valid_rect(to))
        return result;
    const float sx = to.width() / from.width(), sy = to.height() / from.height();
    const auto map = [&](PointF p) {
        return PointF{to.left + (p.x - from.left) * sx, to.top + (p.y - from.top) * sy};
    };
    if (result.image) {
        const auto a = map({result.image_bounds.left, result.image_bounds.top}),
                   b = map({result.image_bounds.right, result.image_bounds.bottom});
        result.image_bounds = {a.x, a.y, b.x, b.y};
    } else {
        for (auto &p : result.ink.points)
            p = map(p);
        const float scale = std::sqrt(sx * sy);
        result.ink.width = std::clamp(result.ink.width * scale, 0.01F, 10000.0F);
        result.ink.reference_scale = std::clamp(result.ink.reference_scale / scale, 0.001F, 10000.0F);
        result.ink.invalidate_bounds_cache();
        if (!result.font.empty()) {
            const auto a = map({result.text_bounds.left, result.text_bounds.top});
            const auto b = map({result.text_bounds.right, result.text_bounds.bottom});
            result.text_bounds = {a.x, a.y, b.x, b.y};
            result.font_size = std::clamp(result.font_size * scale, 1.0F, 10000.0F);
        }
    }
    return result;
}
RectF BoardDocument::bounds() const noexcept {
    std::optional<RectF> r;
    for (const auto &o : objects_)
        r = r ? combine(*r, o->bounds()) : o->bounds();
    return r.value_or(RectF{});
}
BoardIds BoardDocument::all_ids() const {
    BoardIds ids;
    for (const auto &o : objects_)
        ids.push_back(o->id);
    return ids;
}
RectF BoardDocument::selection_bounds(std::span<const std::uint64_t> ids) const {
    const IdSelection selection(ids);
    std::optional<RectF> r;
    for (const auto &o : objects_)
        if (selection.contains(o->id))
            r = r ? combine(*r, o->bounds()) : o->bounds();
    return r.value_or(RectF{});
}
std::shared_ptr<const BoardObject> BoardDocument::hit(PointF p, float tolerance) const {
    for (auto it = objects_.rbegin(); it != objects_.rend(); ++it) {
        const auto &o = *it;
        if ((o->image || !o->font.empty()) ? o->bounds().contains(p) : hit_test(o->ink, p, tolerance))
            return o;
    }
    return {};
}
BoardIds BoardDocument::in_area(RectF r) const {
    BoardIds ids;
    for (const auto &o : objects_)
        if (!o->locked && o->bounds().intersects(r))
            ids.push_back(o->id);
    return ids;
}
void BoardDocument::commit(BoardObjects next, bool append) {
    if (compound_) {
        if (!compound_before_)
            compound_before_ = objects_;
    } else
        keep_history(undo_, objects_);
    redo_.clear();
    objects_ = std::move(next);
    ++revision_;
    append_only_ = append;
}
void BoardDocument::begin_compound() {
    end_compound();
    compound_ = true;
}
void BoardDocument::end_compound() {
    if (compound_before_) {
        keep_history(undo_, std::move(*compound_before_));
        compound_before_.reset();
    }
    compound_ = false;
}
bool BoardDocument::add(BoardObject o) {
    if (objects_.size() >= 100000 || !valid_board_object(o))
        return false;
    o.id = next_id_++;
    auto next = objects_;
    next.push_back(std::make_shared<const BoardObject>(std::move(o)));
    if (!within_save_budget(next))
        return false;
    commit(std::move(next), true);
    return true;
}
bool BoardDocument::replace(std::span<const std::uint64_t> ids, BoardObject object) {
    if (ids.empty() || !valid_board_object(object))
        return false;
    const std::unordered_set<std::uint64_t> selected(ids.begin(), ids.end());
    if (selected.size() != ids.size())
        return false;
    std::size_t found{};
    for (const auto &o : objects_)
        if (selected.contains(o->id)) {
            if (o->locked)
                return false;
            ++found;
        }
    if (found != ids.size())
        return false;
    BoardObjects next;
    bool inserted{};
    for (const auto &o : objects_) {
        if (!selected.contains(o->id))
            next.push_back(o);
        else if (!inserted) {
            object.id = o->id;
            next.push_back(std::make_shared<const BoardObject>(std::move(object)));
            inserted = true;
        }
    }
    if (!within_save_budget(next))
        return false;
    commit(std::move(next));
    return true;
}
bool BoardDocument::remove(std::span<const std::uint64_t> ids) {
    const IdSelection selection(ids);
    auto next = objects_;
    std::erase_if(next, [&](const auto &o) { return !o->locked && selection.contains(o->id); });
    if (next.size() == objects_.size())
        return false;
    commit(std::move(next));
    return true;
}
bool BoardDocument::erase(PointF p, float tolerance) {
    const auto o = hit(p, tolerance);
    if (!o || o->locked)
        return false;
    return remove(BoardIds{o->id});
}
bool BoardDocument::transform(std::span<const std::uint64_t> ids, RectF from, RectF to) {
    const IdSelection selection(ids);
    if (!valid_rect(from) || !valid_rect(to))
        return false;
    auto next = objects_;
    bool changed = false;
    for (auto &p : next)
        if (!p->locked && selection.contains(p->id)) {
            auto o = transform_board_object(*p, from, to);
            if (!valid_board_object(o))
                return false;
            p = std::make_shared<const BoardObject>(std::move(o));
            changed = true;
        }
    if (changed)
        commit(std::move(next));
    return changed;
}
bool BoardDocument::duplicate(std::span<const std::uint64_t> ids) {
    const IdSelection selection(ids);
    auto next = objects_;
    for (const auto &o : objects_)
        if (selection.contains(o->id)) {
            auto copy = transform_board_object(*o, {0, 0, 1, 1}, {24, 24, 25, 25});
            copy.id = next_id_++;
            copy.locked = false;
            if (valid_board_object(copy))
                next.push_back(std::make_shared<const BoardObject>(std::move(copy)));
        }
    if (next.size() == objects_.size() || !within_save_budget(next))
        return false;
    commit(std::move(next));
    return true;
}
bool BoardDocument::lock(std::span<const std::uint64_t> ids, bool locked) {
    const IdSelection selection(ids);
    auto next = objects_;
    bool changed = false;
    for (auto &o : next)
        if (selection.contains(o->id) && o->locked != locked) {
            auto n = *o;
            n.locked = locked;
            o = std::make_shared<const BoardObject>(std::move(n));
            changed = true;
        }
    if (changed)
        commit(std::move(next));
    return changed;
}
bool BoardDocument::reorder(std::span<const std::uint64_t> ids, bool front) {
    const IdSelection selection(ids);
    BoardObjects selected, other;
    for (const auto &o : objects_)
        (selection.contains(o->id) ? selected : other).push_back(o);
    if (selected.empty())
        return false;
    if (front)
        other.insert(other.end(), selected.begin(), selected.end());
    else {
        selected.insert(selected.end(), other.begin(), other.end());
        other = std::move(selected);
    }
    if (other == objects_)
        return false;
    commit(std::move(other));
    return true;
}
bool BoardDocument::clear() {
    if (objects_.empty())
        return false;
    commit({});
    return true;
}
bool BoardDocument::undo() {
    end_compound();
    if (undo_.empty())
        return false;
    keep_history(redo_, objects_);
    objects_ = std::move(undo_.back());
    undo_.pop_back();
    ++revision_;
    append_only_ = false;
    return true;
}
bool BoardDocument::redo() {
    end_compound();
    if (redo_.empty())
        return false;
    keep_history(undo_, objects_);
    objects_ = std::move(redo_.back());
    redo_.pop_back();
    ++revision_;
    append_only_ = false;
    return true;
}
void BoardDocument::restore(BoardObjects objects) {
    objects_ = std::move(objects);
    undo_.clear();
    redo_.clear();
    compound_before_.reset();
    compound_ = false;
    next_id_ = 1;
    for (const auto &o : objects_)
        next_id_ = std::max(next_id_, o->id + 1);
    ++revision_;
    append_only_ = false;
}

std::vector<std::uint8_t> encode_board(const BoardFile &file) {
    if (file.objects.size() > 100000 || !finite(file.camera.origin.x) || !finite(file.camera.origin.y) ||
        !std::isfinite(file.camera.zoom) || file.camera.zoom < 0.03125F || file.camera.zoom > 32 ||
        file.source_path.size() > 32768 ||
        (file.panel_scale != 80 && file.panel_scale != 100 && file.panel_scale != 125 &&
         file.panel_scale != 150 && file.panel_scale != 200 && file.panel_scale != 300) ||
        !std::isfinite(file.pen_width) || file.pen_width < .5F || file.pen_width > 100 ||
        !valid_font(file.text_font) || !std::isfinite(file.text_size) || file.text_size < 8 ||
        file.text_size > 256 || (file.background && file.background->a != 255))
        throw std::runtime_error("Pizarra no válida.");
    Writer p;
    p.real(file.camera.origin.x);
    p.real(file.camera.origin.y);
    p.real(file.camera.zoom);
    p.number((file.dark ? 1U : 0U) | (file.pressure ? 2U : 0U) | (file.grid ? 4U : 0U) |
                 (file.panel_visible ? 8U : 0U),
             4);
    p.number(file.panel_scale, 4);
    p.real(file.pen_width);
    p.number(file.pen_color.argb(), 4);
    p.number(file.source_path.size(), 4);
    p.bytes({reinterpret_cast<const std::uint8_t *>(file.source_path.data()), file.source_path.size()});
    p.number(file.background ? 1 : 0, 1);
    if (file.background)
        p.number(file.background->argb(), 4);
    p.number(file.auto_text ? 1 : 0, 1);
    p.text(file.text_font);
    p.real(file.text_size);
    std::unordered_map<const BoardImage *, std::size_t> ids;
    std::vector<std::shared_ptr<const BoardImage>> images;
    std::unordered_set<std::uint64_t> object_ids;
    for (const auto &o : file.objects) {
        if (!o || !valid_board_object(*o) || !o->id || o->id > 1000000000 || !object_ids.insert(o->id).second)
            throw std::runtime_error("Objeto no válido.");
        if (o->image && !ids.contains(o->image.get())) {
            ids[o->image.get()] = images.size();
            images.push_back(o->image);
        }
    }
    p.number(images.size(), 4);
    for (const auto &image : images) {
        p.number(image->width, 4);
        p.number(image->height, 4);
        p.number(image->png.size(), 4);
        p.bytes(image->png);
    }
    p.number(file.objects.size(), 4);
    std::size_t points{};
    for (const auto &o : file.objects) {
        p.number(o->id, 8);
        p.number(o->locked ? 1 : 0, 1);
        p.number(o->image ? 1 : 0, 1);
        p.number(o->font.empty() ? 0 : 1, 1);
        if (!o->font.empty()) {
            p.text(o->font);
            p.real(o->font_size);
            p.rect(o->text_bounds);
        }
        if (o->image) {
            p.number(ids.at(o->image.get()), 4);
            p.rect(o->image_bounds);
            continue;
        }
        const auto &ink = o->ink;
        p.number(static_cast<std::uint8_t>(ink.kind), 1);
        p.number(ink.color.r, 1);
        p.number(ink.color.g, 1);
        p.number(ink.color.b, 1);
        p.number(ink.color.a, 1);
        p.real(ink.width);
        p.real(ink.reference_scale);
        p.number(ink.text.size(), 4);
        for (auto c : ink.text)
            p.number(static_cast<std::uint32_t>(c), 4);
        points += ink.points.size();
        if (points > kMaxPoints)
            throw std::runtime_error("Demasiados puntos en la pizarra.");
        p.number(ink.points.size(), 4);
        p.number(o->pressures.empty() ? 0 : 1, 1);
        for (std::size_t n = 0; n < ink.points.size(); ++n) {
            p.real(ink.points[n].x);
            p.real(ink.points[n].y);
            if (!o->pressures.empty())
                p.real(o->pressures[n]);
        }
        if (p.data.size() > kMaxFile)
            throw std::runtime_error("Pizarra demasiado grande.");
    }
    Writer w;
    const std::array<std::uint8_t, 8> magic{'E', 'P', 'B', 'O', 'A', 'R', 'D', 0};
    w.bytes(magic);
    w.number(2, 4);
    w.number(p.data.size(), 8);
    w.number(digest(p.data), 8);
    w.bytes(p.data);
    return w.data;
}
bool decode_board(std::span<const std::uint8_t> bytes, BoardFile &file, std::string &error) {
    try {
        if (bytes.size() < 28 || bytes.size() > kMaxFile + 28)
            throw std::runtime_error("Tamaño de archivo no válido.");
        Reader r{bytes};
        const auto magic = r.bytes(8);
        if (magic != std::vector<std::uint8_t>{'E', 'P', 'B', 'O', 'A', 'R', 'D', 0})
            throw std::runtime_error("No es un archivo de pizarra de Elite Pen.");
        const auto version = r.number(4);
        if (version != 1 && version != 2)
            throw std::runtime_error("Versión de pizarra no admitida.");
        const auto size = r.number(8), checksum = r.number(8);
        if (size != bytes.size() - 28 || checksum != digest(bytes.subspan(28)))
            throw std::runtime_error("Archivo incompleto o dañado.");
        BoardFile next;
        next.camera.origin = {r.real(), r.real()};
        next.camera.zoom = r.real();
        const auto flags = r.number(4);
        if (flags > 15 || !finite(next.camera.origin.x) || !finite(next.camera.origin.y) ||
            !std::isfinite(next.camera.zoom) || next.camera.zoom < 0.03125F || next.camera.zoom > 32)
            throw std::runtime_error("Vista no válida.");
        next.dark = (flags & 1) != 0;
        next.pressure = (flags & 2) != 0;
        next.grid = (flags & 4) != 0;
        next.panel_visible = (flags & 8) != 0;
        next.panel_scale = static_cast<std::uint32_t>(r.number(4));
        next.pen_width = r.real();
        const auto color = static_cast<std::uint32_t>(r.number(4));
        next.pen_color = {static_cast<std::uint8_t>(color >> 16), static_cast<std::uint8_t>(color >> 8),
                          static_cast<std::uint8_t>(color), static_cast<std::uint8_t>(color >> 24)};
        if ((next.panel_scale != 80 && next.panel_scale != 100 && next.panel_scale != 125 &&
             next.panel_scale != 150 && next.panel_scale != 200 && next.panel_scale != 300) ||
            !std::isfinite(next.pen_width) || next.pen_width < .5F || next.pen_width > 100)
            throw std::runtime_error("Ajustes de pizarra no válidos.");
        const auto path = r.bytes(r.count(32768));
        next.source_path.assign(path.begin(), path.end());
        if (version >= 2) {
            const auto custom = r.number(1);
            if (custom > 1)
                throw std::runtime_error("Fondo no válido.");
            if (custom) {
                next.background = r.color();
                if (next.background->a != 255)
                    throw std::runtime_error("Fondo no válido.");
            }
            const auto enabled = r.number(1);
            if (enabled > 1)
                throw std::runtime_error("Modo de escritura no válido.");
            next.auto_text = enabled != 0;
            next.text_font = r.text();
            next.text_size = r.real();
            if (!std::isfinite(next.text_size) || next.text_size < 8 || next.text_size > 256)
                throw std::runtime_error("Tamaño de texto no válido.");
        }
        std::vector<std::shared_ptr<const BoardImage>> images;
        const auto image_count = r.count(100000);
        for (std::size_t n = 0; n < image_count; ++n) {
            auto image = std::make_shared<BoardImage>();
            image->width = static_cast<std::uint32_t>(r.number(4));
            image->height = static_cast<std::uint32_t>(r.number(4));
            image->png = r.bytes(r.count(64U * 1024U * 1024U));
            BoardObject check;
            check.image = image;
            check.image_bounds = {0, 0, 1, 1};
            if (!valid_board_object(check))
                throw std::runtime_error("Imagen no válida.");
            images.push_back(image);
        }
        const auto count = r.count(100000);
        std::size_t points{};
        std::unordered_set<std::uint64_t> ids;
        for (std::size_t n = 0; n < count; ++n) {
            BoardObject o;
            o.id = r.number(8);
            const auto locked = r.number(1), kind = r.number(1);
            if (!o.id || o.id > 1000000000 || !ids.insert(o.id).second || locked > 1 || kind > 1)
                throw std::runtime_error("Identidad de objeto no válida.");
            o.locked = locked != 0;
            if (version >= 2) {
                const auto styled = r.number(1);
                if (styled > 1)
                    throw std::runtime_error("Estilo de texto no válido.");
                if (styled) {
                    o.font = r.text();
                    o.font_size = r.real();
                    o.text_bounds = r.rect();
                }
            }
            if (kind) {
                const auto index = r.number(4);
                if (index >= images.size())
                    throw std::runtime_error("Imagen ausente.");
                o.image = images[static_cast<std::size_t>(index)];
                o.image_bounds = r.rect();
            } else {
                o.ink.kind = static_cast<Tool>(r.number(1));
                o.ink.color = {static_cast<std::uint8_t>(r.number(1)), static_cast<std::uint8_t>(r.number(1)),
                               static_cast<std::uint8_t>(r.number(1)),
                               static_cast<std::uint8_t>(r.number(1))};
                o.ink.width = r.real();
                o.ink.reference_scale = r.real();
                const auto text = r.count(1000000);
                for (std::size_t c = 0; c < text; ++c) {
                    const auto unit = r.number(4);
                    if (unit > 0xffff)
                        throw std::runtime_error("Texto no válido.");
                    o.ink.text.push_back(static_cast<wchar_t>(unit));
                }
                const auto count_points = r.count(kMaxPoints);
                points += count_points;
                if (points > kMaxPoints)
                    throw std::runtime_error("Demasiados puntos.");
                const auto pressure = r.number(1);
                if (pressure > 1)
                    throw std::runtime_error("Presión no válida.");
                for (std::size_t v = 0; v < count_points; ++v) {
                    o.ink.points.push_back({r.real(), r.real()});
                    if (pressure)
                        o.pressures.push_back(r.real());
                }
            }
            if (!valid_board_object(o))
                throw std::runtime_error("Geometría de objeto no válida.");
            next.objects.push_back(std::make_shared<const BoardObject>(std::move(o)));
        }
        if (r.cursor != bytes.size())
            throw std::runtime_error("Datos sobrantes en el archivo.");
        file = std::move(next);
        error.clear();
        return true;
    } catch (const std::exception &ex) {
        error = ex.what();
        return false;
    }
}
bool read_board(const std::filesystem::path &path, BoardFile &file, std::string &error, bool allow_previous) {
    try {
        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec || size > kMaxFile + 28)
            throw std::runtime_error("No se pudo leer la pizarra.");
        std::ifstream in(path, std::ios::binary);
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        if (!in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
            throw std::runtime_error("Lectura incompleta.");
        if (decode_board(bytes, file, error))
            return true;
    } catch (const std::exception &ex) {
        error = ex.what();
    }
    if (allow_previous) {
        auto previous = path;
        previous += L".previous";
        std::string fallback;
        if (read_board(previous, file, fallback, false)) {
            error = "Se recuperó la copia anterior; el archivo principal estaba dañado.";
            return true;
        }
    }
    return false;
}
bool write_board(const std::filesystem::path &path, const BoardFile &file, std::string &error) {
    static std::mutex writers;
    const std::lock_guard<std::mutex> guard(writers);
    std::filesystem::path temp = path;
    temp += L".writing";
    try {
        const auto bytes = encode_board(file);
        std::error_code ec;
        if (!path.parent_path().empty()) {
            std::filesystem::create_directories(path.parent_path(), ec);
            if (ec)
                throw std::runtime_error("No se pudo crear la carpeta de la pizarra.");
        }
#ifdef _WIN32
        HANDLE handle = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            throw std::runtime_error("No se pudo escribir la pizarra.");
        bool okay = true;
        for (std::size_t offset = 0; offset < bytes.size();) {
            const DWORD count = static_cast<DWORD>(std::min<std::size_t>(bytes.size() - offset, 1024 * 1024));
            DWORD written{};
            if (!WriteFile(handle, bytes.data() + offset, count, &written, nullptr) || written != count) {
                okay = false;
                break;
            }
            offset += written;
        }
        if (okay)
            okay = FlushFileBuffers(handle) != FALSE;
        CloseHandle(handle);
        if (!okay)
            throw std::runtime_error("La pizarra no pudo guardarse por completo.");
        BoardFile previous_file;
        std::string ignored;
        if (read_board(path, previous_file, ignored, false)) {
            auto backup = path;
            backup += L".previous";
            if (!CopyFileW(path.c_str(), backup.c_str(), FALSE))
                throw std::runtime_error("No se pudo preservar la versión anterior.");
        }
        if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("No se pudo finalizar el guardado; el original se conserva.");
#else
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        out.close();
        if (!out)
            throw std::runtime_error("No se pudo guardar la pizarra.");
        if (std::filesystem::exists(path))
            std::filesystem::copy_file(path, path.string() + ".previous",
                                       std::filesystem::copy_options::overwrite_existing);
        std::filesystem::rename(temp, path);
#endif
        error.clear();
        return true;
    } catch (const std::exception &ex) {
        error = ex.what();
        std::error_code ignored;
        std::filesystem::remove(temp, ignored);
        return false;
    }
}
} // namespace elite_pen
