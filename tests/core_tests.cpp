#include "elite_pen/core.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

using namespace elite_pen;

namespace {

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

Drawable line(PointF a, PointF b) {
    Drawable result;
    result.kind = Tool::Line;
    result.points = {a, b};
    return result;
}

void test_document_history() {
    Document document(3);
    check(document.empty(), "new document is empty");
    document.add(line({0, 0}, {100, 0}));
    document.add(line({0, 20}, {100, 20}));
    check(document.items().size() == 2, "add stores drawables");
    check(document.undo(), "undo succeeds");
    check(document.items().size() == 1, "undo removes latest add");
    check(document.redo(), "redo succeeds");
    check(document.items().size() == 2, "redo restores latest add");
    check(document.clear(), "clear non-empty succeeds");
    check(document.empty(), "clear removes all");
    check(document.undo(), "clear is undoable");
    check(document.items().size() == 2, "undo clear restores exact content");
    check(document.redo(), "clear is redoable");
    check(document.empty(), "redo clear removes content");
    check(!document.clear(), "clear empty is a no-op");
}

void test_erase_and_redo_invalidation() {
    Document document;
    document.add(line({0, 0}, {100, 0}));
    document.add(line({0, 20}, {100, 20}));
    check(document.erase_at({50, 20}, 2), "erase hits topmost line");
    check(document.items().size() == 1, "erase removes one object");
    check(document.undo(), "erase undo succeeds");
    check(document.items().size() == 2, "erase undo restores position");
    document.add(line({0, 40}, {100, 40}));
    check(!document.can_redo(), "new operation invalidates redo branch");
    check(!document.erase_at({500, 500}), "erase miss is no-op");
}

void test_compound_erase() {
    Document document;
    document.add(line({0, 0}, {100, 0}));
    document.add(line({0, 20}, {100, 20}));
    document.add(line({0, 40}, {100, 40}));
    document.begin_compound();
    check(document.erase_at({50, 0}), "compound erase removes first hit");
    check(document.erase_at({50, 20}), "compound erase removes second hit");
    document.end_compound();
    check(document.items().size() == 1, "compound erase applies all removals");
    check(document.undo(), "compound erase uses one undo operation");
    check(document.items().size() == 3, "one undo restores entire eraser gesture");
}

void test_hit_testing() {
    Drawable rectangle;
    rectangle.kind = Tool::Rectangle;
    rectangle.width = 4;
    rectangle.points = {{10, 10}, {110, 60}};
    check(hit_test(rectangle, {10, 30}), "rectangle edge hits");
    check(!hit_test(rectangle, {50, 30}), "rectangle interior does not hit outline");

    Drawable ellipse = rectangle;
    ellipse.kind = Tool::Ellipse;
    check(hit_test(ellipse, {110, 35}), "ellipse edge hits");
    check(!hit_test(ellipse, {60, 35}), "ellipse center does not hit outline");

    const RectF original_bounds = rectangle.bounds();
    rectangle.points.back() = {210, 160};
    rectangle.invalidate_bounds_cache();
    const RectF updated_bounds = rectangle.bounds();
    check(updated_bounds.right > original_bounds.right &&
          updated_bounds.bottom > original_bounds.bottom,
          "bounds cache invalidates after editable geometry changes");

    Drawable curved;
    curved.kind = Tool::CurvedArrow;
    curved.width = 4;
    curved.points = {{0, 0}, {100, 0}};
    const auto bezier = curved_arrow_bezier(curved.points.front(), curved.points.back());
    const PointF crest = cubic_bezier_point(bezier, 0.5F);
    check(crest.y > 15.0F, "curved arrow uses a visible cubic Bezier bend");
    check(curved.bounds().bottom > 15.0F, "curved arrow bounds include Bezier controls");
    check(hit_test(curved, crest, 2.0F), "curved arrow hit testing follows Bezier path");
    check(!hit_test(curved, {50, -30}, 2.0F), "curved arrow misses away from Bezier path");
}

void test_simplification() {
    std::vector<PointF> points;
    for (int x = 0; x <= 100; ++x) points.push_back({static_cast<float>(x), 0.01F * (x % 2)});
    const auto simplified = simplify_path(points, 0.1F);
    check(simplified.size() == 2, "nearly straight path is reduced to endpoints");
    check(simplified.front().x == 0 && simplified.back().x == 100,
          "simplification preserves endpoints");
}

void test_history_limit() {
    Document document(2);
    document.add(line({0, 0}, {1, 1}));
    document.add(line({0, 1}, {1, 2}));
    document.add(line({0, 2}, {1, 3}));
    check(document.history_size() == 2, "history respects configured limit");
    check(document.undo() && document.undo(), "retained history can be undone");
    check(!document.undo(), "discarded operation cannot be undone");
    check(document.items().size() == 1, "oldest content remains after limited undo");
}

}  // namespace

int main() {
    test_document_history();
    test_erase_and_redo_invalidation();
    test_compound_erase();
    test_hit_testing();
    test_simplification();
    test_history_limit();
    if (failures == 0) {
        std::cout << "Elite Pen core: all tests passed\n";
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " test(s) failed\n";
    return EXIT_FAILURE;
}
