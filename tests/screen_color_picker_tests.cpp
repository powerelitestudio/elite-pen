#include "screen_color_picker.hpp"
#include "color_dialog.hpp"
#include <array>
#include <iostream>
#include <thread>
#include <vector>
#include <limits>

using namespace elite_pen;
using namespace elite_pen::win;
namespace {
int checks{}, failures{};
void check(bool ok, const char* description) {
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << description << '\n'; }
}
HWND own_window(const wchar_t* type) {
    for (int n = 0; n < 150; ++n) {
        struct Search { const wchar_t* type; HWND found{}; } search{type};
        EnumWindows([](HWND w, LPARAM p) -> BOOL {
            auto& s = *reinterpret_cast<Search*>(p);
            DWORD pid{}; GetWindowThreadProcessId(w, &pid);
            wchar_t name[100]{}; GetClassNameW(w, name, 100);
            if (pid == GetCurrentProcessId() && std::wstring_view(name) == s.type && IsWindowVisible(w)) {
                s.found = w; return FALSE;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&search));
        if (search.found) { Sleep(80); return search.found; }
        Sleep(30);
    }
    return nullptr;
}
void choose(HWND window, POINT p) {
    if (!window) return;
    ScreenToClient(window, &p);
    PostMessageW(window, WM_LBUTTONUP, 0, MAKELPARAM(p.x, p.y));
}
LRESULT CALLBACK host_proc(HWND window, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_PAINT) {
        PAINTSTRUCT paint{}; HDC dc = BeginPaint(window, &paint);
        RECT r{}; GetClientRect(window, &r);
        HBRUSH brush = CreateSolidBrush(RGB(17, 143, 219));
        FillRect(dc, &r, brush); DeleteObject(brush); EndPaint(window, &paint); return 0;
    }
    return DefWindowProcW(window, m, w, l);
}
std::wstring text(HWND w) { wchar_t value[256]{}; GetWindowTextW(w, value, 256); return value; }
void desktop_tests() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW cls{}; cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpfnWndProc = host_proc; cls.lpszClassName = L"ElitePen.PickerTestHost"; RegisterClassW(&cls);
    HWND host = CreateWindowExW(WS_EX_TOPMOST, cls.lpszClassName, L"Elite Pen picker QA",
        WS_POPUP | WS_VISIBLE, 40, 40, 300, 300, nullptr, nullptr, cls.hInstance, nullptr);
    check(host != nullptr, "Desktop pattern host opens");
    if (!host) return;
    UpdateWindow(host);
    std::vector<RECT> monitors;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR, HDC, LPRECT r, LPARAM p) -> BOOL {
        reinterpret_cast<std::vector<RECT>*>(p)->push_back(*r); return TRUE;
    }, reinterpret_cast<LPARAM>(&monitors));
    std::wstring error;
    for (const auto r : monitors) {
        const POINT sample{r.left + 60, r.top + 60};
        SetWindowPos(host, HWND_TOPMOST, r.left + 40, r.top + 40, 300, 300, SWP_SHOWWINDOW);
        UpdateWindow(host); Sleep(120);
        std::thread input([sample] { choose(own_window(L"ElitePen.ScreenColorPicker"), sample); });
        const auto result = pick_screen_color(host, error); input.join();
        check(error.empty() && result == Color{17, 143, 219, 255}, "Physical desktop pixel matches on each connected monitor");
    }
    for (UINT cancel : {UINT(WM_KEYDOWN), UINT(WM_RBUTTONUP)}) {
        std::thread input([cancel] {
            if (auto w = own_window(L"ElitePen.ScreenColorPicker")) PostMessageW(w, cancel, VK_ESCAPE, 0);
        });
        const auto result = pick_screen_color(host, error); input.join();
        check(!result && error.empty(), "Escape and right click cancel without selecting");
    }
    // Repeat full modal creation/destruction, including GDI ownership, after warm-up.
    const DWORD resources = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int n = 0; n < 3; ++n) {
        std::thread input([] { if (auto w = own_window(L"ElitePen.ScreenColorPicker")) PostMessageW(w, WM_CLOSE, 0, 0); });
        pick_screen_color(host, error); input.join();
    }
    check(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) <= resources, "Repeated picker use releases GDI resources");
    // Exercise the real editor: sample -> synchronized fields -> saved favorite.
    const RECT r = monitors.front();
    SetWindowPos(host, HWND_TOPMOST, r.left + 40, r.top + 40, 300, 300, SWP_SHOWWINDOW); UpdateWindow(host);
    std::vector<Color> favorites; int saves{}; bool fields{}, reopened{};
    std::thread editor([&] {
        auto dialog = own_window(L"ElitePen.CustomColors");
        if (!dialog) return;
        PostMessageW(dialog, WM_COMMAND, MAKEWPARAM(107, BN_CLICKED), 0);
        choose(own_window(L"ElitePen.ScreenColorPicker"), {r.left + 60, r.top + 60});
        dialog = own_window(L"ElitePen.CustomColors");
        if (!dialog) return;
        reopened = IsWindowEnabled(GetDlgItem(dialog, IDOK)) != FALSE;
        fields = text(GetDlgItem(dialog, 101)) == L"#118FDB" && text(GetDlgItem(dialog, 102)) == L"17" &&
                 text(GetDlgItem(dialog, 103)) == L"143" && text(GetDlgItem(dialog, 104)) == L"219";
        SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(105, BN_CLICKED), 0);
        PostMessageW(dialog, WM_COMMAND, IDOK, 0);
    });
    const auto result = custom_color_dialog(host, kRed, favorites, [&](const auto&) { ++saves; return true; }); editor.join();
    check(reopened && fields, "Eyedropper restores editor and synchronizes HEX/RGB");
    check(result == Color{17, 143, 219, 255} && saves == 1 && favorites.size() == 1 && favorites[0] == *result,
          "Sample is saved only through Guardar color and accepted through Usar color");
    bool unchanged{};
    std::thread cancel_editor([&] {
        auto dialog = own_window(L"ElitePen.CustomColors");
        if (!dialog) return;
        SendMessageW(GetDlgItem(dialog, 101), WM_SETTEXT, 0, reinterpret_cast<LPARAM>(L"#12"));
        PostMessageW(dialog, WM_COMMAND, MAKEWPARAM(107, BN_CLICKED), 0);
        if (auto w = own_window(L"ElitePen.ScreenColorPicker")) PostMessageW(w, WM_KEYDOWN, VK_ESCAPE, 0);
        dialog = own_window(L"ElitePen.CustomColors");
        if (!dialog) return;
        unchanged = text(GetDlgItem(dialog, 101)) == L"#12" && !IsWindowEnabled(GetDlgItem(dialog, IDOK));
        PostMessageW(dialog, WM_COMMAND, IDCANCEL, 0);
    });
    const auto canceled = custom_color_dialog(host, kRed, favorites, [&](const auto&) { ++saves; return true; }); cancel_editor.join();
    check(!canceled && unchanged && saves == 1, "Cancelling preserves invalid text, validation and saved palette");
    DestroyWindow(host);
}
}
int main(int argc, char** argv) {
    const std::array<std::uint32_t, 4> pixels{0x00118FDB, 0xFFABCDEF, 0x00FF0000, 0x0000FF00};
    check(desktop_pixel(pixels, 2, 2, {-1920, -1080}, {-1920, -1080}) == Color{17, 143, 219, 255}, "Negative desktop origin and BGRA ordering");
    check(desktop_pixel(pixels, 2, 2, {-1920, -1080}, {-1919, -1080}) == Color{171, 205, 239, 255}, "Ignore unspecified desktop alpha");
    check(desktop_pixel(pixels, 2, 2, {0, 0}, {0, 1}) == Color{255, 0, 0, 255}, "Top-down row mapping");
    check(!desktop_pixel(pixels, 2, 2, {0, 0}, {-1, 0}), "Reject left of desktop");
    check(!desktop_pixel(pixels, 2, 2, {0, 0}, {2, 1}), "Reject right boundary");
    check(!desktop_pixel(pixels, 2, 2, {0, 0}, {1, 2}), "Reject bottom boundary");
    check(!desktop_pixel(pixels, -1, 2, {0, 0}, {0, 0}), "Reject invalid geometry");
    check(!desktop_pixel({}, 2, 2, {0, 0}, {1, 1}), "Reject missing buffer");
    check(!desktop_pixel(pixels, INT_MAX, INT_MAX, {LONG_MIN, LONG_MIN}, {0, 0}), "Large coordinates do not wrap into buffer");
    if (argc > 1 && std::string_view(argv[1]) == "--desktop") desktop_tests();
    std::cout << checks << " picker checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
