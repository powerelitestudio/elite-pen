#include "elite_pen/core.hpp"

#include <array>
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
    check(document.redo(), "compound erase can be redone");
    check(document.items().size() == 1 && document.items().front().points.front().y == 40,
          "compound erase redo preserves surviving order");

    Document reverse;
    reverse.add(line({0, 0}, {100, 0}));
    reverse.add(line({0, 20}, {100, 20}));
    reverse.add(line({0, 40}, {100, 40}));
    reverse.begin_compound();
    check(reverse.erase_at({50, 40}), "reverse compound removes the last object first");
    check(reverse.erase_at({50, 0}), "reverse compound then removes the first object");
    reverse.end_compound();
    check(reverse.undo() && reverse.items().size() == 3,
          "reverse compound undo restores every original position");
    check(reverse.redo() && reverse.items().size() == 1 &&
          reverse.items().front().points.front().y == 20,
          "reverse compound redo removes the same objects deterministically");

    std::array<std::size_t, 5> order{0, 1, 2, 3, 4};
    do {
        Document permutation;
        for (std::size_t index = 0; index < 5; ++index) {
            permutation.add(line({0, static_cast<float>(index * 20)},
                                 {100, static_cast<float>(index * 20)}));
        }
        permutation.begin_compound();
        for (std::size_t index = 0; index < 3; ++index) {
            check(permutation.erase_at({50, static_cast<float>(order[index] * 20)}),
                  "compound erase accepts every removal order");
        }
        permutation.end_compound();
        std::array<bool, 5> removed{};
        for (std::size_t index = 0; index < 3; ++index) removed[order[index]] = true;
        check(permutation.undo() && permutation.items().size() == 5,
              "permuted compound undo restores all objects");
        check(permutation.redo() && permutation.items().size() == 2,
              "permuted compound redo restores the survivor count");
        std::size_t survivor = 0;
        for (std::size_t index = 0; index < 5; ++index) {
            if (!removed[index]) {
                check(permutation.items()[survivor].points.front().y ==
                          static_cast<float>(index * 20),
                      "permuted compound redo preserves survivor ordering");
                ++survivor;
            }
        }
    } while (std::next_permutation(order.begin(), order.end()));
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

    Drawable flat_ellipse;
    flat_ellipse.kind = Tool::Ellipse;
    flat_ellipse.width = 4;
    flat_ellipse.points = {{0, 0}, {200, 20}};
    check(hit_test(flat_ellipse, {100, 23}, 2),
          "flat ellipse keeps pixel-accurate tolerance on its short axis");
    check(!hit_test(flat_ellipse, {100, 30}, 2),
          "flat ellipse rejects points beyond pixel tolerance");

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

    Drawable arrow;
    arrow.kind = Tool::Arrow;
    arrow.width = 4;
    arrow.points = {{10, 100}, {110, 100}};
    const auto arrow_head = arrow_head_points(arrow.points.front(), arrow.points.back(),
                                              arrow.width);
    const PointF arrow_head_midpoint{
        (arrow.points.back().x + arrow_head.left.x) * 0.5F,
        (arrow.points.back().y + arrow_head.left.y) * 0.5F};
    check(arrow.bounds().contains(arrow_head.left),
          "arrow bounds include the rendered arrowhead");
    check(hit_test(arrow, arrow_head_midpoint, 1.0F),
          "arrowhead can be selected and erased");

    const auto curved_head = arrow_head_points(bezier.control2, bezier.end, curved.width);
    const PointF curved_head_midpoint{
        (bezier.end.x + curved_head.right.x) * 0.5F,
        (bezier.end.y + curved_head.right.y) * 0.5F};
    check(curved.bounds().contains(curved_head.right),
          "curved arrow bounds include the rendered arrowhead");
    check(hit_test(curved, curved_head_midpoint, 1.0F),
          "curved arrowhead can be selected and erased");
}

void test_simplification() {
    std::vector<PointF> points;
    for (int x = 0; x <= 100; ++x) points.push_back({static_cast<float>(x), 0.01F * (x % 2)});
    const auto simplified = simplify_path(points, 0.1F);
    check(simplified.size() == 2, "nearly straight path is reduced to endpoints");
    check(simplified.front().x == 0 && simplified.back().x == 100,
          "simplification preserves endpoints");

    std::vector<PointF> adversarial;
    adversarial.reserve(12000);
    for (int index = 0; index < 12000; ++index) {
        adversarial.push_back({static_cast<float>(index),
                               index % 2 == 0 ? 0.0F : 2.0F});
    }
    const auto stable = simplify_path(adversarial, 0.25F);
    check(stable.size() > 1000 && stable.front().x == 0.0F &&
          stable.back().x == 11999.0F,
          "iterative simplification handles deep adversarial paths without recursion");
}

