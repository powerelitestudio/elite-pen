#pragma once

#include <windows.h>

#include <array>

namespace elite_pen::win {

enum class HistoryKeyResult { Pass, Consume, Undo, Redo };

// Preserve ownership for a whole key press: repeats must not undo multiple
// strokes, and key-up must not leak if the drawing mode changed in between.
class HistoryShortcutRouter {
public:
    HistoryKeyResult route(UINT key, bool down, UINT modifiers, bool annotating,
                           bool reserved) noexcept {
        if (key != 'Z' && key != 'Y') return HistoryKeyResult::Pass;
        auto& press = presses_[key == 'Z' ? 0U : 1U];
        if (!down) {
            const bool owned = press.owned;
            press = {};
            return owned ? HistoryKeyResult::Consume : HistoryKeyResult::Pass;
        }
        if (press.down)
            return press.owned ? HistoryKeyResult::Consume : HistoryKeyResult::Pass;
        press.down = true;
        press.owned = annotating && !reserved && modifiers == MOD_CONTROL;
        if (!press.owned) return HistoryKeyResult::Pass;
        return key == 'Z' ? HistoryKeyResult::Undo : HistoryKeyResult::Redo;
    }

private:
    struct Press { bool down{}; bool owned{}; };
    std::array<Press, 2> presses_{};
};

}  // namespace elite_pen::win
