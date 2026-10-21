#include "elite_pen/board.hpp"
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <thread>
#include <windows.h>

using namespace elite_pen;
namespace {
int failures{}, checks{};
void check(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
bool close_enough(float a, float b) { return std::abs(a - b) < .005F; }
BoardObject stroke(float y = 0) {
    BoardObject o;
    o.ink.points = {{-50, y}, {0, y + 20}, {80, y}};
    o.ink.width = 12;
    o.pressures = {.05F, .5F, 1};
    return o;
}
BoardFile file(const BoardDocument &document) {
    BoardFile f;
    f.objects = document.objects();
    f.camera = {{-200, -100}, 2};
    f.dark = true;
    f.grid = true;
    f.panel_scale = 300;
    f.panel_visible = false;
    f.pen_width = 17;
    f.pen_color = kRed;
    f.source_path = "prueba-\xc3\xb1.epboard";
    return f;
}
void model() {
    BoardDocument d;
    check(d.add(stroke()), "add pressure stroke");
    const auto id = d.objects().front()->id;
    check(d.hit({0, 20}, 2)->id == id, "hit stroke");
    check(!d.hit({300, 300}, 2), "empty hit");
    check(d.in_area({-60, -10, 100, 30}) == BoardIds{id}, "area selection");
    const auto before = d.objects().front();
    const auto bounds = before->bounds();
    RectF moved{bounds.left + 400, bounds.top - 200, bounds.right + 400, bounds.bottom - 200};
    check(d.transform(BoardIds{id}, bounds, moved), "move selection");
    check(close_enough(d.objects().front()->ink.points[0].x, 350), "move world coordinates");
    check(before->ink.points[0].x == -50, "immutable history snapshot");
    check(d.undo() && d.objects().front() == before, "undo exact object");
    check(d.redo(), "redo move");
    check(d.lock(BoardIds{id}, true), "lock object");
    check(!d.remove(BoardIds{id}) && !d.erase({350, -200}, 10), "locked ink protected");
    check(d.in_area({-10000, -10000, 10000, 10000}).empty(), "locked excluded from selection");
    check(d.lock(BoardIds{id}, false), "unlock");
    check(d.duplicate(BoardIds{id}) && d.objects().size() == 2, "duplicate");
    check(d.objects().back()->id != id, "unique duplicated id");
    check(d.reorder(BoardIds{id}, true) && d.objects().back()->id == id, "bring front");
    d.begin_compound();
    check(d.remove(BoardIds{id}), "compound erase first");
    check(d.remove(d.all_ids()), "compound erase rest");
    d.end_compound();
    check(d.objects().empty() && d.undo() && d.objects().size() == 2,
          "one undo restores whole eraser gesture");
    check(d.clear() && d.undo() && d.objects().size() == 2, "undo clear");
    auto bad = stroke();
    bad.pressures.pop_back();
    check(!d.add(bad), "reject mismatched pressure");
    bad = stroke();
    bad.ink.points[0].x = std::numeric_limits<float>::infinity();
    check(!d.add(bad), "reject infinite geometry");
    check(board_pressure_width(.05F) < board_pressure_width(.5F) &&
              board_pressure_width(.5F) < board_pressure_width(1),
          "pressure widens stroke progressively");
    BoardCamera c{{-123, 456}, 1.25F};
    const PointF point{220, 130};
    const auto fixed = c.to_world(point);
    c.zoom_at(point, 2);
    check(close_enough(c.to_world(point).x, fixed.x) && close_enough(c.to_world(point).y, fixed.y),
          "zoom fixes cursor world point");
    c.pan({100, -50});
    check(close_enough(c.to_view(fixed).x, point.x + 100) && close_enough(c.to_view(fixed).y, point.y - 50),
          "pan uses screen delta");
    c.zoom_at(point, 1e8F);
    check(c.zoom == 32, "zoom upper bound");
    c.zoom_at(point, 1e-12F);
    check(c.zoom == .03125F, "zoom lower bound");
    c.frame({-100, -50, 100, 50}, {1000, 800});
    check(c.to_view({0, 0}).x == 500, "frame centers content");
}
void serialization() {
    BoardDocument d;
    d.add(stroke());
    auto text = stroke();
    text.ink.kind = Tool::Text;
    text.pressures.clear();
    text.ink.text = L"Explicación ñ 漢字 \U0001F680";
    d.add(text);
    auto image = std::make_shared<BoardImage>();
    image->width = 1;
    image->height = 1;
    image->png = {137, 80, 78, 71, 13, 10, 26, 10};
    BoardObject i;
    i.image = image;
    i.image_bounds = {100, 100, 200, 200};
    d.add(i);
    d.add(i);
    const auto source = file(d);
    const auto bytes = encode_board(source);
    BoardFile result;
    std::string error;
    check(decode_board(bytes, result, error), "round trip decodes");
    check(result.objects.size() == 4 && result.dark && result.grid && result.pressure,
          "round trip settings and count");
    check(result.panel_scale == 300 && !result.panel_visible && result.pen_width == 17 &&
              result.pen_color == kRed,
          "toolbar scale, visibility and pen settings survive restart");
    check(result.camera.origin.x == -200 && result.camera.zoom == 2 &&
              result.source_path == source.source_path,
          "round trip camera and UTF8 source path");
    check(result.objects[0]->pressures == source.objects[0]->pressures, "all pressure samples survive");
    check(result.objects[1]->ink.text == text.ink.text, "unicode text survives");
    check(result.objects[2]->image == result.objects[3]->image, "shared image asset stored once");
    check(encode_board(result) == bytes, "deterministic encoding");
    const auto retained = result.objects.front();
    auto corrupt = bytes;
    corrupt.back() ^= 0x40;
    check(!decode_board(corrupt, result, error) && result.objects.front() == retained,
          "checksum rejection leaves destination intact");
    for (std::size_t n = 0; n < bytes.size(); n += 7)
        check(!decode_board(std::span(bytes).first(n), result, error), "reject truncated files");
    corrupt = bytes;
    corrupt[8] = 99;
    check(!decode_board(corrupt, result, error), "reject future version");
    corrupt = bytes;
    corrupt.push_back(0);
    check(!decode_board(corrupt, result, error), "reject trailing bytes");
    std::mt19937 random(816);
    for (int n = 0; n < 300; ++n) {
        corrupt = bytes;
        const auto at = static_cast<std::size_t>(random()) % corrupt.size();
        corrupt[at] ^= static_cast<std::uint8_t>((random() % 255) + 1);
        check(!decode_board(corrupt, result, error), "mutated archive rejected");
    }
}
void styled_text() {
    BoardDocument d;
    check(d.add(stroke()) && d.add(stroke(100)), "recognition source strokes");
    const auto original = d.objects();
    BoardObject text;
    text.ink.kind = Tool::Text;
    text.ink.points = {{-50, 0}};
    text.ink.text = L"Hola, español";
    text.font = L"Palitoon";
    text.font_size = 28;
    text.text_bounds = {-50, 0, 190, 36};
    check(d.replace(d.all_ids(), text) && d.objects().size() == 1, "conversion is one atomic replacement");
    check(d.hit({180, 20})->id == d.objects()[0]->id && !d.hit({200, 20}), "text hit uses measured bounds");
    check(d.undo() && d.objects() == original, "undo restores exact strokes and pressure");
    check(d.redo() && d.objects()[0]->ink.text == text.ink.text, "redo restores text");
    const auto id = d.objects()[0]->id;
    const auto revision = d.revision();
    check(!d.replace(BoardIds{id, 999999}, text) && d.revision() == revision,
          "stale recognition cannot partially remove content");
    check(!d.replace(BoardIds{id, id}, text), "duplicate replacement targets rejected");
    d.lock(BoardIds{id}, true);
    check(!d.replace(BoardIds{id}, text), "locked text is protected");
    d.lock(BoardIds{id}, false);
    auto transformed = transform_board_object(*d.objects()[0], text.text_bounds, {-100, 100, 380, 172});
    check(close_enough(transformed.font_size, 56) && close_enough(transformed.bounds().left, -100),
          "text scales and moves with selection");
    auto f = file(d);
    f.background = Color{20, 60, 140, 255};
    f.auto_text = true;
    f.text_font = L"Palitoon";
    f.text_size = 48;
    BoardFile recovered;
    std::string error;
    check(decode_board(encode_board(f), recovered, error) && recovered.background == f.background &&
              recovered.auto_text && recovered.text_font == L"Palitoon" && recovered.text_size == 48 &&
              recovered.objects[0]->font == L"Palitoon" && recovered.objects[0]->ink.text == text.ink.text &&
              recovered.objects[0]->text_bounds.right == 190,
          "font, converted text and arbitrary background survive persistence");
    auto invalid = text;
    invalid.font_size = std::numeric_limits<float>::quiet_NaN();
    check(!d.replace(BoardIds{id}, invalid), "invalid text style rejected");
    const auto fixture = std::filesystem::path(__FILE__).parent_path() / "fixtures" / "board-v1.epboard";
    check(read_board(fixture, recovered, error, false) && recovered.objects.size() == 1 && recovered.dark &&
              recovered.panel_scale == 200 && !recovered.background && !recovered.auto_text &&
              recovered.text_font == L"Palitoon" && recovered.objects[0]->pressures.size() == 2,
          "v1 fixture remains readable with safe defaults");
}
void persistence() {
    const auto folder = std::filesystem::temp_directory_path() /
                        (L"elite-pen-board-model-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(folder);
    const auto path = folder / L"prueba ñ.epboard";
    BoardDocument d;
    d.add(stroke());
    std::string error;
    BoardFile recovered;
    check(write_board(path, file(d), error), "atomic initial save");
    d.add(stroke(100));
    check(write_board(path, file(d), error), "atomic second save");
    check(read_board(path, recovered, error) && recovered.objects.size() == 2, "load latest");
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << "interrupted";
    }
    check(read_board(path, recovered, error) && recovered.objects.size() == 1 && !error.empty(),
          "corrupt current falls back to previous valid save");
    check(!read_board(path, recovered, error, false), "explicit invalid file rejected without fallback");
    const auto invalid = folder / L"missing" / L"blocked.epboard";
    {
        std::ofstream out(folder / L"missing");
        out << "file";
    }
    check(!write_board(invalid, file(d), error), "failed write reports error");
    check(write_board(path, file(d), error) && read_board(path, recovered, error) &&
              recovered.objects.size() == 2,
          "repair current without damaging valid backup");
    // Only this test's uniquely named directory is removed.
    std::filesystem::remove_all(folder);
}
} // namespace
int main() {
    model();
    serialization();
    styled_text();
    persistence();
    std::cout << checks << " board checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
