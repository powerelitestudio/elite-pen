#include "elite_pen/core.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

using namespace elite_pen;

namespace {

using Clock = std::chrono::steady_clock;

#ifdef NDEBUG
constexpr double kBudgetScale = 1.0;
#else
// Debug deliberately disables the optimizer; keep it useful for catching
// algorithmic explosions without pretending it represents shipping latency.
constexpr double kBudgetScale = 3.0;
#endif

double milliseconds(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

Drawable make_stroke(int stroke_index) {
    Drawable result;
    result.kind = Tool::Pen;
    result.color = kBlue;
    result.width = 7.0F;
    result.points.reserve(128);
    const float base_y = static_cast<float>(stroke_index % 1000);
    for (int index = 0; index < 128; ++index) {
        const float x = static_cast<float>(index) * 3.0F;
        result.points.push_back({x, base_y + std::sin(x * 0.05F) * 12.0F});
    }
    return result;
}

bool within(std::string_view name, double value, double limit) {
    std::cout << name << '=' << value << "ms (limit " << limit << "ms)\n";
    if (value <= limit) return true;
    std::cerr << "PERF FAIL: " << name << " exceeded its compatibility budget\n";
    return false;
}

}  // namespace

int main() {
    bool passed = true;
    Document document(256);

    auto start = Clock::now();
    for (int index = 0; index < 5000; ++index) document.add(make_stroke(index));
    passed &= within("add_5000_vector_strokes", milliseconds(start),
                     250.0 * kBudgetScale);

    start = Clock::now();
    for (int index = 0; index < 250; ++index) {
        document.erase_at({5000.0F + static_cast<float>(index), 5000.0F}, 2.0F);
    }
    passed &= within("250_eraser_misses_over_5000_objects", milliseconds(start),
                     400.0 * kBudgetScale);

    start = Clock::now();
    for (int index = 0; index < 10000; ++index) {
        if (!document.undo() || !document.redo()) {
            std::cerr << "PERF FAIL: latest-object undo/redo lost history\n";
            passed = false;
            break;
        }
    }
    passed &= within("10000_latest_object_undo_redo_cycles", milliseconds(start),
                     200.0 * kBudgetScale);

    start = Clock::now();
    document.clear();
    document.undo();
    passed &= within("clear_and_restore_5000_objects", milliseconds(start),
                     100.0 * kBudgetScale);

    std::vector<PointF> dense_path;
    dense_path.reserve(100000);
    for (int index = 0; index < 100000; ++index) {
        const float x = static_cast<float>(index) * 0.2F;
        dense_path.push_back({x, std::sin(x * 0.015F) * 80.0F});
    }
    start = Clock::now();
    const auto simplified = simplify_path(dense_path, 0.8F);
    passed &= within("simplify_100000_samples", milliseconds(start),
                     500.0 * kBudgetScale);
    if (simplified.size() >= dense_path.size() / 10) {
        std::cerr << "PERF FAIL: simplification retained too many samples\n";
        passed = false;
    }
    std::cout << "simplified_samples=" << simplified.size() << '\n';
    std::cout << "stored_objects=" << document.items().size() << '\n';
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
