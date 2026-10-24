#include "screen_color_picker.hpp"
#include <algorithm>
#include <dwmapi.h>
#include <windowsx.h>

namespace elite_pen::win {
namespace {
struct Picker {
    HWND window{};
    HDC snapshot{};
    HBITMAP bitmap{};
    HGDIOBJ old_bitmap{};
    HFONT font{};
    std::uint32_t* pixels{};
    POINT origin{}, point{};
    int width{}, height{};
    bool done{}, active_seen{};
    std::optional<Color> result;
    ~Picker() {
        if (window) DestroyWindow(window);
        if (snapshot && old_bitmap) SelectObject(snapshot, old_bitmap);
        if (bitmap) DeleteObject(bitmap);
        if (snapshot) DeleteDC(snapshot);
        if (font) DeleteObject(font);
    }
    std::optional<Color> color(POINT p) const {
        return desktop_pixel({pixels, static_cast<std::size_t>(width) * static_cast<std::size_t>(height)},
                             width, height, origin, p);
    }
    RECT hud_bounds(POINT p) const {
        MONITORINFO monitor{}; monitor.cbSize = sizeof(monitor);
        GetMonitorInfoW(MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST), &monitor);
        constexpr int w = 246, h = 176;
        int x = p.x + 24, y = p.y + 24;
        if (x + w > monitor.rcMonitor.right) x = p.x - w - 24;
        if (y + h > monitor.rcMonitor.bottom) y = p.y - h - 24;
        x = std::clamp(x, static_cast<int>(monitor.rcMonitor.left), std::max(static_cast<int>(monitor.rcMonitor.left), static_cast<int>(monitor.rcMonitor.right) - w)) - origin.x;
        y = std::clamp(y, static_cast<int>(monitor.rcMonitor.top), std::max(static_cast<int>(monitor.rcMonitor.top), static_cast<int>(monitor.rcMonitor.bottom) - h)) - origin.y;
        return {x, y, x + w, y + h};
    }
    void move(POINT p) {
        if (p.x == point.x && p.y == point.y) return;
        const RECT previous = hud_bounds(point), next = hud_bounds(p);
        point = p;
        InvalidateRect(window, &previous, FALSE); InvalidateRect(window, &next, FALSE);
    }
    void choose(POINT p) {
        if (!MonitorFromPoint(p, MONITOR_DEFAULTTONULL)) return;
        if (auto sample = color(p)) { result = sample; done = true; }
    }
    void paint() {
        PAINTSTRUCT paint{};
        HDC screen = BeginPaint(window, &paint);
        // One small back buffer for the HUD; never recapture our own magnifier.
        BitBlt(screen, paint.rcPaint.left, paint.rcPaint.top,
               paint.rcPaint.right - paint.rcPaint.left, paint.rcPaint.bottom - paint.rcPaint.top,
               snapshot, paint.rcPaint.left, paint.rcPaint.top, SRCCOPY);
        constexpr int hud_width = 246, hud_height = 176;
        const auto bounds = hud_bounds(point);
        HDC hud = CreateCompatibleDC(screen);
        HBITMAP hud_bitmap = CreateCompatibleBitmap(screen, hud_width, hud_height);
        if (!hud || !hud_bitmap) {
            if (hud_bitmap) DeleteObject(hud_bitmap);
            if (hud) DeleteDC(hud);
            EndPaint(window, &paint); return;
        }
        HGDIOBJ previous_bitmap = SelectObject(hud, hud_bitmap);
        HBRUSH background = CreateSolidBrush(RGB(24, 24, 31));
        RECT panel{0, 0, hud_width, hud_height}; FillRect(hud, &panel, background); DeleteObject(background);
        // 11x11 nearest-neighbor pixels; sample coordinates always refer to the snapshot.
        constexpr int pixel_size = 8, side = 11;
        for (int row = 0; row < side; ++row) for (int col = 0; col < side; ++col) {
            const auto c = color({point.x + col - 5, point.y + row - 5}).value_or(Color{48, 48, 56, 255});
            SetDCBrushColor(hud, RGB(c.r, c.g, c.b));
            RECT cell{12 + col * pixel_size, 12 + row * pixel_size, 12 + (col + 1) * pixel_size, 12 + (row + 1) * pixel_size};
            FillRect(hud, &cell, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        }
        RECT center{51, 51, 61, 61}; FrameRect(hud, &center, GetSysColorBrush(COLOR_WINDOWTEXT));
        InflateRect(&center, 1, 1); FrameRect(hud, &center, GetSysColorBrush(COLOR_WINDOW));
        const auto sample = color(point).value_or(kBlack);
        HBRUSH swatch = CreateSolidBrush(RGB(sample.r, sample.g, sample.b));
        RECT chip{114, 14, 232, 48}; FillRect(hud, &chip, swatch); DeleteObject(swatch);
        HGDIOBJ previous_font = SelectObject(hud, font);
        SetBkMode(hud, TRANSPARENT); SetTextColor(hud, RGB(247, 247, 250));
        RECT hex{114, 56, 236, 82}; const auto value = color_hex(sample);
        DrawTextW(hud, value.c_str(), -1, &hex, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        RECT instruction{12, 110, 236, 138};
        DrawTextW(hud, L"Clic o Enter: elegir · Esc: cancelar", -1, &instruction, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        RECT help{12, 140, 236, 166}; SetTextColor(hud, RGB(183, 183, 197));
        DrawTextW(hud, L"Flechas: mover píxel a píxel", -1, &help, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        SelectObject(hud, previous_font);
        BitBlt(screen, bounds.left, bounds.top, hud_width, hud_height, hud, 0, 0, SRCCOPY);
        SelectObject(hud, previous_bitmap); DeleteObject(hud_bitmap); DeleteDC(hud);
        EndPaint(window, &paint);
    }
    static LRESULT CALLBACK proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
        auto* self = reinterpret_cast<Picker*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<Picker*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
            self->window = hwnd; SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self) return DefWindowProcW(hwnd, message, wp, lp);
        switch (message) {
        case WM_PAINT: self->paint(); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_SETCURSOR: SetCursor(LoadCursorW(nullptr, IDC_CROSS)); return TRUE;
        case WM_MOUSEMOVE: self->move({self->origin.x + GET_X_LPARAM(lp), self->origin.y + GET_Y_LPARAM(lp)}); return 0;
        case WM_LBUTTONUP: self->choose({self->origin.x + GET_X_LPARAM(lp), self->origin.y + GET_Y_LPARAM(lp)}); return 0;
        case WM_RBUTTONUP: case WM_CLOSE: case WM_DISPLAYCHANGE: case WM_DPICHANGED:
            self->done = true; return 0;
        case WM_ACTIVATE:
            if (LOWORD(wp) != WA_INACTIVE) self->active_seen = true;
            else if (self->active_seen) self->done = true;
            return 0;
        case WM_CAPTURECHANGED: if (self->active_seen) self->done = true; return 0;
        case WM_POINTERUPDATE: case WM_POINTERUP: {
            POINTER_INFO info{};
            if (GetPointerInfo(GET_POINTERID_WPARAM(wp), &info)) {
                self->move(info.ptPixelLocation);
                if (message == WM_POINTERUP) self->choose(info.ptPixelLocation);
                return 0;
            }
            break;
        }
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) { self->done = true; return 0; }
            if (wp == VK_RETURN) { self->choose(self->point); return 0; }
            if (wp == VK_LEFT || wp == VK_RIGHT || wp == VK_UP || wp == VK_DOWN) {
                POINT p = self->point;
                p.x += wp == VK_LEFT ? -1 : (wp == VK_RIGHT ? 1 : 0);
                p.y += wp == VK_UP ? -1 : (wp == VK_DOWN ? 1 : 0);
                if (MonitorFromPoint(p, MONITOR_DEFAULTTONULL)) { self->move(p); SetCursorPos(p.x, p.y); }
                return 0;
            }
            break;
        default: break;
        }
        return DefWindowProcW(hwnd, message, wp, lp);
    }
};
}
std::optional<Color> pick_screen_color(HWND owner, std::wstring& error) {
    error.clear();
    Picker picker;
    picker.origin = {GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN)};
    picker.width = GetSystemMetrics(SM_CXVIRTUALSCREEN); picker.height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    const auto count = static_cast<std::uint64_t>(std::max(0, picker.width)) * static_cast<unsigned>(std::max(0, picker.height));
    if (!count || count > 64000000 || picker.width > 32767 || picker.height > 32767) {
        error = L"No se pudo preparar el cuentagotas para esta disposición de pantallas."; return {};
    }
    DwmFlush();
    HDC desktop = GetDC(nullptr);
    if (!desktop) { error = L"No se pudo leer la pantalla."; return {}; }
    picker.snapshot = CreateCompatibleDC(desktop);
    BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = picker.width; info.bmiHeader.biHeight = -picker.height;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    picker.bitmap = CreateDIBSection(desktop, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    bool captured = false;
    if (picker.snapshot && picker.bitmap && pixels) {
        picker.old_bitmap = SelectObject(picker.snapshot, picker.bitmap);
        captured = BitBlt(picker.snapshot, 0, 0, picker.width, picker.height, desktop,
                          picker.origin.x, picker.origin.y, SRCCOPY | CAPTUREBLT) != FALSE;
        GdiFlush();
    }
    ReleaseDC(nullptr, desktop);
    if (!captured) { error = L"No se pudo leer la pantalla. Se conserva el color anterior."; return {}; }
    picker.pixels = static_cast<std::uint32_t*>(pixels);
    picker.font = CreateFontW(-13, 0, 0, 0, 400, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    WNDCLASSW cls{}; cls.hInstance = GetModuleHandleW(nullptr); cls.lpfnWndProc = Picker::proc;
    cls.lpszClassName = L"ElitePen.ScreenColorPicker"; cls.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    RegisterClassW(&cls);
    GetCursorPos(&picker.point);
    HWND window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, cls.lpszClassName,
        L"Cuentagotas · Elite Pen", WS_POPUP, picker.origin.x, picker.origin.y,
        picker.width, picker.height, owner, nullptr, cls.hInstance, &picker);
    if (!window) { error = L"No se pudo abrir el cuentagotas."; return {}; }
    ShowWindow(window, SW_SHOW); SetForegroundWindow(window); SetFocus(window); SetCapture(window);
    MSG message{};
    while (!picker.done) {
        const int next = GetMessageW(&message, nullptr, 0, 0);
        if (next <= 0) { if (!next) PostQuitMessage(static_cast<int>(message.wParam)); break; }
        TranslateMessage(&message); DispatchMessageW(&message);
    }
    if (GetCapture() == window) ReleaseCapture();
    return picker.result;
}
}