void test_modifier_gestures() {
    check(gesture_tool(Tool::Pen, true, false, false, false) == Tool::Line,
          "Shift temporarily selects a straight line");
    check(gesture_tool(Tool::Pen, false, true, false, false) == Tool::Rectangle,
          "Control temporarily selects a rectangle");
    check(gesture_tool(Tool::Pen, false, false, false, true) == Tool::Ellipse,
          "Tab temporarily selects an ellipse");
    check(gesture_tool(Tool::Pen, true, true, false, false) == Tool::Arrow,
          "Control Shift temporarily selects an arrow");
    check(gesture_tool(Tool::Pen, true, false, false, true) == Tool::CurvedArrow,
          "Shift Tab temporarily selects a curved arrow");
    check(gesture_tool(Tool::Pen, false, true, true, false) == Tool::Rectangle,
          "Alt does not change the Control rectangle gesture");
    check(gesture_tool(Tool::Pen, true, true, true, true) == Tool::Arrow,
          "arrow chord has deterministic priority over other modifiers");
    check(gesture_tool(Tool::Rectangle, true, false, false, false) == Tool::Rectangle,
          "explicit rectangle keeps Shift square constraint");
    check(gesture_tool(Tool::Eraser, true, true, true, true) == Tool::Eraser,
          "modifier gestures never override the eraser");
    check(gesture_tool(Tool::Text, true, true, true, true) == Tool::Text,
          "modifier gestures never override text insertion");
}

void test_document_revision() {
    Document document;
    const auto initial = document.revision();
    document.add(line({0, 0}, {10, 10}));
    const auto added = document.revision();
    check(added > initial, "document revision advances on add");
    check(document.undo() && document.revision() > added,
          "document revision advances on undo");
    const auto undone = document.revision();
    check(document.redo() && document.revision() > undone,
          "document revision advances on redo");
    const auto redone = document.revision();
    check(!document.erase_at({500, 500}) && document.revision() == redone,
          "document revision stays stable for no-op eraser misses");
}

void test_zoom_viewport_transform() {
    const ZoomViewportTransform transform{{-1920.0F, 120.0F, -960.0F, 660.0F}, 2.5F};
    const PointF source{-1712.5F, 307.5F};
    const PointF view = transform.source_to_view(source);
    check(std::abs(view.x - 518.75F) < 0.001F &&
          std::abs(view.y - 468.75F) < 0.001F,
          "zoom transform supports negative desktop coordinates");
    const PointF round_trip = transform.view_to_source(view);
    check(distance(round_trip, source) < 0.001F,
          "zoom source and viewport transforms round trip");
    check(std::abs(transform.view_to_source_length(10.0F) - 4.0F) < 0.001F &&
          std::abs(transform.source_to_view_length(4.0F) - 10.0F) < 0.001F,
          "zoom transform scales annotation widths consistently");

    ZoomViewportTransform moved = transform;
    moved.source.left += 80.0F;
    moved.source.top -= 40.0F;
    const PointF moved_view = moved.source_to_view(source);
    check(std::abs(moved_view.x - (view.x - 200.0F)) < 0.001F &&
          std::abs(moved_view.y - (view.y + 100.0F)) < 0.001F,
          "anchored points move with the magnified source");

    const ZoomViewportTransform invalid{{10.0F, 20.0F, 30.0F, 40.0F}, 0.0F};
    const PointF safe = invalid.view_to_source({5.0F, 7.0F});
    check(safe.x == 15.0F && safe.y == 27.0F,
          "invalid zoom factors fail safely at one-to-one scale");

    const auto full_curve = curved_arrow_bezier({0, 0}, {400, 0});
    const auto source_curve = curved_arrow_bezier({0, 0}, {200, 0}, 2.0F);
    check(distance(full_curve.control1,
                   {source_curve.control1.x * 2.0F,
                    source_curve.control1.y * 2.0F}) < 0.001F &&
          distance(full_curve.control2,
                   {source_curve.control2.x * 2.0F,
                    source_curve.control2.y * 2.0F}) < 0.001F,
          "curved arrow controls preserve their creation geometry in source space");
    const auto full_head = arrow_head_points({0, 0}, {400, 0}, 4.0F);
    const auto source_head = arrow_head_points({0, 0}, {200, 0}, 2.0F, 2.0F);
    check(distance(full_head.left,
                   {source_head.left.x * 2.0F,
                    source_head.left.y * 2.0F}) < 0.001F,
          "arrow head limits preserve their creation geometry in source space");
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
    test_modifier_gestures();
    test_history_limit();
    test_document_revision();
    test_zoom_viewport_transform();
    if (failures == 0) {
        std::cout << "Elite Pen core: all tests passed\n";
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " test(s) failed\n";
    return EXIT_FAILURE;
}
