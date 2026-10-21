#include "board_recognition.hpp"
#include <iostream>
#include <limits>
using namespace elite_pen;
int main() {
    std::vector<std::vector<PointF>> s{{{10, 10}, {10, 60}},
                                       {{40, 10}, {40, 60}},
                                       {{10, 35}, {40, 35}},
                                       {{60, 10}, {60, 60}},
                                       {{90, 10}, {90, 60}, {120, 60}}};
    auto r = win::recognize_board_ink(s);
    std::wcout << L"Recognized: " << r.text << L"; recognizer: " << r.recognizer << L"; error: " << r.error
               << L"\n";
    if (r.text.empty() || r.recognizer.empty())
        return 1;
    auto invalid = win::recognize_board_ink({{{std::numeric_limits<float>::quiet_NaN(), 0}}});
    if (!invalid.text.empty())
        return 3;
    auto empty = win::recognize_board_ink({});
    if (!empty.text.empty())
        return 2;
    return 0;
}
