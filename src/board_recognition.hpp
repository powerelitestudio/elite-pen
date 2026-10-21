#pragma once
#include "elite_pen/core.hpp"
#include <string>
#include <vector>

namespace elite_pen::win {
struct BoardRecognition {
    std::wstring text, recognizer, error;
};
// Owns its COM apartment. Call on a worker, never on the input/render thread.
[[nodiscard]] BoardRecognition recognize_board_ink(const std::vector<std::vector<PointF>> &strokes);
} // namespace elite_pen::win
