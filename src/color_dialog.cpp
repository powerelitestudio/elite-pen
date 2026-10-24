#include "color_dialog.hpp"
#include <algorithm>
#include <array>
#include <commctrl.h>

namespace elite_pen::win {
namespace {
struct ColorDialog {
    HWND window{}, hex{}, status{}, use{}, add{}, remove{};
    std::array<HWND, 3> rgb{};
    std::array<HWND, kCustomColorLimit> chips{};
    HFONT font{};
    Color color;
    std::vector<Color>& favorites;
    const std::function<bool(const std::vector<Color>&)>& save;
    std::optional<Color> result;
    int selected{-1};
    bool updating{}, done{};
    ColorDialog(Color initial, std::vector<Color>& saved,
                const std::function<bool(const std::vector<Color>&)>& persist)
        : color(initial), favorites(saved), save(persist) {}
    bool persist(const std::vector<Color>& previous, int previous_selection) {
        if (save(favorites)) return true;
        favorites = previous; selected = previous_selection; refresh();
        SetWindowTextW(status, L"No se pudo guardar. Tu paleta anterior se conserva.");
        return false;
    }
    static std::wstring text(HWND control) {
        wchar_t value[256]{};
        GetWindowTextW(control, value, 256);
        return value;
    }
    HWND control(const wchar_t* type, const wchar_t* label, DWORD style, int id, int x, int y, int w, int h) {
        const HWND child = CreateWindowExW(type == std::wstring_view(L"EDIT") ? WS_EX_CLIENTEDGE : 0,
            type, label, WS_CHILD | WS_VISIBLE | style, x, y, w, h, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        return child;
    }
    void sync() {
        updating = true;
        SetWindowTextW(hex, color_hex(color).c_str());
        const unsigned channels[]{color.r, color.g, color.b};
        for (std::size_t n = 0; n < 3; ++n) SetWindowTextW(rgb[n], std::to_wstring(channels[n]).c_str());
        updating = false;
        valid(true);
    }
    void valid(bool ok) {
        EnableWindow(use, ok);
        EnableWindow(add, ok);
        SetWindowTextW(status, ok ? L"RGB: 0–255 · HTML: #RRGGBB" : L"Revisa el valor: RGB 0–255 o seis dígitos hexadecimales.");
        InvalidateRect(window, nullptr, TRUE);
    }
    void refresh() {
        for (std::size_t n = 0; n < chips.size(); ++n) {
            ShowWindow(chips[n], n < favorites.size() ? SW_SHOW : SW_HIDE);
            if (n < favorites.size()) SetWindowTextW(chips[n], color_hex(favorites[n]).c_str());
            InvalidateRect(chips[n], nullptr, TRUE);
        }
        EnableWindow(remove, selected >= 0 && static_cast<std::size_t>(selected) < favorites.size());
    }
    void create() {
        font = CreateFontW(-16, 0, 0, 0, 400, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        control(L"STATIC", L"Tu color", 0, 0, 22, 18, 260, 24);
        control(L"STATIC", L"HTML / hexadecimal", 0, 0, 22, 55, 200, 22);
        hex = control(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, 101, 22, 80, 180, 30);
        SendMessageW(hex, EM_SETLIMITTEXT, 7, 0);
        const wchar_t* names[]{L"Rojo", L"Verde", L"Azul"};
        for (std::size_t n = 0; n < 3; ++n) {
            const int x = 230 + static_cast<int>(n) * 92;
            control(L"STATIC", names[n], 0, 0, x, 55, 82, 22);
            rgb[n] = control(L"EDIT", L"", WS_TABSTOP | ES_NUMBER, 102 + static_cast<int>(n), x, 80, 76, 30);
            SendMessageW(rgb[n], EM_SETLIMITTEXT, 3, 0);
        }
        status = control(L"STATIC", L"", 0, 0, 22, 124, 490, 38);
        add = control(L"BUTTON", L"Guardar color", WS_TABSTOP, 105, 22, 168, 160, 34);
        remove = control(L"BUTTON", L"Quitar seleccionado", WS_TABSTOP, 106, 200, 168, 186, 34);
        control(L"STATIC", L"Mis colores · hasta 16 · se guardan en este equipo", 0, 0, 22, 224, 490, 24);
        for (std::size_t n = 0; n < chips.size(); ++n)
            chips[n] = control(L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW, 200 + static_cast<int>(n),
                22 + static_cast<int>(n % 8) * 62, 258 + static_cast<int>(n / 8) * 48, 54, 40);
        control(L"BUTTON", L"Cerrar", WS_TABSTOP, IDCANCEL, 274, 370, 110, 34);
        use = control(L"BUTTON", L"Usar color", WS_TABSTOP | BS_DEFPUSHBUTTON, IDOK, 398, 370, 116, 34);
        sync(); refresh(); SetFocus(hex);
    }
    void command(int id, int event) {
        if (id == IDCANCEL) { done = true; return; }
        if (id == IDOK) { if (IsWindowEnabled(use)) { result = color; done = true; } return; }
        if (event == EN_CHANGE && !updating && id >= 101 && id <= 104) {
            if (id == 101) {
                if (auto c = parse_color_hex(text(hex))) {
                    color = *c; updating = true;
                    const unsigned channels[]{color.r, color.g, color.b};
                    for (std::size_t n = 0; n < 3; ++n) SetWindowTextW(rgb[n], std::to_wstring(channels[n]).c_str());
                    updating = false; valid(true);
                } else valid(false);
            } else {
                const auto r = parse_color_channel(text(rgb[0])), g = parse_color_channel(text(rgb[1])), b = parse_color_channel(text(rgb[2]));
                if (!r || !g || !b) { valid(false); return; }
                color = {static_cast<std::uint8_t>(*r), static_cast<std::uint8_t>(*g), static_cast<std::uint8_t>(*b), 255};
                updating = true; SetWindowTextW(hex, color_hex(color).c_str()); updating = false; valid(true);
            }
        } else if (event == BN_CLICKED) {
            if (id >= 200 && id < 216 && static_cast<std::size_t>(id - 200) < favorites.size()) {
                selected = id - 200; color = favorites[static_cast<std::size_t>(selected)]; sync(); refresh();
            } else if (id == 105 && IsWindowEnabled(add)) {
                const auto previous = favorites;
                const int previous_selection = selected;
                const auto found = std::find(favorites.begin(), favorites.end(), color);
                if (found != favorites.end()) selected = static_cast<int>(found - favorites.begin());
                else if (favorites.size() < kCustomColorLimit) { favorites.push_back(color); selected = static_cast<int>(favorites.size() - 1); }
                else { SetWindowTextW(status, L"Ya tienes 16 colores. Quita uno antes de añadir otro."); return; }
                if (!persist(previous, previous_selection)) return;
                refresh(); SetWindowTextW(status, L"Color guardado. Estará disponible al volver a abrir Elite Pen.");
            } else if (id == 106 && selected >= 0 && static_cast<std::size_t>(selected) < favorites.size()) {
                const auto previous = favorites;
                const int previous_selection = selected;
                favorites.erase(favorites.begin() + selected); selected = -1;
                if (!persist(previous, previous_selection)) return;
                refresh();
                SetWindowTextW(status, L"Color quitado de tu paleta.");
            }
        }
    }
    static LRESULT CALLBACK proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
        auto* self = reinterpret_cast<ColorDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<ColorDialog*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
            self->window = hwnd; SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self) return DefWindowProcW(hwnd, message, wp, lp);
        if (message == WM_COMMAND) { self->command(LOWORD(wp), HIWORD(wp)); return 0; }
        if (message == WM_CLOSE) { self->done = true; return 0; }
        if (message == WM_PAINT) {
            PAINTSTRUCT paint{}; HDC dc = BeginPaint(hwnd, &paint);
            const auto c = self->color;
            HBRUSH preview = CreateSolidBrush(RGB(c.r, c.g, c.b));
            RECT r{474, 16, 514, 42}; FillRect(dc, &r, preview); DeleteObject(preview);
            FrameRect(dc, &r, GetSysColorBrush(COLOR_BTNSHADOW));
            EndPaint(hwnd, &paint); return 0;
        }
        if (message == WM_DRAWITEM) {
            auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
            const auto n = static_cast<std::size_t>(draw->CtlID - 200);
            if (n >= self->favorites.size()) return TRUE;
            const auto c = self->favorites[n];
            HBRUSH brush = CreateSolidBrush(RGB(c.r, c.g, c.b));
            RECT swatch = draw->rcItem; InflateRect(&swatch, -6, -6);
            FillRect(draw->hDC, &draw->rcItem, GetSysColorBrush(COLOR_BTNFACE));
            FillRect(draw->hDC, &swatch, brush); DeleteObject(brush);
            FrameRect(draw->hDC, &swatch, GetSysColorBrush(COLOR_BTNSHADOW));
            if (self->selected == static_cast<int>(n) || (draw->itemState & ODS_FOCUS)) DrawFocusRect(draw->hDC, &draw->rcItem);
            return TRUE;
        }
        return DefWindowProcW(hwnd, message, wp, lp);
    }
};
}
std::optional<Color> custom_color_dialog(HWND owner, Color initial, std::vector<Color>& favorites,
    const std::function<bool(const std::vector<Color>&)>& save) {
    WNDCLASSW cls{}; cls.lpfnWndProc = ColorDialog::proc; cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"ElitePen.CustomColors"; cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.hbrBackground = GetSysColorBrush(COLOR_BTNFACE); RegisterClassW(&cls);
    ColorDialog dialog(initial, favorites, save);
    RECT parent{}; GetWindowRect(owner, &parent);
    RECT bounds{0, 0, 540, 426}; AdjustWindowRectEx(&bounds, WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    MONITORINFO monitor{}; monitor.cbSize = sizeof(monitor);
    GetMonitorInfoW(MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST), &monitor);
    const int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
    const int x = std::clamp(static_cast<int>((parent.left + parent.right - width) / 2), static_cast<int>(monitor.rcWork.left), std::max(static_cast<int>(monitor.rcWork.left), static_cast<int>(monitor.rcWork.right) - width));
    const int y = std::clamp(static_cast<int>((parent.top + parent.bottom - height) / 2), static_cast<int>(monitor.rcWork.top), std::max(static_cast<int>(monitor.rcWork.top), static_cast<int>(monitor.rcWork.bottom) - height));
    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, cls.lpszClassName, L"Colores personalizados · Elite Pen",
        WS_CAPTION | WS_SYSMENU, x, y, width, height, owner, nullptr, cls.hInstance, &dialog);
    if (!hwnd) return {};
    const bool enabled = IsWindowEnabled(owner) != FALSE;
    EnableWindow(owner, FALSE); dialog.create(); ShowWindow(hwnd, SW_SHOW); SetForegroundWindow(hwnd); SetFocus(dialog.hex);
    MSG message{};
    while (!dialog.done) {
        const int result = GetMessageW(&message, nullptr, 0, 0);
        if (result <= 0) { if (result == 0) PostQuitMessage(static_cast<int>(message.wParam)); break; }
        if (!IsDialogMessageW(hwnd, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    EnableWindow(owner, enabled); DestroyWindow(hwnd); DeleteObject(dialog.font); SetForegroundWindow(owner);
    return dialog.result;
}
} // namespace elite_pen::win
