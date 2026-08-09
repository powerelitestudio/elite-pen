#include "application.hpp"

#include "elite_pen/core.hpp"
#include "graphics.hpp"
#include "preferences.hpp"

#include <windows.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <magnification.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wincodec.h>
#include <windowsx.h>

#include <array>
#include <chrono>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <sstream>
#include <utility>
#include <vector>

namespace elite_pen::win {

namespace {

constexpr UINT kTrayMessage = WM_APP + 10;
constexpr UINT kExitMessage = WM_APP + 12;
constexpr UINT kQaQueryToolMessage = WM_APP + 90;
constexpr UINT kQaQueryColorMessage = WM_APP + 91;
constexpr UINT kQaQueryThicknessMessage = WM_APP + 92;
constexpr UINT kQaCommitInlineTextMessage = WM_APP + 93;
constexpr UINT kQaQueryDocumentCountMessage = WM_APP + 94;
constexpr UINT_PTR kTrayId = 1;
constexpr float kPaletteScale = 0.60F;
constexpr int kPaletteDesignWidth = 290;
constexpr int kPaletteDesignHeight = 280;
constexpr int kPaletteWidth = static_cast<int>(kPaletteDesignWidth * kPaletteScale);
constexpr int kPaletteHeight = static_cast<int>(kPaletteDesignHeight * kPaletteScale);

constexpr int kHotkeyInteract = 1;
constexpr int kHotkeyVisibility = 2;
constexpr int kHotkeyWhiteboard = 3;
constexpr int kHotkeyUndo = 4;
constexpr int kHotkeyRedo = 5;
constexpr int kHotkeyClear = 6;
constexpr int kHotkeyZoom = 7;
constexpr int kHotkeyBlackboard = 8;

constexpr UINT kHotkeyModifiers = MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT;

enum class ZoomView : int { Fullscreen = 0, Lens = 1, Docked = 2 };

class Controller;

class WindowBase {
public:
    explicit WindowBase(Controller& controller) : controller_(controller) {}
    virtual ~WindowBase() {
        if (window_) DestroyWindow(window_);
    }

    WindowBase(const WindowBase&) = delete;
    WindowBase& operator=(const WindowBase&) = delete;

    bool create(const wchar_t* class_name, const wchar_t* title, DWORD ex_style,
                DWORD style, const RECT& bounds, HWND parent = nullptr);
    bool initialize_surface(GraphicsDevice& graphics);
    void invalidate() const { if (window_) InvalidateRect(window_, nullptr, FALSE); }
    [[nodiscard]] HWND hwnd() const noexcept { return window_; }
    [[nodiscard]] Surface& surface() noexcept { return surface_; }

protected:
    virtual LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam);
    virtual void render() {}
    Controller& controller_;
    HWND window_{};
    Surface surface_;

private:
    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam,
                                        LPARAM lparam);
};

class OverlayWindow final : public WindowBase {
public:
    OverlayWindow(Controller& controller, RECT monitor_rect)
        : WindowBase(controller), monitor_rect_(monitor_rect) {}

    bool initialize(GraphicsDevice& graphics);
    void update_interaction();
    void cancel_gesture();
    [[nodiscard]] const RECT& monitor_rect() const noexcept { return monitor_rect_; }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;
    void render() override;

private:
    PointF global_point(LPARAM lparam) const noexcept;
    std::optional<PointF> pointer_point(WPARAM wparam) const noexcept;
    void begin_gesture(PointF point, float pressure = 1.0F);
    void update_gesture(PointF point, WPARAM keys);
    void finish_gesture(PointF point, WPARAM keys);

    RECT monitor_rect_{};
    bool drawing_{};
    bool erasing_{};
    bool pointer_active_{};
    UINT32 pointer_id_{};
};

class PaletteWindow final : public WindowBase {
public:
    explicit PaletteWindow(Controller& controller) : WindowBase(controller) {}
    bool initialize(GraphicsDevice& graphics);
    void install_hotkeys();
    void remove_hotkeys();
    void add_tray_icon();
    void remove_tray_icon();
    void show_notification(const wchar_t* title, const std::wstring& message);
    [[nodiscard]] bool activate_command_at(POINT point);
    [[nodiscard]] bool contains_screen_point(POINT point) const;

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;
    void render() override;

private:
    void install_tooltips();
    [[nodiscard]] bool command_at(POINT point) const;
    void activate_at(POINT point);
    void show_tool_menu();
    void show_tray_menu();
    void choose_custom_color();
    bool dragging_{};
    POINT drag_origin_{};
    POINT window_origin_{};
    NOTIFYICONDATAW tray_{};
    bool escape_down_{};
    HWND tooltip_{};
};

class ColorWindow final : public WindowBase {
public:
    explicit ColorWindow(Controller& controller) : WindowBase(controller) {}
    bool initialize(GraphicsDevice& graphics);
    void toggle_near(HWND anchor);
    void hide() { ShowWindow(window_, SW_HIDE); }
    [[nodiscard]] bool visible() const noexcept { return IsWindowVisible(window_) != FALSE; }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;
    void render() override;

private:
    void choose_system_color();
};

class ToolWindow final : public WindowBase {
public:
    explicit ToolWindow(Controller& controller) : WindowBase(controller) {}
    bool initialize(GraphicsDevice& graphics);
    void toggle_near(HWND anchor);
    void toggle_geometry_near(HWND anchor);
    void hide() { ShowWindow(window_, SW_HIDE); }
    [[nodiscard]] bool visible() const noexcept { return IsWindowVisible(window_) != FALSE; }
    [[nodiscard]] bool geometry_only() const noexcept { return geometry_only_; }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;
    void render() override;

private:
    void show_near(HWND anchor, bool geometry_only);
    [[nodiscard]] std::size_t tool_count() const noexcept;
    [[nodiscard]] Tool tool_at(std::size_t index) const noexcept;
    bool geometry_only_{};
};

class TextInputWindow final : public WindowBase {
public:
    explicit TextInputWindow(Controller& controller) : WindowBase(controller) {}
    bool initialize(GraphicsDevice& graphics);
    void show_at(PointF position, Color color, float thickness);
    void update_style(Color color, float thickness);
    void commit();
    void cancel();
    [[nodiscard]] bool active() const noexcept { return active_; }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;
    void render() override;

private:
    PointF position_{};
    Color color_{};
    float thickness_{};
    std::wstring text_;
    bool active_{};
    bool caret_visible_{true};
};

class SettingsWindow final : public WindowBase {
public:
    explicit SettingsWindow(Controller& controller) : WindowBase(controller) {}
    bool initialize();
    void show_settings();

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;

private:
    void refresh_controls();
    HWND title_{};
    HWND capture_{};
    HWND confirm_clear_{};
    HWND start_interact_{};
    HWND highlight_cursor_{};
    HWND fade_label_{};
    HWND fade_{};
    HWND thickness_label_{};
    HWND thickness_{};
    HWND zoom_label_{};
    HWND zoom_{};
    HWND zoom_view_label_{};
    HWND zoom_view_{};
    HWND zoom_invert_{};
    HWND reset_position_{};
    HWND shortcuts_{};
    HWND close_{};
};

class ZoomWindow final : public WindowBase {
public:
    explicit ZoomWindow(Controller& controller) : WindowBase(controller) {}
    ~ZoomWindow() override;

    bool initialize(GraphicsDevice& graphics);
    bool show_zoom();
    void hide_zoom();
    [[nodiscard]] bool active() const noexcept { return active_; }

protected:
    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) override;

private:
    void refresh_source();
    void apply_color_effect();
    void cycle_view();
    bool load_magnification();

    using MagInitializePointer = BOOL(WINAPI*)();
    using MagUninitializePointer = BOOL(WINAPI*)();
    using MagSetWindowSourcePointer = BOOL(WINAPI*)(HWND, RECT);
    using MagSetWindowTransformPointer = BOOL(WINAPI*)(HWND, PMAGTRANSFORM);
    using MagSetWindowFilterListPointer = BOOL(WINAPI*)(HWND, DWORD, int, HWND*);
    using MagSetColorEffectPointer = BOOL(WINAPI*)(HWND, PMAGCOLOREFFECT);

    HMODULE magnification_module_{};
    MagInitializePointer mag_initialize_{};
    MagUninitializePointer mag_uninitialize_{};
    MagSetWindowSourcePointer mag_set_source_{};
    MagSetWindowTransformPointer mag_set_transform_{};
    MagSetWindowFilterListPointer mag_set_filter_{};
    MagSetColorEffectPointer mag_set_color_effect_{};
    HWND magnifier_{};
    RECT monitor_rect_{};
    bool initialized_{};
    bool active_{};
    bool overview_{};
};

struct TransientDrawable {
    Drawable drawable;
    std::uint64_t expires_at_ms{};
};

class Controller {
public:
    bool initialize(std::wstring& error);
    int message_loop();
    void shutdown();

    AppState& state() noexcept { return state_; }
    GraphicsDevice& graphics() noexcept { return graphics_; }
    Preferences& preferences() noexcept { return preferences_; }
    PaletteWindow* palette() const noexcept { return palette_.get(); }
    const std::vector<std::unique_ptr<OverlayWindow>>& overlays() const noexcept {
        return overlays_;
    }
    std::optional<Drawable>& preview() noexcept { return preview_; }
    const std::optional<Drawable>& preview() const noexcept { return preview_; }
    const std::vector<TransientDrawable>& transient_drawables() const noexcept {
        return transient_drawables_;
    }

    void invalidate_all();
    void invalidate_document();
    void update_overlay_interaction();
    [[nodiscard]] bool route_palette_command(PointF screen_point);
    void set_tool(Tool tool);
    void set_color(Color color);
    void set_thickness(float thickness);
    void toggle_visibility();
    void toggle_whiteboard();
    void toggle_blackboard();
    void clear_document();
    void undo();
    void redo();
    void toggle_zoom();
    void toggle_color_panel();
    void toggle_tool_panel();
    void toggle_geometry_panel();
    void close_panels();
    void stop_transient_mode();
    void begin_text(PointF position);
    void commit_text(PointF position, Color color, float thickness, std::wstring text);
    void commit_drawable(Drawable drawable);
    void update_transient_ink();
    void capture_region(PointF first, PointF second);
    void rebuild_overlays();
    void report_runtime_error(const std::wstring& message);
    void request_exit();
    void save_palette_position();
    void show_settings_window();
    void apply_capture_preference();
    void reset_palette_position();
    void save_preferences();

private:
    static BOOL CALLBACK collect_monitor(HMONITOR monitor, HDC, LPRECT, LPARAM data);
    void create_overlays(std::wstring& error);

    GraphicsDevice graphics_;
    AppState state_;
    PreferencesStore preferences_store_;
    Preferences preferences_;
    std::optional<Drawable> preview_;
    std::vector<TransientDrawable> transient_drawables_;
    std::vector<std::unique_ptr<OverlayWindow>> overlays_;
    std::unique_ptr<PaletteWindow> palette_;
    std::unique_ptr<ColorWindow> colors_;
    std::unique_ptr<ToolWindow> tools_;
    std::unique_ptr<TextInputWindow> text_input_;
    std::unique_ptr<SettingsWindow> settings_;
    std::unique_ptr<ZoomWindow> zoom_;
    bool shutting_down_{};
};

D2D1_COLOR_F d2d_color(Color color, float opacity = 1.0F) {
    return D2D1::ColorF(static_cast<float>(color.r) / 255.0F,
                        static_cast<float>(color.g) / 255.0F,
                        static_cast<float>(color.b) / 255.0F,
                        static_cast<float>(color.a) / 255.0F * opacity);
}

std::uint64_t monotonic_milliseconds() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

constexpr std::array<Color, 42> kExtendedColors{{
    {24, 24, 27, 255}, {82, 82, 91, 255}, {161, 161, 170, 255},
    {228, 228, 231, 255}, {255, 255, 255, 255}, {120, 72, 40, 255},
    {239, 68, 68, 255}, {249, 115, 22, 255}, {245, 158, 11, 255},
    {255, 190, 45, 255}, {250, 204, 21, 255}, {234, 179, 8, 255},
    {132, 204, 22, 255}, {34, 197, 94, 255}, {16, 185, 129, 255},
    {20, 184, 166, 255}, {6, 182, 212, 255}, {14, 165, 233, 255},
    {31, 136, 229, 255}, {59, 130, 246, 255}, {99, 102, 241, 255},
    {139, 92, 246, 255}, {168, 85, 247, 255}, {217, 70, 239, 255},
    {236, 72, 153, 255}, {244, 63, 94, 255}, {190, 24, 93, 255},
    {127, 29, 29, 255}, {154, 52, 18, 255}, {133, 77, 14, 255},
    {63, 98, 18, 255}, {22, 101, 52, 255}, {17, 94, 89, 255},
    {21, 94, 117, 255}, {30, 64, 175, 255}, {49, 46, 129, 255},
    {88, 28, 135, 255}, {112, 26, 117, 255}, {131, 24, 67, 255},
    {255, 128, 161, 255}, {92, 225, 230, 255}, {137, 245, 173, 255}
}};

constexpr std::array<Tool, 12> kTools{{
    Tool::Interact, Tool::Pen, Tool::Highlighter, Tool::Eraser, Tool::Line,
    Tool::Rectangle, Tool::Ellipse, Tool::Arrow, Tool::CurvedArrow, Tool::Text,
    Tool::Screenshot, Tool::Zoom
}};

constexpr std::array<Tool, 5> kGeometryTools{{
    Tool::Line, Tool::Rectangle, Tool::Ellipse, Tool::Arrow, Tool::CurvedArrow
}};

bool point_in_circle(POINT point, float cx, float cy, float radius) {
    const float dx = static_cast<float>(point.x) - cx;
    const float dy = static_cast<float>(point.y) - cy;
    return dx * dx + dy * dy <= radius * radius;
}

bool point_near_segment(POINT point, float start_x, float start_y,
                        float end_x, float end_y, float radius) {
    const float segment_x = end_x - start_x;
    const float segment_y = end_y - start_y;
    const float length_squared = segment_x * segment_x + segment_y * segment_y;
    if (length_squared <= 0.0F) return point_in_circle(point, start_x, start_y, radius);
    const float offset_x = static_cast<float>(point.x) - start_x;
    const float offset_y = static_cast<float>(point.y) - start_y;
    const float position = std::clamp(
        (offset_x * segment_x + offset_y * segment_y) / length_squared, 0.0F, 1.0F);
    const float closest_x = start_x + position * segment_x;
    const float closest_y = start_y + position * segment_y;
    return point_in_circle(point, closest_x, closest_y, radius);
}

POINT palette_logical_point(POINT point) noexcept {
    return {
        static_cast<LONG>(std::lround(static_cast<float>(point.x) / kPaletteScale)),
        static_cast<LONG>(std::lround(static_cast<float>(point.y) / kPaletteScale))
    };
}

RECT palette_scaled_rect(RECT bounds) noexcept {
    return {
        static_cast<LONG>(std::lround(static_cast<float>(bounds.left) * kPaletteScale)),
        static_cast<LONG>(std::lround(static_cast<float>(bounds.top) * kPaletteScale)),
        static_cast<LONG>(std::lround(static_cast<float>(bounds.right) * kPaletteScale)),
        static_cast<LONG>(std::lround(static_cast<float>(bounds.bottom) * kPaletteScale))
    };
}

bool capture_desktop_duplication(GraphicsDevice& graphics, int left, int top,
                                 int width, int height, void* pixels) {
    ComPtr<IDXGIDevice> dxgi_device;
    if (FAILED(graphics.d3d()->QueryInterface(IID_PPV_ARGS(&dxgi_device)))) return false;
    ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgi_device->GetAdapter(adapter.GetAddressOf()))) return false;

    const RECT requested{left, top, left + width, top + height};
    std::uint64_t copied_area = 0;
    for (UINT output_index = 0;; ++output_index) {
        ComPtr<IDXGIOutput> output;
        const HRESULT enumerate = adapter->EnumOutputs(output_index, output.GetAddressOf());
        if (enumerate == DXGI_ERROR_NOT_FOUND) break;
        if (FAILED(enumerate)) return false;
        DXGI_OUTPUT_DESC output_description{};
        if (FAILED(output->GetDesc(&output_description)) ||
            !output_description.AttachedToDesktop) continue;
        RECT intersection{};
        if (!IntersectRect(&intersection, &requested,
                           &output_description.DesktopCoordinates)) continue;

        ComPtr<IDXGIOutput1> output1;
        if (FAILED(output.As(&output1))) continue;
        ComPtr<IDXGIOutputDuplication> duplication;
        if (FAILED(output1->DuplicateOutput(graphics.d3d(),
                                             duplication.GetAddressOf()))) continue;
        DXGI_OUTDUPL_FRAME_INFO frame_information{};
        ComPtr<IDXGIResource> frame_resource;
        HRESULT acquired = duplication->AcquireNextFrame(
            350, &frame_information, frame_resource.GetAddressOf());
        if (FAILED(acquired)) continue;

        ComPtr<ID3D11Texture2D> source;
        acquired = frame_resource.As(&source);
        if (FAILED(acquired)) {
            duplication->ReleaseFrame();
            continue;
        }
        D3D11_TEXTURE2D_DESC description{};
        source->GetDesc(&description);
        description.Usage = D3D11_USAGE_STAGING;
        description.BindFlags = 0;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        description.MiscFlags = 0;
        description.ArraySize = 1;
        description.MipLevels = 1;
        description.SampleDesc = {1, 0};
        ComPtr<ID3D11Texture2D> staging;
        HRESULT copied = graphics.d3d()->CreateTexture2D(
            &description, nullptr, staging.GetAddressOf());
        if (SUCCEEDED(copied)) {
            graphics.d3d_context()->CopyResource(staging.Get(), source.Get());
            D3D11_MAPPED_SUBRESOURCE mapped{};
            copied = graphics.d3d_context()->Map(staging.Get(), 0,
                                                  D3D11_MAP_READ, 0, &mapped);
            if (SUCCEEDED(copied)) {
                const int source_x = intersection.left -
                    output_description.DesktopCoordinates.left;
                const int source_y = intersection.top -
                    output_description.DesktopCoordinates.top;
                const int destination_x = intersection.left - left;
                const int destination_y = intersection.top - top;
                const std::size_t row_bytes = static_cast<std::size_t>(
                    intersection.right - intersection.left) * 4U;
                for (int row = 0; row < intersection.bottom - intersection.top; ++row) {
                    const auto* source_row = static_cast<const std::uint8_t*>(mapped.pData) +
                        static_cast<std::size_t>(source_y + row) * mapped.RowPitch +
                        static_cast<std::size_t>(source_x) * 4U;
                    auto* destination_row = static_cast<std::uint8_t*>(pixels) +
                        (static_cast<std::size_t>(destination_y + row) *
                         static_cast<std::size_t>(width) +
                         static_cast<std::size_t>(destination_x)) * 4U;
                    std::memcpy(destination_row, source_row, row_bytes);
                }
                graphics.d3d_context()->Unmap(staging.Get(), 0);
                copied_area += static_cast<std::uint64_t>(
                    intersection.right - intersection.left) *
                    static_cast<std::uint64_t>(intersection.bottom - intersection.top);
            }
        }
        duplication->ReleaseFrame();
    }
    return copied_area >= static_cast<std::uint64_t>(width) *
                          static_cast<std::uint64_t>(height);
}

void draw_arrow_head(ID2D1DeviceContext* context, ID2D1Brush* brush, PointF before,
                     PointF end, float width) {
    const float angle = std::atan2(end.y - before.y, end.x - before.x);
    const float size = std::clamp(width * 3.2F, 12.0F, 38.0F);
    const float spread = 0.62F;
    const D2D1_POINT_2F left{
        end.x - size * std::cos(angle - spread),
        end.y - size * std::sin(angle - spread)};
    const D2D1_POINT_2F right{
        end.x - size * std::cos(angle + spread),
        end.y - size * std::sin(angle + spread)};
    context->DrawLine(D2D1::Point2F(end.x, end.y), left, brush, width);
    context->DrawLine(D2D1::Point2F(end.x, end.y), right, brush, width);
}

void draw_drawable(GraphicsDevice& graphics, ID2D1DeviceContext* context,
                   const Drawable& drawable, float offset_x, float offset_y,
                   float opacity = 1.0F) {
    if (drawable.points.empty()) return;
    const float alpha = drawable.kind == Tool::Highlighter ? 0.34F * opacity : opacity;
    ComPtr<ID2D1SolidColorBrush> brush;
    context->CreateSolidColorBrush(d2d_color(drawable.color, alpha), brush.GetAddressOf());
    if (!brush) return;

    ComPtr<ID2D1StrokeStyle> stroke_style;
    D2D1_STROKE_STYLE_PROPERTIES properties = D2D1::StrokeStyleProperties(
        D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
        D2D1_LINE_JOIN_ROUND, 10.0F, D2D1_DASH_STYLE_SOLID, 0.0F);
    graphics.d2d_factory()->CreateStrokeStyle(properties, nullptr, 0,
                                               stroke_style.GetAddressOf());

    const auto local = [offset_x, offset_y](PointF point) {
        return D2D1::Point2F(point.x - offset_x, point.y - offset_y);
    };

    if ((drawable.kind == Tool::Rectangle || drawable.kind == Tool::Screenshot) &&
        drawable.points.size() >= 2) {
        const auto a = local(drawable.points.front());
        const auto b = local(drawable.points.back());
        const auto rectangle = D2D1::RectF(std::min(a.x, b.x), std::min(a.y, b.y),
                                            std::max(a.x, b.x), std::max(a.y, b.y));
        if (drawable.kind == Tool::Screenshot) {
            ComPtr<ID2D1SolidColorBrush> shade;
            context->CreateSolidColorBrush(D2D1::ColorF(0x1F88E5, 0.12F), shade.GetAddressOf());
            context->FillRectangle(rectangle, shade.Get());
        }
        context->DrawRectangle(rectangle, brush.Get(), drawable.width, stroke_style.Get());
        return;
    }
    if (drawable.kind == Tool::Ellipse && drawable.points.size() >= 2) {
        const auto a = local(drawable.points.front());
        const auto b = local(drawable.points.back());
        const float left = std::min(a.x, b.x);
        const float top = std::min(a.y, b.y);
        const float right = std::max(a.x, b.x);
        const float bottom = std::max(a.y, b.y);
        context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F((left + right) * 0.5F,
                                                         (top + bottom) * 0.5F),
                                           (right - left) * 0.5F, (bottom - top) * 0.5F),
                             brush.Get(), drawable.width, stroke_style.Get());
        return;
    }
    if (drawable.kind == Tool::Text) {
        ComPtr<IDWriteTextFormat> format;
        const float font_size = std::clamp(drawable.width * 3.4F, 16.0F, 72.0F);
        graphics.dwrite()->CreateTextFormat(L"Segoe UI", nullptr,
            DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, font_size, L"es-CO", format.GetAddressOf());
        if (format) {
            const auto origin = local(drawable.points.front());
            context->DrawTextW(drawable.text.c_str(), static_cast<UINT32>(drawable.text.size()),
                               format.Get(), D2D1::RectF(origin.x, origin.y,
                                                        origin.x + 600.0F,
                                                        origin.y + 400.0F), brush.Get());
        }
        return;
    }

    if (drawable.kind == Tool::CurvedArrow && drawable.points.size() >= 2) {
        const auto curve = curved_arrow_bezier(drawable.points.front(), drawable.points.back());
        ComPtr<ID2D1PathGeometry> geometry;
        graphics.d2d_factory()->CreatePathGeometry(geometry.GetAddressOf());
        if (!geometry) return;
        ComPtr<ID2D1GeometrySink> sink;
        geometry->Open(sink.GetAddressOf());
        sink->BeginFigure(local(curve.start), D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddBezier(D2D1::BezierSegment(local(curve.control1), local(curve.control2),
                                             local(curve.end)));
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
        sink->Close();
        context->DrawGeometry(geometry.Get(), brush.Get(), drawable.width, stroke_style.Get());
        PointF tangent = curve.control2;
        PointF end = curve.end;
        tangent.x -= offset_x;
        tangent.y -= offset_y;
        end.x -= offset_x;
        end.y -= offset_y;
        draw_arrow_head(context, brush.Get(), tangent, end, drawable.width);
        return;
    }

    if (drawable.points.size() == 1) {
        const auto point = local(drawable.points.front());
        context->FillEllipse(D2D1::Ellipse(point, drawable.width * 0.5F,
                                           drawable.width * 0.5F), brush.Get());
        return;
    }

    ComPtr<ID2D1PathGeometry> geometry;
    graphics.d2d_factory()->CreatePathGeometry(geometry.GetAddressOf());
    if (!geometry) return;
    ComPtr<ID2D1GeometrySink> sink;
    geometry->Open(sink.GetAddressOf());
    sink->BeginFigure(local(drawable.points.front()), D2D1_FIGURE_BEGIN_HOLLOW);
    const bool smooth = drawable.points.size() >= 3 &&
        (drawable.kind == Tool::Pen || drawable.kind == Tool::Highlighter);
    if (smooth) {
        for (std::size_t index = 0; index + 1 < drawable.points.size(); ++index) {
            const PointF p0 = drawable.points[index == 0 ? 0 : index - 1];
            const PointF p1 = drawable.points[index];
            const PointF p2 = drawable.points[index + 1];
            const PointF p3 = drawable.points[
                std::min(index + 2, drawable.points.size() - 1)];
            const PointF c1{p1.x + (p2.x - p0.x) / 6.0F,
                            p1.y + (p2.y - p0.y) / 6.0F};
            const PointF c2{p2.x - (p3.x - p1.x) / 6.0F,
                            p2.y - (p3.y - p1.y) / 6.0F};
            sink->AddBezier(D2D1::BezierSegment(local(c1), local(c2), local(p2)));
        }
    } else {
        for (std::size_t index = 1; index < drawable.points.size(); ++index) {
            sink->AddLine(local(drawable.points[index]));
        }
    }
    sink->EndFigure(D2D1_FIGURE_END_OPEN);
    sink->Close();
    context->DrawGeometry(geometry.Get(), brush.Get(), drawable.width, stroke_style.Get());
    if (drawable.kind == Tool::Arrow && drawable.points.size() >= 2) {
        PointF before = drawable.points[drawable.points.size() - 2];
        PointF end = drawable.points.back();
        before.x -= offset_x;
        before.y -= offset_y;
        end.x -= offset_x;
        end.y -= offset_y;
        draw_arrow_head(context, brush.Get(), before, end, drawable.width);
    }
}

bool WindowBase::create(const wchar_t* class_name, const wchar_t* title, DWORD ex_style,
                        DWORD style, const RECT& bounds, HWND parent) {
    WNDCLASSEXW existing{};
    existing.cbSize = sizeof(existing);
    if (!GetClassInfoExW(GetModuleHandleW(nullptr), class_name, &existing)) {
        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.style = CS_HREDRAW | CS_VREDRAW;
        window_class.lpfnWndProc = &WindowBase::window_proc;
        window_class.hInstance = GetModuleHandleW(nullptr);
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101));
        window_class.hIconSm = window_class.hIcon;
        window_class.lpszClassName = class_name;
        if (!RegisterClassExW(&window_class)) return false;
    }
    window_ = CreateWindowExW(ex_style, class_name, title, style,
                              bounds.left, bounds.top, bounds.right - bounds.left,
                              bounds.bottom - bounds.top, parent, nullptr,
                              GetModuleHandleW(nullptr), this);
    return window_ != nullptr;
}

bool WindowBase::initialize_surface(GraphicsDevice& graphics) {
    RECT client{};
    GetClientRect(window_, &client);
    std::wstring error;
    if (!surface_.initialize(graphics, window_, static_cast<UINT>(client.right),
                             static_cast<UINT>(client.bottom), error)) {
        controller_.report_runtime_error(error);
        return false;
    }
    return true;
}

LRESULT WindowBase::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_SIZE && surface_.context()) {
        std::wstring error;
        if (!surface_.resize(LOWORD(lparam), HIWORD(lparam), error)) {
            controller_.report_runtime_error(error);
        }
        invalidate();
        return 0;
    }
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        BeginPaint(window_, &paint);
        render();
        EndPaint(window_, &paint);
        return 0;
    }
    return DefWindowProcW(window_, message, wparam, lparam);
}

LRESULT CALLBACK WindowBase::window_proc(HWND window, UINT message, WPARAM wparam,
                                         LPARAM lparam) {
    WindowBase* self = reinterpret_cast<WindowBase*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* creation = reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = static_cast<WindowBase*>(creation->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self) return DefWindowProcW(window, message, wparam, lparam);
    if (message == WM_NCDESTROY) {
        const LRESULT result = self->handle_message(message, wparam, lparam);
        self->window_ = nullptr;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        return result;
    }
    return self->handle_message(message, wparam, lparam);
}

bool OverlayWindow::initialize(GraphicsDevice& graphics) {
    constexpr DWORD ex_style = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                               WS_EX_LAYERED;
    if (!create(L"ElitePen.Overlay", L"Elite Pen Overlay", ex_style, WS_POPUP,
                monitor_rect_)) return false;
    SetLayeredWindowAttributes(window_, 0, 255, LWA_ALPHA);
    if (!initialize_surface(graphics)) return false;
    SetWindowDisplayAffinity(window_, WDA_NONE);
    ShowWindow(window_, SW_SHOWNOACTIVATE);
    update_interaction();
    return true;
}

void OverlayWindow::update_interaction() {
    LONG_PTR style = GetWindowLongPtrW(window_, GWL_EXSTYLE);
    if (controller_.state().tool == Tool::Interact) style |= WS_EX_TRANSPARENT;
    else style &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
    SetWindowLongPtrW(window_, GWL_EXSTYLE, style);
    SetWindowPos(window_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void OverlayWindow::cancel_gesture() {
    if (!drawing_ && !erasing_) return;
    drawing_ = false;
    erasing_ = false;
    pointer_active_ = false;
    if (GetCapture() == window_) ReleaseCapture();
    controller_.state().document.end_compound();
    controller_.preview().reset();
    controller_.invalidate_document();
}

PointF OverlayWindow::global_point(LPARAM lparam) const noexcept {
    return {static_cast<float>(GET_X_LPARAM(lparam) + monitor_rect_.left),
            static_cast<float>(GET_Y_LPARAM(lparam) + monitor_rect_.top)};
}

std::optional<PointF> OverlayWindow::pointer_point(WPARAM wparam) const noexcept {
    POINTER_INFO info{};
    if (!GetPointerInfo(GET_POINTERID_WPARAM(wparam), &info)) return std::nullopt;
    return PointF{static_cast<float>(info.ptPixelLocation.x),
                  static_cast<float>(info.ptPixelLocation.y)};
}

void OverlayWindow::begin_gesture(PointF point, float pressure) {
    if (controller_.route_palette_command(point)) return;
    const Tool tool = controller_.state().tool;
    if (tool == Tool::Eraser) {
        controller_.state().document.begin_compound();
        erasing_ = true;
        if (controller_.state().document.erase_at(point,
                std::max(12.0F, controller_.state().thickness * 2.5F)))
            controller_.invalidate_document();
        drawing_ = true;
        SetCapture(window_);
        return;
    }
    if (tool == Tool::Text) {
        controller_.begin_text(point);
        return;
    }
    if (!is_drawing_tool(tool)) return;
    Drawable drawable;
    drawable.kind = tool;
    drawable.color = controller_.state().color;
    drawable.width = controller_.state().effective_width() *
        std::clamp(pressure, 0.35F, 1.45F);
    drawable.points.push_back(point);
    if (tool == Tool::Line || tool == Tool::Rectangle || tool == Tool::Ellipse ||
        tool == Tool::Arrow || tool == Tool::CurvedArrow || tool == Tool::Screenshot) {
        drawable.points.push_back(point);
    }
    controller_.preview() = std::move(drawable);
    drawing_ = true;
    SetCapture(window_);
    controller_.invalidate_document();
}

void OverlayWindow::update_gesture(PointF point, WPARAM keys) {
    if (!drawing_) return;
    if (controller_.state().tool == Tool::Eraser) {
        if ((keys & MK_LBUTTON) && controller_.state().document.erase_at(
                point, std::max(12.0F, controller_.state().thickness * 2.5F)))
            controller_.invalidate_document();
        return;
    }
    auto& preview = controller_.preview();
    if (!preview) return;
    preview->invalidate_bounds_cache();
    const Tool tool = preview->kind;
    if (tool == Tool::Line || tool == Tool::Rectangle || tool == Tool::Ellipse ||
        tool == Tool::Arrow || tool == Tool::CurvedArrow || tool == Tool::Screenshot) {
        if ((keys & MK_SHIFT) && (tool == Tool::Rectangle || tool == Tool::Ellipse)) {
            const PointF origin = preview->points.front();
            const float dx = point.x - origin.x;
            const float dy = point.y - origin.y;
            const float size = std::max(std::abs(dx), std::abs(dy));
            point.x = origin.x + std::copysign(size, dx == 0 ? 1.0F : dx);
            point.y = origin.y + std::copysign(size, dy == 0 ? 1.0F : dy);
        }
        preview->points.back() = point;
    } else if (distance(preview->points.back(), point) >= 1.0F) {
        if (preview->points.size() >= 8192) {
            std::vector<PointF> reduced;
            reduced.reserve(preview->points.size() / 2 + 1);
            for (std::size_t index = 0; index < preview->points.size(); index += 2) {
                reduced.push_back(preview->points[index]);
            }
            if (reduced.back().x != preview->points.back().x ||
                reduced.back().y != preview->points.back().y) {
                reduced.push_back(preview->points.back());
            }
            preview->points = std::move(reduced);
        }
        preview->points.push_back(point);
    }
    controller_.invalidate_document();
}

void OverlayWindow::finish_gesture(PointF point, WPARAM keys) {
    if (!drawing_) return;
    update_gesture(point, keys);
    drawing_ = false;
    if (GetCapture() == window_) ReleaseCapture();
    if (erasing_) {
        controller_.state().document.end_compound();
        erasing_ = false;
        controller_.invalidate_document();
        return;
    }
    auto& preview = controller_.preview();
    if (!preview) return;
    Drawable completed = std::move(*preview);
    preview.reset();
    if (completed.kind == Tool::Screenshot && completed.points.size() >= 2) {
        controller_.invalidate_document();
        UpdateWindow(window_);
        controller_.capture_region(completed.points.front(), completed.points.back());
        controller_.set_tool(Tool::Interact);
        return;
    }
    if (completed.kind == Tool::Pen || completed.kind == Tool::Highlighter) {
        completed.points = simplify_path(completed.points,
            std::clamp(completed.width * 0.08F, 0.5F, 2.0F));
    }
    if (completed.points.size() >= 2 &&
        distance(completed.points.front(), completed.points.back()) < 2.0F) {
        completed.points.resize(1);
    }
    controller_.commit_drawable(std::move(completed));
    controller_.invalidate_document();
}

LRESULT OverlayWindow::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_SETCURSOR:
            if (LOWORD(lparam) == HTCLIENT) {
                const Tool tool = controller_.state().tool;
                const wchar_t* cursor = tool == Tool::Interact ? IDC_ARROW
                    : (tool == Tool::Text ? IDC_IBEAM : IDC_CROSS);
                SetCursor(LoadCursorW(nullptr, cursor));
                return TRUE;
            }
            break;
        case WM_NCHITTEST:
            if (controller_.palette() && controller_.palette()->contains_screen_point(
                    {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)})) {
                return HTTRANSPARENT;
            }
            if (controller_.state().tool == Tool::Interact) return HTTRANSPARENT;
            return HTCLIENT;
        case WM_LBUTTONDOWN:
            if (pointer_active_) return 0;
            begin_gesture(global_point(lparam));
            return 0;
        case WM_MOUSEMOVE:
            if (pointer_active_) return 0;
            update_gesture(global_point(lparam), wparam);
            return 0;
        case WM_LBUTTONUP:
            if (pointer_active_) return 0;
            finish_gesture(global_point(lparam), wparam);
            return 0;
        case WM_POINTERDOWN: {
            const UINT32 pointer_id = GET_POINTERID_WPARAM(wparam);
            POINTER_INPUT_TYPE type{};
            if (!GetPointerType(pointer_id, &type) ||
                (type != PT_PEN && type != PT_TOUCH)) return 0;
            const auto point = pointer_point(wparam);
            if (!point) return 0;
            float pressure = 1.0F;
            if (type == PT_PEN) {
                POINTER_PEN_INFO pen{};
                if (GetPointerPenInfo(pointer_id, &pen) &&
                    (pen.penMask & PEN_MASK_PRESSURE) != 0) {
                    const float normalized = static_cast<float>(pen.pressure) / 1024.0F;
                    pressure = 0.45F + normalized * 1.0F;
                }
            }
            pointer_active_ = true;
            pointer_id_ = pointer_id;
            begin_gesture(*point, pressure);
            return 0;
        }
        case WM_POINTERUPDATE: {
            if (!pointer_active_ || GET_POINTERID_WPARAM(wparam) != pointer_id_) return 0;
            const auto point = pointer_point(wparam);
            if (point) {
                WPARAM keys = MK_LBUTTON;
                if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) keys |= MK_SHIFT;
                update_gesture(*point, keys);
            }
            return 0;
        }
        case WM_POINTERUP: {
            if (!pointer_active_ || GET_POINTERID_WPARAM(wparam) != pointer_id_) return 0;
            const auto point = pointer_point(wparam);
            if (point) finish_gesture(*point, 0);
            pointer_active_ = false;
            pointer_id_ = 0;
            return 0;
        }
        case WM_POINTERCAPTURECHANGED:
            pointer_active_ = false;
            pointer_id_ = 0;
            if (drawing_ && controller_.preview())
                finish_gesture(controller_.preview()->points.back(), 0);
            else if (erasing_) cancel_gesture();
            return 0;
        case WM_CAPTURECHANGED:
            if (drawing_ && controller_.preview())
                finish_gesture(controller_.preview()->points.back(), 0);
            else if (erasing_) cancel_gesture();
            drawing_ = false;
            return 0;
        case WM_RBUTTONDOWN:
            controller_.stop_transient_mode();
            return 0;
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) controller_.stop_transient_mode();
            return 0;
        default:
            return WindowBase::handle_message(message, wparam, lparam);
    }
    return WindowBase::handle_message(message, wparam, lparam);
}

void OverlayWindow::render() {
    const D2D1_COLOR_F background = controller_.state().whiteboard
        ? D2D1::ColorF(D2D1::ColorF::White)
        : (controller_.state().blackboard ? D2D1::ColorF(0x111318)
                                          : D2D1::ColorF(0, 0.0F));
    auto* context = surface_.begin_draw(background);
    if (!context) return;
    if (controller_.state().annotations_visible) {
        for (const auto& drawable : controller_.state().document.items()) {
            draw_drawable(controller_.graphics(), context, drawable,
                          static_cast<float>(monitor_rect_.left),
                          static_cast<float>(monitor_rect_.top));
        }
        const std::uint64_t now = monotonic_milliseconds();
        for (const auto& transient : controller_.transient_drawables()) {
            const std::uint64_t remaining = transient.expires_at_ms > now
                ? transient.expires_at_ms - now : 0;
            const float opacity = remaining >= 1200U ? 1.0F
                : static_cast<float>(remaining) / 1200.0F;
            draw_drawable(controller_.graphics(), context, transient.drawable,
                          static_cast<float>(monitor_rect_.left),
                          static_cast<float>(monitor_rect_.top), opacity);
        }
        if (controller_.preview()) {
            draw_drawable(controller_.graphics(), context, *controller_.preview(),
                          static_cast<float>(monitor_rect_.left),
                          static_cast<float>(monitor_rect_.top), 0.82F);
        }
    }
    if (controller_.state().cursor_highlight) {
        POINT cursor{};
        GetCursorPos(&cursor);
        if (cursor.x >= monitor_rect_.left && cursor.x < monitor_rect_.right &&
            cursor.y >= monitor_rect_.top && cursor.y < monitor_rect_.bottom) {
            const auto center = D2D1::Point2F(
                static_cast<float>(cursor.x - monitor_rect_.left),
                static_cast<float>(cursor.y - monitor_rect_.top));
            ComPtr<ID2D1SolidColorBrush> halo;
            ComPtr<ID2D1SolidColorBrush> ring;
            context->CreateSolidColorBrush(D2D1::ColorF(0xFFD329, 0.22F), halo.GetAddressOf());
            context->CreateSolidColorBrush(D2D1::ColorF(0xFFB000, 0.88F), ring.GetAddressOf());
            context->FillEllipse(D2D1::Ellipse(center, 25, 25), halo.Get());
            context->DrawEllipse(D2D1::Ellipse(center, 25, 25), ring.Get(), 3.0F);
        }
    }
    std::wstring error;
    if (!surface_.end_draw(error)) controller_.report_runtime_error(error);
}

bool PaletteWindow::initialize(GraphicsDevice& graphics) {
    const int work_left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int work_top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const RECT bounds{work_left + 28, work_top + 28,
                      work_left + 28 + kPaletteWidth, work_top + 28 + kPaletteHeight};
    constexpr DWORD ex_style = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                               WS_EX_LAYERED;
    if (!create(L"ElitePen.Palette", L"Elite Pen", ex_style, WS_POPUP, bounds)) return false;
    SetLayeredWindowAttributes(window_, 0, 255, LWA_ALPHA);
    if (!initialize_surface(graphics)) return false;
#ifndef ELITE_PEN_DEBUG
    if (controller_.preferences().exclude_palette_from_capture)
        SetWindowDisplayAffinity(window_, WDA_EXCLUDEFROMCAPTURE);
#endif
    ShowWindow(window_, SW_SHOWNOACTIVATE);
    install_tooltips();
    install_hotkeys();
    add_tray_icon();
    SetTimer(window_, 20, 33, nullptr);
    return true;
}

void PaletteWindow::install_tooltips() {
    tooltip_ = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                               WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                               window_, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!tooltip_) return;
    SetWindowPos(tooltip_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SendMessageW(tooltip_, TTM_SETMAXTIPWIDTH, 0, 340);
    struct Tip { UINT id; RECT bounds; const wchar_t* text; };
    constexpr std::array tips{
        Tip{1, {7, 11, 41, 144}, L"Grosor del trazo"},
        Tip{2, {50, 90, 84, 124}, L"Negro"},
        Tip{3, {52, 43, 86, 77}, L"Amarillo"},
        Tip{4, {102, 12, 136, 46}, L"Azul"},
        Tip{5, {152, 36, 186, 70}, L"Rojo"},
        Tip{6, {167, 79, 201, 113}, L"Verde"},
        Tip{7, {143, 113, 177, 147}, L"Morado"},
        Tip{8, {96, 122, 130, 156}, L"Mas colores"},
        Tip{9, {104, 61, 148, 105}, L"Ocultar o mostrar anotaciones"},
        Tip{10, {42, 120, 86, 174}, L"Alternar entre lapiz y cursor normal"},
        Tip{11, {88, 151, 116, 184}, L"Pizarra blanca (clic) o negra (clic derecho)"},
        Tip{12, {102, 159, 230, 239}, L"Abrir herramientas y configuracion"},
        Tip{16, {237, 214, 279, 276}, L"Papelera: limpiar todas las anotaciones"}
    };
    for (const auto& tip : tips) {
        TOOLINFOW information{};
        information.cbSize = sizeof(information);
        information.uFlags = TTF_SUBCLASS | TTF_TRANSPARENT;
        information.hwnd = window_;
        information.uId = tip.id;
        information.rect = palette_scaled_rect(tip.bounds);
        information.lpszText = const_cast<wchar_t*>(tip.text);
        SendMessageW(tooltip_, TTM_ADDTOOLW, 0,
                     reinterpret_cast<LPARAM>(&information));
    }
}

void PaletteWindow::install_hotkeys() {
    RegisterHotKey(window_, kHotkeyInteract, kHotkeyModifiers, 'P');
    RegisterHotKey(window_, kHotkeyVisibility, kHotkeyModifiers, 'H');
    RegisterHotKey(window_, kHotkeyWhiteboard, kHotkeyModifiers, 'W');
    RegisterHotKey(window_, kHotkeyUndo, kHotkeyModifiers, 'Z');
    RegisterHotKey(window_, kHotkeyRedo, kHotkeyModifiers, 'Y');
    RegisterHotKey(window_, kHotkeyClear, kHotkeyModifiers, 'C');
    RegisterHotKey(window_, kHotkeyZoom, kHotkeyModifiers, 'M');
    RegisterHotKey(window_, kHotkeyBlackboard, kHotkeyModifiers, 'B');
}

void PaletteWindow::remove_hotkeys() {
    for (int id = kHotkeyInteract; id <= kHotkeyBlackboard; ++id)
        UnregisterHotKey(window_, id);
}

void PaletteWindow::add_tray_icon() {
    tray_.cbSize = sizeof(tray_);
    tray_.hWnd = window_;
    tray_.uID = static_cast<UINT>(kTrayId);
    tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    tray_.uCallbackMessage = kTrayMessage;
    tray_.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101));
    if (!tray_.hIcon) tray_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    lstrcpynW(tray_.szTip, L"Elite Pen", ARRAYSIZE(tray_.szTip));
    Shell_NotifyIconW(NIM_ADD, &tray_);
    tray_.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &tray_);
}

void PaletteWindow::remove_tray_icon() {
    if (tray_.hWnd) Shell_NotifyIconW(NIM_DELETE, &tray_);
    tray_ = {};
}

void PaletteWindow::show_notification(const wchar_t* title, const std::wstring& message) {
    if (!tray_.hWnd) return;
    tray_.uFlags = NIF_INFO;
    lstrcpynW(tray_.szInfoTitle, title, ARRAYSIZE(tray_.szInfoTitle));
    lstrcpynW(tray_.szInfo, message.c_str(), ARRAYSIZE(tray_.szInfo));
    tray_.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &tray_);
    tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
}

void PaletteWindow::show_tool_menu() {
    controller_.toggle_tool_panel();
}

void PaletteWindow::choose_custom_color() {
    controller_.toggle_color_panel();
}

void PaletteWindow::show_tray_menu() {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, 2001, L"Mostrar paleta");
    AppendMenuW(menu, MF_STRING, 2002, L"Ocultar / mostrar trazos");
    AppendMenuW(menu, MF_STRING, 2003, L"Pizarra blanca");
    AppendMenuW(menu, MF_STRING, 2006, L"Pizarra negra");
    AppendMenuW(menu, MF_STRING, 2004, L"Deshacer");
    AppendMenuW(menu, MF_STRING, 2005, L"Rehacer");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 2099, L"Salir de Elite Pen");
    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(window_);
    const int selected = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                        cursor.x, cursor.y, 0, window_, nullptr);
    DestroyMenu(menu);
    switch (selected) {
        case 2001: ShowWindow(window_, SW_SHOWNOACTIVATE); break;
        case 2002: controller_.toggle_visibility(); break;
        case 2003: controller_.toggle_whiteboard(); break;
        case 2006: controller_.toggle_blackboard(); break;
        case 2004: controller_.undo(); break;
        case 2005: controller_.redo(); break;
        case 2099: controller_.request_exit(); break;
        default: break;
    }
}

bool PaletteWindow::command_at(POINT point) const {
    point = palette_logical_point(point);
    constexpr std::array<float, 5> thickness_y{32.0F, 49.0F, 69.0F, 93.0F, 122.0F};
    for (const float y : thickness_y) {
        if (point_in_circle(point, 23.0F, y, 11.0F)) return true;
    }
    constexpr std::array<POINT, 7> color_points{{
        {67, 107}, {69, 60}, {119, 29}, {169, 53}, {184, 96}, {160, 130}, {113, 139}
    }};
    for (const auto& color_point : color_points) {
        if (point_in_circle(point, static_cast<float>(color_point.x),
                            static_cast<float>(color_point.y), 17.0F)) return true;
    }
    return point_in_circle(point, 126, 83, 23) ||
           point_in_circle(point, 256, 244, 25) ||
           (point.x >= 42 && point.x <= 86 && point.y >= 120 && point.y <= 174) ||
           (point.x >= 88 && point.x <= 116 && point.y >= 151 && point.y <= 184) ||
           point_near_segment(point, 106, 173, 216, 225, 14);
}

bool PaletteWindow::contains_screen_point(POINT point) const {
    if (!IsWindowVisible(window_)) return false;
    RECT bounds{};
    return GetWindowRect(window_, &bounds) && PtInRect(&bounds, point);
}

bool PaletteWindow::activate_command_at(POINT point) {
    if (!command_at(point)) return false;
    activate_at(point);
    return true;
}

void PaletteWindow::activate_at(POINT point) {
    const POINT physical_point = point;
    point = palette_logical_point(point);
    constexpr std::array<float, 5> thicknesses{2.0F, 4.0F, 7.0F, 12.0F, 20.0F};
    constexpr std::array<float, 5> thickness_y{32.0F, 49.0F, 69.0F, 93.0F, 122.0F};
    for (std::size_t index = 0; index < thickness_y.size(); ++index) {
        if (point_in_circle(point, 23.0F, thickness_y[index], 10.0F)) {
            controller_.set_thickness(thicknesses[index]);
            return;
        }
    }
    struct Swatch { float x; float y; Color color; };
    constexpr std::array swatches{
        Swatch{67, 107, kBlack}, Swatch{69, 60, kYellow},
        Swatch{119, 29, kBlue}, Swatch{169, 53, kRed},
        Swatch{184, 96, kGreen}, Swatch{160, 130, kPurple}};
    for (const auto& swatch : swatches) {
        if (point_in_circle(point, swatch.x, swatch.y, 15.0F)) {
            controller_.set_color(swatch.color);
            return;
        }
    }
    if (point_in_circle(point, 113, 139, 16)) { choose_custom_color(); return; }
    if (point_in_circle(point, 126, 83, 22)) { controller_.toggle_visibility(); return; }
    if (point_in_circle(point, 256, 244, 24)) { controller_.clear_document(); return; }
    if (point.x >= 42 && point.x <= 86 && point.y >= 120 && point.y <= 174) {
        controller_.set_tool(controller_.state().tool == Tool::Interact
            ? Tool::Pen : Tool::Interact);
        return;
    }
    if (point.x >= 88 && point.x <= 116 && point.y >= 155 && point.y <= 184) {
        controller_.toggle_whiteboard(); return;
    }
    if (point_near_segment(point, 106, 173, 216, 225, 14)) {
        show_tool_menu(); return;
    }

    dragging_ = true;
    drag_origin_ = physical_point;
    RECT bounds{};
    GetWindowRect(window_, &bounds);
    window_origin_ = {bounds.left, bounds.top};
    SetCapture(window_);
}

LRESULT PaletteWindow::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case kQaQueryToolMessage:
            return static_cast<LRESULT>(controller_.state().tool);
        case kQaQueryColorMessage:
            return static_cast<LRESULT>(controller_.state().color.argb());
        case kQaQueryThicknessMessage:
            return static_cast<LRESULT>(std::lround(controller_.state().thickness * 10.0F));
        case kQaQueryDocumentCountMessage:
            return static_cast<LRESULT>(controller_.state().document.items().size());
        case WM_SETCURSOR:
            if (LOWORD(lparam) == HTCLIENT) {
                POINT point{};
                GetCursorPos(&point);
                ScreenToClient(window_, &point);
                SetCursor(LoadCursorW(nullptr, command_at(point) ? IDC_HAND : IDC_SIZEALL));
                return TRUE;
            }
            break;
        case WM_LBUTTONDOWN:
            activate_at({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
            return 0;
        case WM_MOUSEMOVE:
            if (dragging_ && (wparam & MK_LBUTTON)) {
                POINT current{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                SetWindowPos(window_, HWND_TOPMOST,
                             window_origin_.x + current.x - drag_origin_.x,
                             window_origin_.y + current.y - drag_origin_.y,
                             0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
            }
            return 0;
        case WM_LBUTTONUP:
            dragging_ = false;
            if (GetCapture() == window_) ReleaseCapture();
            controller_.save_palette_position();
            return 0;
        case WM_RBUTTONUP:
            if (const POINT point = palette_logical_point(
                    {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
                point.x >= 88 && point.x <= 116 && point.y >= 155 && point.y <= 184) {
                controller_.toggle_blackboard();
            } else {
                show_tray_menu();
            }
            return 0;
        case WM_HOTKEY:
            switch (static_cast<int>(wparam)) {
                case kHotkeyInteract:
                    controller_.set_tool(controller_.state().tool == Tool::Interact
                        ? Tool::Pen : Tool::Interact); break;
                case kHotkeyVisibility: controller_.toggle_visibility(); break;
                case kHotkeyWhiteboard: controller_.toggle_whiteboard(); break;
                case kHotkeyUndo: controller_.undo(); break;
                case kHotkeyRedo: controller_.redo(); break;
                case kHotkeyClear: controller_.clear_document(); break;
                case kHotkeyZoom: controller_.toggle_zoom(); break;
                case kHotkeyBlackboard: controller_.toggle_blackboard(); break;
                default: break;
            }
            return 0;
        case WM_TIMER:
            if (wparam == 20) {
                const bool down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
                if (down && !escape_down_) controller_.stop_transient_mode();
                escape_down_ = down;
                if (controller_.state().cursor_highlight) controller_.invalidate_document();
                controller_.update_transient_ink();
                return 0;
            }
            break;
        case kTrayMessage:
            if (LOWORD(lparam) == WM_CONTEXTMENU || LOWORD(lparam) == WM_RBUTTONUP) {
                show_tray_menu();
            } else if (LOWORD(lparam) == WM_LBUTTONUP) {
                ShowWindow(window_, IsWindowVisible(window_) ? SW_HIDE : SW_SHOWNOACTIVATE);
            }
            return 0;
        case WM_DISPLAYCHANGE:
            controller_.rebuild_overlays();
            return 0;
        case WM_SETTINGCHANGE:
            invalidate();
            return 0;
        case kExitMessage:
            DestroyWindow(window_);
            return 0;
        case WM_DESTROY:
            KillTimer(window_, 20);
            remove_hotkeys();
            remove_tray_icon();
            PostQuitMessage(0);
            return 0;
        default:
            return WindowBase::handle_message(message, wparam, lparam);
    }
    return WindowBase::handle_message(message, wparam, lparam);
}

void PaletteWindow::render() {
    auto* context = surface_.begin_draw(D2D1::ColorF(0, 0.0F));
    if (!context) return;
    context->SetTransform(D2D1::Matrix3x2F::Scale(kPaletteScale, kPaletteScale));

    ComPtr<ID2D1SolidColorBrush> cream;
    ComPtr<ID2D1SolidColorBrush> shadow;
    ComPtr<ID2D1SolidColorBrush> ink;
    context->CreateSolidColorBrush(D2D1::ColorF(0xF3E3CA), cream.GetAddressOf());
    context->CreateSolidColorBrush(D2D1::ColorF(0, 0.16F), shadow.GetAddressOf());
    context->CreateSolidColorBrush(D2D1::ColorF(0x232326), ink.GetAddressOf());

    context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(127, 83), 83, 73), shadow.Get());
    ComPtr<ID2D1PathGeometry> palette_geometry;
    controller_.graphics().d2d_factory()->CreatePathGeometry(palette_geometry.GetAddressOf());
    ComPtr<ID2D1GeometrySink> sink;
    palette_geometry->Open(sink.GetAddressOf());
    sink->BeginFigure(D2D1::Point2F(59, 28), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(91, 2), D2D1::Point2F(166, 3),
                                        D2D1::Point2F(199, 42)));
    sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(228, 78), D2D1::Point2F(205, 135),
                                        D2D1::Point2F(168, 142)));
    sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(136, 149), D2D1::Point2F(139, 188),
                                        D2D1::Point2F(99, 183)));
    sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(53, 179), D2D1::Point2F(34, 145),
                                        D2D1::Point2F(50, 116)));
    sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(62, 94), D2D1::Point2F(23, 91),
                                        D2D1::Point2F(36, 55)));
    sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(41, 42), D2D1::Point2F(48, 35),
                                        D2D1::Point2F(59, 28)));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    context->FillGeometry(palette_geometry.Get(), cream.Get());

    struct Swatch { float x; float y; Color color; };
    constexpr std::array swatches{
        Swatch{67, 107, kBlack}, Swatch{69, 60, kYellow},
        Swatch{119, 29, kBlue}, Swatch{169, 53, kRed},
        Swatch{184, 96, kGreen}, Swatch{160, 130, kPurple}};
    for (const auto& swatch : swatches) {
        ComPtr<ID2D1SolidColorBrush> color_brush;
        context->CreateSolidColorBrush(d2d_color(swatch.color), color_brush.GetAddressOf());
        if (controller_.state().color == swatch.color) {
            context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(swatch.x, swatch.y), 17, 17),
                                 ink.Get(), 2.5F);
            context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(swatch.x, swatch.y), 15, 15),
                                 cream.Get(), 1.5F);
        }
        context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(swatch.x, swatch.y), 12, 12),
                             color_brush.Get());
    }
    // The advanced selector is intentionally just a clean plus sign after purple.
    context->DrawLine(D2D1::Point2F(106, 139), D2D1::Point2F(120, 139), ink.Get(), 2.2F);
    context->DrawLine(D2D1::Point2F(113, 132), D2D1::Point2F(113, 146), ink.Get(), 2.2F);

    const auto thickness_shadow = D2D1::RoundedRect(D2D1::RectF(5, 13, 41, 144), 17, 17);
    context->FillRoundedRectangle(thickness_shadow, shadow.Get());
    const auto thickness_panel = D2D1::RoundedRect(D2D1::RectF(7, 11, 39, 142), 16, 16);
    context->FillRoundedRectangle(thickness_panel, cream.Get());
    context->DrawRoundedRectangle(thickness_panel, ink.Get(), 1.0F);

    constexpr std::array<float, 5> thicknesses{2, 4, 7, 12, 20};
    constexpr std::array<float, 5> ys{32, 49, 69, 93, 122};
    for (std::size_t index = 0; index < ys.size(); ++index) {
        const float radius = 1.7F + static_cast<float>(index) * 1.45F;
        if (std::abs(controller_.state().thickness - thicknesses[index]) < 0.1F) {
            context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(23, ys[index]),
                                                radius + 4, radius + 4), ink.Get(), 1.5F);
        }
        context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(23, ys[index]), radius, radius),
                             ink.Get());
    }

    // Eye: open means visible; closed means annotations are preserved but hidden.
    if (controller_.state().annotations_visible) {
        ComPtr<ID2D1PathGeometry> eye;
        controller_.graphics().d2d_factory()->CreatePathGeometry(eye.GetAddressOf());
        ComPtr<ID2D1GeometrySink> eye_sink;
        eye->Open(eye_sink.GetAddressOf());
        eye_sink->BeginFigure(D2D1::Point2F(108, 83), D2D1_FIGURE_BEGIN_HOLLOW);
        eye_sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(118, 72), D2D1::Point2F(135, 72),
                                                D2D1::Point2F(145, 83)));
        eye_sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(135, 94), D2D1::Point2F(118, 94),
                                                D2D1::Point2F(108, 83)));
        eye_sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        eye_sink->Close();
        context->DrawGeometry(eye.Get(), ink.Get(), 1.7F);
        context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(126.5F, 83), 4.5F, 6.5F), ink.Get());
    } else {
        ComPtr<ID2D1PathGeometry> closed_eye;
        controller_.graphics().d2d_factory()->CreatePathGeometry(closed_eye.GetAddressOf());
        ComPtr<ID2D1GeometrySink> closed_eye_sink;
        closed_eye->Open(closed_eye_sink.GetAddressOf());
        closed_eye_sink->BeginFigure(D2D1::Point2F(108, 81), D2D1_FIGURE_BEGIN_HOLLOW);
        closed_eye_sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(118, 92),
                                                       D2D1::Point2F(136, 92),
                                                       D2D1::Point2F(145, 81)));
        closed_eye_sink->EndFigure(D2D1_FIGURE_END_OPEN);
        closed_eye_sink->Close();
        context->DrawGeometry(closed_eye.Get(), ink.Get(), 2.1F);
        context->DrawLine(D2D1::Point2F(115, 87), D2D1::Point2F(111, 92), ink.Get(), 1.6F);
        context->DrawLine(D2D1::Point2F(126.5F, 90), D2D1::Point2F(126.5F, 96), ink.Get(), 1.6F);
        context->DrawLine(D2D1::Point2F(138, 87), D2D1::Point2F(142, 92), ink.Get(), 1.6F);
    }

    // Functional brush: tip mode, white ferrule/board and a clean tool-menu handle.
    ComPtr<ID2D1SolidColorBrush> bristle;
    ComPtr<ID2D1SolidColorBrush> red;
    ComPtr<ID2D1SolidColorBrush> ferrule;
    ComPtr<ID2D1SolidColorBrush> handle;
    context->CreateSolidColorBrush(D2D1::ColorF(0x574060), bristle.GetAddressOf());
    context->CreateSolidColorBrush(D2D1::ColorF(0xFF6868), red.GetAddressOf());
    context->CreateSolidColorBrush(D2D1::ColorF(0xF7F7F4), ferrule.GetAddressOf());
    context->CreateSolidColorBrush(D2D1::ColorF(0x27A9D2), handle.GetAddressOf());
    context->DrawLine(D2D1::Point2F(106, 177), D2D1::Point2F(216, 229),
                      shadow.Get(), 20.0F);
    context->DrawLine(D2D1::Point2F(106, 173), D2D1::Point2F(216, 225),
                      handle.Get(), 17.0F);
    D2D1_POINT_2F ferrule_points[]{{82, 151}, {110, 164}, {102, 182}, {75, 169}};
    ComPtr<ID2D1PathGeometry> ferrule_geometry;
    controller_.graphics().d2d_factory()->CreatePathGeometry(ferrule_geometry.GetAddressOf());
    ComPtr<ID2D1GeometrySink> ferrule_sink;
    ferrule_geometry->Open(ferrule_sink.GetAddressOf());
    ferrule_sink->BeginFigure(ferrule_points[0], D2D1_FIGURE_BEGIN_FILLED);
    ferrule_sink->AddLines(ferrule_points + 1, 3);
    ferrule_sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    ferrule_sink->Close();
    context->FillGeometry(ferrule_geometry.Get(), ferrule.Get());
    context->DrawGeometry(ferrule_geometry.Get(), ink.Get(), 1.0F);

    ComPtr<ID2D1PathGeometry> bristle_geometry;
    controller_.graphics().d2d_factory()->CreatePathGeometry(bristle_geometry.GetAddressOf());
    ComPtr<ID2D1GeometrySink> bristle_sink;
    bristle_geometry->Open(bristle_sink.GetAddressOf());
    bristle_sink->BeginFigure(D2D1::Point2F(76, 169), D2D1_FIGURE_BEGIN_FILLED);
    bristle_sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(61, 169), D2D1::Point2F(49, 158),
                                                D2D1::Point2F(48, 137)));
    bristle_sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(57, 149), D2D1::Point2F(80, 141),
                                                D2D1::Point2F(84, 153)));
    bristle_sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(87, 160), D2D1::Point2F(82, 166),
                                                D2D1::Point2F(76, 169)));
    bristle_sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    bristle_sink->Close();
    context->FillGeometry(bristle_geometry.Get(), bristle.Get());
    context->DrawLine(D2D1::Point2F(49, 137), D2D1::Point2F(50, 123), red.Get(), 7.0F);

    // Pointer/pen toggle lives in the brush tip.
    if (controller_.state().tool == Tool::Interact) {
        context->DrawLine(D2D1::Point2F(63, 149), D2D1::Point2F(74, 162),
                          ferrule.Get(), 2.0F);
        context->DrawLine(D2D1::Point2F(63, 149), D2D1::Point2F(65, 165),
                          ferrule.Get(), 2.0F);
    } else {
        context->DrawLine(D2D1::Point2F(61, 162), D2D1::Point2F(75, 149),
                          ferrule.Get(), 2.3F);
        context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(61, 162), 2.3F, 2.3F),
                             ferrule.Get());
    }

    // A familiar trash can makes the destructive action immediately recognizable.
    context->DrawRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(244, 233, 268, 263), 3, 3), handle.Get(), 2.2F);
    context->DrawLine(D2D1::Point2F(241, 229), D2D1::Point2F(271, 229),
                      handle.Get(), 2.6F);
    context->DrawLine(D2D1::Point2F(251, 225), D2D1::Point2F(261, 225),
                      handle.Get(), 2.6F);
    context->DrawLine(D2D1::Point2F(250, 239), D2D1::Point2F(250, 257),
                      handle.Get(), 1.7F);
    context->DrawLine(D2D1::Point2F(256, 239), D2D1::Point2F(256, 257),
                      handle.Get(), 1.7F);
    context->DrawLine(D2D1::Point2F(262, 239), D2D1::Point2F(262, 257),
                      handle.Get(), 1.7F);

    std::wstring error;
    if (!surface_.end_draw(error)) controller_.report_runtime_error(error);
}

bool ColorWindow::initialize(GraphicsDevice& graphics) {
    const RECT bounds{0, 0, 344, 330};
    constexpr DWORD ex_style = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                               WS_EX_LAYERED;
    if (!create(L"ElitePen.Colors", L"Colores — Elite Pen", ex_style, WS_POPUP,
                bounds)) return false;
    SetLayeredWindowAttributes(window_, 0, 255, LWA_ALPHA);
    if (!initialize_surface(graphics)) return false;
#ifndef ELITE_PEN_DEBUG
    if (controller_.preferences().exclude_palette_from_capture)
        SetWindowDisplayAffinity(window_, WDA_EXCLUDEFROMCAPTURE);
#endif
    return true;
}

void ColorWindow::toggle_near(HWND anchor) {
    RECT anchor_rect{};
    GetWindowRect(anchor, &anchor_rect);
    RECT desired{anchor_rect.right + 10, anchor_rect.top,
                 anchor_rect.right + 10 + 344, anchor_rect.top + 330};
    HMONITOR monitor = MonitorFromRect(&anchor_rect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    if (desired.right > info.rcWork.right) {
        desired.left = anchor_rect.left - 354;
        desired.right = desired.left + 344;
    }
    desired.top = std::clamp(static_cast<int>(desired.top),
                             static_cast<int>(info.rcWork.top),
                             static_cast<int>(info.rcWork.bottom) - 330);
    SetWindowPos(window_, HWND_TOPMOST, desired.left, desired.top, 344, 330,
                 SWP_SHOWWINDOW | SWP_NOACTIVATE);
    invalidate();
}

void ColorWindow::choose_system_color() {
    static COLORREF custom_colors[16]{};
    CHOOSECOLORW chooser{};
    chooser.lStructSize = sizeof(chooser);
    chooser.hwndOwner = window_;
    chooser.rgbResult = RGB(controller_.state().color.r, controller_.state().color.g,
                            controller_.state().color.b);
    chooser.lpCustColors = custom_colors;
    chooser.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ANYCOLOR;
    SetForegroundWindow(window_);
    if (ChooseColorW(&chooser)) {
        controller_.set_color({GetRValue(chooser.rgbResult), GetGValue(chooser.rgbResult),
                               GetBValue(chooser.rgbResult), 255});
        hide();
    }
}

LRESULT ColorWindow::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_LBUTTONDOWN: {
            const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            for (std::size_t index = 0; index < kExtendedColors.size(); ++index) {
                const int column = static_cast<int>(index % 7);
                const int row = static_cast<int>(index / 7);
                if (point_in_circle(point, 31.0F + static_cast<float>(column) * 47.0F,
                                    57.0F + static_cast<float>(row) * 36.0F, 16.0F)) {
                    controller_.set_color(kExtendedColors[index]);
                    hide();
                    return 0;
                }
            }
            if (point.x >= 18 && point.x <= 326 && point.y >= 283 && point.y <= 316) {
                choose_system_color();
            }
            return 0;
        }
        case WM_RBUTTONDOWN:
            hide();
            return 0;
        default:
            return WindowBase::handle_message(message, wparam, lparam);
    }
}

void ColorWindow::render() {
    auto* context = surface_.begin_draw(D2D1::ColorF(0, 0.0F));
    if (!context) return;
    ComPtr<ID2D1SolidColorBrush> panel;
    ComPtr<ID2D1SolidColorBrush> border;
    ComPtr<ID2D1SolidColorBrush> text;
    ComPtr<ID2D1SolidColorBrush> selected;
    context->CreateSolidColorBrush(D2D1::ColorF(0x202126, 0.98F), panel.GetAddressOf());
    context->CreateSolidColorBrush(D2D1::ColorF(0x4B4D56), border.GetAddressOf());
    context->CreateSolidColorBrush(D2D1::ColorF(0xF8FAFC), text.GetAddressOf());
    context->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF), selected.GetAddressOf());
    const auto panel_rect = D2D1::RoundedRect(D2D1::RectF(2, 2, 342, 328), 18, 18);
    context->FillRoundedRectangle(panel_rect, panel.Get());
    context->DrawRoundedRectangle(panel_rect, border.Get(), 1.0F);

    ComPtr<IDWriteTextFormat> title_format;
    controller_.graphics().dwrite()->CreateTextFormat(L"Segoe UI", nullptr,
        DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        14.0F, L"es-CO", title_format.GetAddressOf());
    if (title_format) {
        context->DrawTextW(L"ELIGE UN COLOR", 14, title_format.Get(),
                           D2D1::RectF(18, 15, 230, 38), text.Get());
    }
    for (std::size_t index = 0; index < kExtendedColors.size(); ++index) {
        const float x = 31.0F + static_cast<float>(index % 7) * 47.0F;
        const float y = 57.0F + static_cast<float>(index / 7) * 36.0F;
        ComPtr<ID2D1SolidColorBrush> color;
        context->CreateSolidColorBrush(d2d_color(kExtendedColors[index]), color.GetAddressOf());
        if (controller_.state().color == kExtendedColors[index]) {
            context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), 16, 16),
                                 selected.Get(), 2.5F);
        }
        context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), 12.5F, 12.5F), color.Get());
        if (kExtendedColors[index] == Color{255, 255, 255, 255}) {
            context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), 12.5F, 12.5F),
                                 border.Get(), 1.0F);
        }
    }
    const auto custom = D2D1::RoundedRect(D2D1::RectF(18, 283, 326, 316), 9, 9);
    context->DrawRoundedRectangle(custom, border.Get(), 1.4F);
    if (title_format) {
        context->DrawTextW(L"Color personalizado…", 20, title_format.Get(),
                           D2D1::RectF(38, 290, 300, 313), text.Get());
    }
    std::wstring error;
    if (!surface_.end_draw(error)) controller_.report_runtime_error(error);
}

bool ToolWindow::initialize(GraphicsDevice& graphics) {
    const RECT bounds{0, 0, 366, 400};
    constexpr DWORD ex_style = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
                               WS_EX_LAYERED;
    if (!create(L"ElitePen.Tools", L"Herramientas — Elite Pen", ex_style, WS_POPUP,
                bounds)) return false;
    SetLayeredWindowAttributes(window_, 0, 255, LWA_ALPHA);
    if (!initialize_surface(graphics)) return false;
#ifndef ELITE_PEN_DEBUG
    if (controller_.preferences().exclude_palette_from_capture)
        SetWindowDisplayAffinity(window_, WDA_EXCLUDEFROMCAPTURE);
#endif
    return true;
}

void ToolWindow::toggle_near(HWND anchor) {
    show_near(anchor, false);
}

void ToolWindow::toggle_geometry_near(HWND anchor) {
    show_near(anchor, true);
}

std::size_t ToolWindow::tool_count() const noexcept {
    return geometry_only_ ? kGeometryTools.size() : kTools.size();
}

Tool ToolWindow::tool_at(std::size_t index) const noexcept {
    return geometry_only_ ? kGeometryTools[index] : kTools[index];
}

void ToolWindow::show_near(HWND anchor, bool geometry_only) {
    geometry_only_ = geometry_only;
    const int panel_height = geometry_only_ ? 112 : 400;
    RECT anchor_rect{};
    GetWindowRect(anchor, &anchor_rect);
    RECT desired{anchor_rect.right + 10, anchor_rect.top,
                 anchor_rect.right + 376, anchor_rect.top + panel_height};
    HMONITOR monitor = MonitorFromRect(&anchor_rect, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    if (desired.right > info.rcWork.right) {
        desired.left = anchor_rect.left - 376;
        desired.right = desired.left + 366;
    }
    desired.top = std::clamp(static_cast<int>(desired.top),
                             static_cast<int>(info.rcWork.top),
                             static_cast<int>(info.rcWork.bottom) - panel_height);
    SetWindowPos(window_, HWND_TOPMOST, desired.left, desired.top, 366, panel_height,
                 SWP_SHOWWINDOW | SWP_NOACTIVATE);
    invalidate();
}

LRESULT ToolWindow::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_LBUTTONDOWN) {
        const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        for (std::size_t index = 0; index < tool_count(); ++index) {
            RECT item{};
            if (geometry_only_) {
                const int left = 14 + static_cast<int>(index) * 68;
                item = {left, 43, left + 58, 95};
            } else {
                const int column = static_cast<int>(index % 2);
                const int row = static_cast<int>(index / 2);
                item = {15 + column * 169, 48 + row * 48,
                        15 + column * 169 + 162, 48 + row * 48 + 40};
            }
            if (PtInRect(&item, point)) {
                const Tool tool = tool_at(index);
                hide();
                controller_.set_tool(tool);
                return 0;
            }
        }
        if (!geometry_only_) {
            constexpr RECT settings_item{15, 340, 351, 382};
            if (PtInRect(&settings_item, point)) {
                hide();
                controller_.show_settings_window();
                return 0;
            }
        }
        return 0;
    }
    if (message == WM_RBUTTONDOWN) { hide(); return 0; }
    return WindowBase::handle_message(message, wparam, lparam);
}

void ToolWindow::render() {
    auto* context = surface_.begin_draw(D2D1::ColorF(0, 0.0F));
    if (!context) return;
    ComPtr<ID2D1SolidColorBrush> panel;
    ComPtr<ID2D1SolidColorBrush> border;
    ComPtr<ID2D1SolidColorBrush> text;
    ComPtr<ID2D1SolidColorBrush> accent;
    ComPtr<ID2D1SolidColorBrush> hover;
    context->CreateSolidColorBrush(D2D1::ColorF(0x202126, 0.98F), panel.GetAddressOf());
    context->CreateSolidColorBrush(D2D1::ColorF(0x4B4D56), border.GetAddressOf());
    context->CreateSolidColorBrush(D2D1::ColorF(0xF8FAFC), text.GetAddressOf());
    context->CreateSolidColorBrush(d2d_color(controller_.state().color), accent.GetAddressOf());
    context->CreateSolidColorBrush(D2D1::ColorF(0x34363E), hover.GetAddressOf());
    const float panel_bottom = static_cast<float>(surface_.height()) - 2.0F;
    const auto background = D2D1::RoundedRect(
        D2D1::RectF(2, 2, 364, panel_bottom), 18, 18);
    context->FillRoundedRectangle(background, panel.Get());
    context->DrawRoundedRectangle(background, border.Get(), 1.0F);

    ComPtr<IDWriteTextFormat> title_format;
    ComPtr<IDWriteTextFormat> item_format;
    controller_.graphics().dwrite()->CreateTextFormat(L"Segoe UI", nullptr,
        DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        14.0F, L"es-CO", title_format.GetAddressOf());
    controller_.graphics().dwrite()->CreateTextFormat(L"Segoe UI", nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        13.0F, L"es-CO", item_format.GetAddressOf());
    if (title_format) {
        const wchar_t* title = geometry_only_ ? L"FIGURAS" : L"HERRAMIENTAS";
        context->DrawTextW(title, static_cast<UINT32>(wcslen(title)), title_format.Get(),
                           D2D1::RectF(17, 14, 220, 38), text.Get());
    }
    for (std::size_t index = 0; index < tool_count(); ++index) {
        const Tool panel_tool = tool_at(index);
        const float left = geometry_only_
            ? 14.0F + static_cast<float>(index) * 68.0F
            : 15.0F + static_cast<float>(index % 2) * 169.0F;
        const float top = geometry_only_
            ? 43.0F
            : 48.0F + static_cast<float>(index / 2) * 48.0F;
        const float item_width = geometry_only_ ? 58.0F : 162.0F;
        const float item_height = geometry_only_ ? 52.0F : 40.0F;
        const auto item = D2D1::RoundedRect(
            D2D1::RectF(left, top, left + item_width, top + item_height), 9, 9);
        const bool active = controller_.state().tool == panel_tool;
        if (active) context->FillRoundedRectangle(item, hover.Get());
        context->DrawRoundedRectangle(item, active ? accent.Get() : border.Get(),
                                      active ? 2.0F : 1.0F);
        const float icon_x = left + (geometry_only_ ? item_width * 0.5F : 20.0F);
        const float icon_y = top + item_height * 0.5F;
        switch (panel_tool) {
            case Tool::Interact:
                context->DrawLine(D2D1::Point2F(icon_x - 6, icon_y - 9),
                                  D2D1::Point2F(icon_x + 6, icon_y + 8), text.Get(), 2.0F);
                context->DrawLine(D2D1::Point2F(icon_x - 6, icon_y - 9),
                                  D2D1::Point2F(icon_x - 3, icon_y + 10), text.Get(), 2.0F);
                break;
            case Tool::Pen:
                context->DrawLine(D2D1::Point2F(icon_x - 7, icon_y + 7),
                                  D2D1::Point2F(icon_x + 7, icon_y - 7), text.Get(), 2.2F);
                context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(icon_x - 8, icon_y + 8),
                                                   1.8F, 1.8F), text.Get());
                break;
            case Tool::Highlighter:
                context->DrawLine(D2D1::Point2F(icon_x - 7, icon_y + 6),
                                  D2D1::Point2F(icon_x + 7, icon_y - 6), text.Get(), 5.0F);
                break;
            case Tool::Eraser:
                context->DrawRoundedRectangle(D2D1::RoundedRect(
                    D2D1::RectF(icon_x - 8, icon_y - 6, icon_x + 8, icon_y + 6), 3, 3),
                    text.Get(), 2.0F);
                break;
            case Tool::Rectangle:
                context->DrawRectangle(D2D1::RectF(icon_x - 8, icon_y - 7,
                                                   icon_x + 8, icon_y + 7), text.Get(), 1.8F);
                break;
            case Tool::Ellipse:
                context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(icon_x, icon_y), 9, 7),
                                     text.Get(), 1.8F);
                break;
            case Tool::Line:
                context->DrawLine(D2D1::Point2F(icon_x - 8, icon_y + 7),
                                  D2D1::Point2F(icon_x + 8, icon_y - 7), text.Get(), 2.0F);
                break;
            case Tool::Arrow:
                context->DrawLine(D2D1::Point2F(icon_x - 8, icon_y + 5),
                                  D2D1::Point2F(icon_x + 8, icon_y - 5), text.Get(), 2.0F);
                context->DrawLine(D2D1::Point2F(icon_x + 8, icon_y - 5),
                                  D2D1::Point2F(icon_x + 2, icon_y - 5), text.Get(), 2.0F);
                break;
            case Tool::CurvedArrow: {
                ComPtr<ID2D1PathGeometry> curve;
                controller_.graphics().d2d_factory()->CreatePathGeometry(curve.GetAddressOf());
                ComPtr<ID2D1GeometrySink> curve_sink;
                curve->Open(curve_sink.GetAddressOf());
                curve_sink->BeginFigure(D2D1::Point2F(icon_x - 8, icon_y + 5),
                                        D2D1_FIGURE_BEGIN_HOLLOW);
                curve_sink->AddBezier(D2D1::BezierSegment(
                    D2D1::Point2F(icon_x - 3, icon_y - 8),
                    D2D1::Point2F(icon_x + 5, icon_y - 8),
                    D2D1::Point2F(icon_x + 8, icon_y - 2)));
                curve_sink->EndFigure(D2D1_FIGURE_END_OPEN);
                curve_sink->Close();
                context->DrawGeometry(curve.Get(), text.Get(), 2.0F);
                draw_arrow_head(context, text.Get(), {icon_x + 5, icon_y - 8},
                                {icon_x + 8, icon_y - 2}, 2.0F);
                break;
            }
            case Tool::Text:
                if (item_format) context->DrawTextW(L"T", 1, item_format.Get(),
                    D2D1::RectF(icon_x - 5, icon_y - 10, icon_x + 8, icon_y + 10), text.Get());
                break;
            case Tool::Screenshot:
                context->DrawRectangle(D2D1::RectF(icon_x - 9, icon_y - 7,
                                                   icon_x + 9, icon_y + 7), text.Get(), 1.7F);
                context->DrawLine(D2D1::Point2F(icon_x - 5, icon_y + 3),
                                  D2D1::Point2F(icon_x - 1, icon_y - 1), text.Get(), 1.5F);
                context->DrawLine(D2D1::Point2F(icon_x - 1, icon_y - 1),
                                  D2D1::Point2F(icon_x + 6, icon_y + 5), text.Get(), 1.5F);
                break;
            case Tool::Zoom:
                context->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(icon_x - 2, icon_y - 2),
                                                   6, 6), text.Get(), 2.0F);
                context->DrawLine(D2D1::Point2F(icon_x + 3, icon_y + 3),
                                  D2D1::Point2F(icon_x + 9, icon_y + 9), text.Get(), 2.4F);
                break;
            default:
                context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(icon_x, icon_y),
                                                   panel_tool == Tool::Eraser ? 6.0F : 3.0F,
                                                   panel_tool == Tool::Eraser ? 6.0F : 3.0F),
                                     text.Get());
                break;
        }
        if (item_format && !geometry_only_) {
            const wchar_t* name = tool_name(panel_tool);
            context->DrawTextW(name, static_cast<UINT32>(wcslen(name)), item_format.Get(),
                               D2D1::RectF(left + 40, top + 11, left + 155, top + 35),
                               text.Get());
        }
    }
    if (!geometry_only_) {
        const auto settings_item = D2D1::RoundedRect(D2D1::RectF(15, 340, 351, 382), 9, 9);
        context->DrawRoundedRectangle(settings_item, border.Get(), 1.0F);
        constexpr D2D1_POINT_2F gear_center{35, 361};
        context->DrawEllipse(D2D1::Ellipse(gear_center, 6.0F, 6.0F), text.Get(), 1.8F);
        context->FillEllipse(D2D1::Ellipse(gear_center, 2.0F, 2.0F), text.Get());
        for (int index = 0; index < 8; ++index) {
            const float angle = static_cast<float>(index) * 0.785398F;
            context->DrawLine(
                D2D1::Point2F(gear_center.x + std::cos(angle) * 7.0F,
                               gear_center.y + std::sin(angle) * 7.0F),
                D2D1::Point2F(gear_center.x + std::cos(angle) * 9.0F,
                               gear_center.y + std::sin(angle) * 9.0F), text.Get(), 1.6F);
        }
        if (item_format) {
            context->DrawTextW(L"Configuracion", 13, item_format.Get(),
                               D2D1::RectF(55, 350, 220, 376), text.Get());
        }
    }
    std::wstring error;
    if (!surface_.end_draw(error)) controller_.report_runtime_error(error);
}

bool TextInputWindow::initialize(GraphicsDevice& graphics) {
    const RECT bounds{0, 0, 640, 420};
    constexpr DWORD ex_style = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED;
    if (!create(L"ElitePen.TextInput", L"Insertar texto — Elite Pen",
                ex_style, WS_POPUP, bounds)) return false;
    SetLayeredWindowAttributes(window_, 0, 255, LWA_ALPHA);
    if (!initialize_surface(graphics)) return false;
#ifndef ELITE_PEN_DEBUG
    if (controller_.preferences().exclude_palette_from_capture)
        SetWindowDisplayAffinity(window_, WDA_EXCLUDEFROMCAPTURE);
#endif
    return true;
}

void TextInputWindow::show_at(PointF position, Color color, float thickness) {
    if (active_) commit();
    position_ = position;
    color_ = color;
    thickness_ = thickness;
    text_.clear();
    active_ = true;
    caret_visible_ = true;

    POINT requested{static_cast<LONG>(std::lround(position.x)),
                    static_cast<LONG>(std::lround(position.y))};
    HMONITOR monitor = MonitorFromPoint(requested, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    const int x = std::clamp(requested.x, info.rcWork.left, info.rcWork.right - 40);
    const int y = std::clamp(requested.y, info.rcWork.top, info.rcWork.bottom - 40);
    const int width = std::max(40, std::min(640, static_cast<int>(info.rcWork.right) - x));
    const int height = std::max(40, std::min(420, static_cast<int>(info.rcWork.bottom) - y));
    SetWindowPos(window_, HWND_TOPMOST, x, y, width, height, SWP_SHOWWINDOW);
    SetForegroundWindow(window_);
    SetFocus(window_);
    SetTimer(window_, 31, 500, nullptr);
    invalidate();
}

void TextInputWindow::update_style(Color color, float thickness) {
    color_ = color;
    thickness_ = thickness;
    if (active_) invalidate();
}

void TextInputWindow::commit() {
    if (!active_) return;
    active_ = false;
    KillTimer(window_, 31);
    ShowWindow(window_, SW_HIDE);
    std::wstring text = std::move(text_);
    text_.clear();
    if (text.find_first_not_of(L" \t\r\n") != std::wstring::npos) {
        controller_.commit_text(position_, color_, thickness_, std::move(text));
    }
}

void TextInputWindow::cancel() {
    if (!active_) return;
    active_ = false;
    text_.clear();
    KillTimer(window_, 31);
    ShowWindow(window_, SW_HIDE);
}

void TextInputWindow::render() {
    auto* context = surface_.begin_draw(D2D1::ColorF(0, 0.0F));
    if (!context) return;
    if (active_) {
        const float font_size = std::clamp(thickness_ * 3.4F, 16.0F, 72.0F);
        ComPtr<ID2D1SolidColorBrush> brush;
        context->CreateSolidColorBrush(d2d_color(color_), brush.GetAddressOf());
        ComPtr<IDWriteTextFormat> format;
        controller_.graphics().dwrite()->CreateTextFormat(
            L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, font_size, L"es-CO", format.GetAddressOf());
        RECT client{};
        GetClientRect(window_, &client);
        ComPtr<IDWriteTextLayout> layout;
        if (format) {
            controller_.graphics().dwrite()->CreateTextLayout(
                text_.c_str(), static_cast<UINT32>(text_.size()), format.Get(),
                static_cast<float>(std::clamp(static_cast<int>(client.right), 1, 600)),
                static_cast<float>(std::clamp(static_cast<int>(client.bottom), 1, 400)),
                layout.GetAddressOf());
        }
        if (layout && brush) {
            context->DrawTextLayout(D2D1::Point2F(0, 0), layout.Get(), brush.Get());
            if (caret_visible_) {
                FLOAT caret_x = 0;
                FLOAT caret_y = 0;
                DWRITE_HIT_TEST_METRICS metrics{};
                layout->HitTestTextPosition(static_cast<UINT32>(text_.size()), FALSE,
                                            &caret_x, &caret_y, &metrics);
                const float caret_height = metrics.height > 0 ? metrics.height : font_size * 1.2F;
                context->DrawLine(D2D1::Point2F(caret_x, caret_y),
                                  D2D1::Point2F(caret_x, caret_y + caret_height),
                                  brush.Get(), 1.7F);
            }
        }
    }
    std::wstring error;
    if (!surface_.end_draw(error)) controller_.report_runtime_error(error);
}

LRESULT TextInputWindow::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_NCHITTEST:
            return HTTRANSPARENT;
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) { cancel(); return 0; }
            if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 && wparam == 'V') {
                SendMessageW(window_, WM_PASTE, 0, 0);
                return 0;
            }
            break;
        case WM_CHAR:
            if (!active_) return 0;
            if (wparam == L'\b') {
                if (!text_.empty()) {
                    text_.pop_back();
                    if (!text_.empty() && text_.back() >= 0xD800 && text_.back() <= 0xDBFF) {
                        text_.pop_back();
                    }
                }
            } else if (wparam == L'\r') {
                if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) commit();
                else text_.push_back(L'\n');
            } else if (wparam == L'\t') {
                text_.append(4, L' ');
            } else if (wparam >= 0x20 && wparam != 0x7F) {
                text_.push_back(static_cast<wchar_t>(wparam));
            }
            caret_visible_ = true;
            invalidate();
            return 0;
        case WM_PASTE:
            if (active_ && OpenClipboard(window_)) {
                if (HANDLE data = GetClipboardData(CF_UNICODETEXT)) {
                    if (const auto* value = static_cast<const wchar_t*>(GlobalLock(data))) {
                        text_.append(value);
                        GlobalUnlock(data);
                    }
                }
                CloseClipboard();
                caret_visible_ = true;
                invalidate();
            }
            return 0;
        case WM_TIMER:
            if (wparam == 31 && active_) {
                caret_visible_ = !caret_visible_;
                invalidate();
                return 0;
            }
            break;
        case kQaCommitInlineTextMessage:
            commit();
            return 0;
        case WM_CLOSE:
            cancel();
            return 0;
        default: break;
    }
    return WindowBase::handle_message(message, wparam, lparam);
}

bool SettingsWindow::initialize() {
    const RECT bounds{0, 0, 590, 575};
    if (!create(L"ElitePen.Settings", L"Configuracion — Elite Pen",
                WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, bounds)) return false;
    HFONT regular = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    title_ = CreateWindowW(L"STATIC", L"Elite Pen 1.5.0", WS_CHILD | WS_VISIBLE,
                           24, 20, 510, 24, window_, nullptr,
                           GetModuleHandleW(nullptr), nullptr);
    capture_ = CreateWindowW(L"BUTTON", L"Ocultar la paleta en capturas de pantalla",
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                             24, 68, 500, 25, window_, reinterpret_cast<HMENU>(4001),
                             GetModuleHandleW(nullptr), nullptr);
    confirm_clear_ = CreateWindowW(L"BUTTON", L"Pedir confirmacion antes de limpiar",
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                   24, 103, 500, 25, window_, reinterpret_cast<HMENU>(4002),
                                   GetModuleHandleW(nullptr), nullptr);
    start_interact_ = CreateWindowW(L"BUTTON", L"Iniciar en modo cursor normal",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                    24, 138, 500, 25, window_, reinterpret_cast<HMENU>(4003),
                                    GetModuleHandleW(nullptr), nullptr);
    highlight_cursor_ = CreateWindowW(L"BUTTON", L"Resaltar la posicion del cursor",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                      24, 173, 270, 25, window_, reinterpret_cast<HMENU>(4008),
                                      GetModuleHandleW(nullptr), nullptr);
    fade_label_ = CreateWindowW(L"STATIC", L"Tinta temporal:", WS_CHILD | WS_VISIBLE,
                                316, 176, 112, 22, window_, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
    fade_ = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                          CBS_DROPDOWNLIST, 430, 171, 128, 130, window_,
                          reinterpret_cast<HMENU>(4009), GetModuleHandleW(nullptr), nullptr);
    for (const wchar_t* value : {L"Permanente", L"3 segundos", L"8 segundos", L"15 segundos"}) {
        SendMessageW(fade_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
    }
    thickness_label_ = CreateWindowW(L"STATIC", L"Grosor inicial:", WS_CHILD | WS_VISIBLE,
                                     24, 220, 145, 24, window_, nullptr,
                                     GetModuleHandleW(nullptr), nullptr);
    thickness_ = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                               CBS_DROPDOWNLIST, 165, 216, 116, 160, window_,
                               reinterpret_cast<HMENU>(4010), GetModuleHandleW(nullptr), nullptr);
    for (const wchar_t* value : {L"2 px", L"4 px", L"7 px", L"12 px", L"20 px"}) {
        SendMessageW(thickness_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
    }
    zoom_label_ = CreateWindowW(L"STATIC", L"Ampliacion:", WS_CHILD | WS_VISIBLE,
                                304, 220, 112, 24, window_, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
    zoom_ = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                          CBS_DROPDOWNLIST, 414, 216, 144, 180, window_,
                          reinterpret_cast<HMENU>(4004), GetModuleHandleW(nullptr), nullptr);
    for (const wchar_t* value : {L"1.5×", L"2×", L"3×", L"4×", L"6×", L"8×"}) {
        SendMessageW(zoom_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
    }
    zoom_view_label_ = CreateWindowW(L"STATIC", L"Vista de ampliacion:",
                                     WS_CHILD | WS_VISIBLE, 24, 263, 170, 24,
                                     window_, nullptr, GetModuleHandleW(nullptr), nullptr);
    zoom_view_ = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                               CBS_DROPDOWNLIST, 195, 258, 210, 150, window_,
                               reinterpret_cast<HMENU>(4006), GetModuleHandleW(nullptr), nullptr);
    for (const wchar_t* value : {L"Pantalla completa", L"Lente", L"Acoplada"}) {
        SendMessageW(zoom_view_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
    }
    zoom_invert_ = CreateWindowW(L"BUTTON", L"Invertir colores durante la ampliacion",
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                 24, 301, 500, 25, window_, reinterpret_cast<HMENU>(4007),
                                 GetModuleHandleW(nullptr), nullptr);
    reset_position_ = CreateWindowW(L"BUTTON", L"Restablecer posicion de la paleta",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                    24, 344, 260, 31, window_, reinterpret_cast<HMENU>(4005),
                                    GetModuleHandleW(nullptr), nullptr);
    shortcuts_ = CreateWindowW(L"STATIC",
        L"Atajos: Ctrl+Shift+P cursor/lapiz · H ocultar · W pizarra\r\n"
        L"Z deshacer · Y rehacer · C limpiar · M zoom · Esc salir del modo\r\n"
        L"En zoom: F completa · L lente · D acoplada · I invertir · 0 vista 1×\r\n"
        L"Pizarra negra: clic derecho en la parte blanca o Ctrl+Shift+B",
        WS_CHILD | WS_VISIBLE, 24, 395, 540, 88, window_, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    close_ = CreateWindowW(L"BUTTON", L"Cerrar", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                           BS_DEFPUSHBUTTON, 455, 498, 100, 32, window_,
                           reinterpret_cast<HMENU>(IDOK), GetModuleHandleW(nullptr), nullptr);
    for (HWND child : {title_, capture_, confirm_clear_, start_interact_, highlight_cursor_,
                       fade_label_, fade_, thickness_label_, thickness_, zoom_label_,
                       zoom_, zoom_view_label_, zoom_view_, zoom_invert_, reset_position_,
                       shortcuts_, close_}) {
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(regular), TRUE);
    }
    return true;
}

void SettingsWindow::refresh_controls() {
    const auto& preferences = controller_.preferences();
    SendMessageW(capture_, BM_SETCHECK,
                 preferences.exclude_palette_from_capture ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(confirm_clear_, BM_SETCHECK,
                 preferences.confirm_clear ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(start_interact_, BM_SETCHECK,
                 preferences.start_in_interact_mode ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(highlight_cursor_, BM_SETCHECK,
                 preferences.highlight_cursor ? BST_CHECKED : BST_UNCHECKED, 0);
    constexpr std::array<int, 4> fade_values{0, 3, 8, 15};
    std::size_t fade_selection = 0;
    for (std::size_t index = 0; index < fade_values.size(); ++index) {
        if (preferences.fade_seconds == fade_values[index]) fade_selection = index;
    }
    SendMessageW(fade_, CB_SETCURSEL, static_cast<WPARAM>(fade_selection), 0);
    constexpr std::array<float, 5> thickness_values{2.0F, 4.0F, 7.0F, 12.0F, 20.0F};
    std::size_t thickness_selection = 0;
    float thickness_difference = 100.0F;
    for (std::size_t index = 0; index < thickness_values.size(); ++index) {
        const float current = std::abs(controller_.state().thickness - thickness_values[index]);
        if (current < thickness_difference) {
            thickness_difference = current;
            thickness_selection = index;
        }
    }
    SendMessageW(thickness_, CB_SETCURSEL, static_cast<WPARAM>(thickness_selection), 0);
    constexpr std::array<float, 6> factors{1.5F, 2.0F, 3.0F, 4.0F, 6.0F, 8.0F};
    std::size_t closest = 0;
    float difference = 100.0F;
    for (std::size_t index = 0; index < factors.size(); ++index) {
        const float current = std::abs(controller_.state().zoom_factor - factors[index]);
        if (current < difference) { difference = current; closest = index; }
    }
    SendMessageW(zoom_, CB_SETCURSEL, closest, 0);
    SendMessageW(zoom_view_, CB_SETCURSEL,
                 static_cast<WPARAM>(std::clamp(preferences.zoom_view, 0, 2)), 0);
    SendMessageW(zoom_invert_, BM_SETCHECK,
                 preferences.zoom_invert ? BST_CHECKED : BST_UNCHECKED, 0);
}

void SettingsWindow::show_settings() {
    refresh_controls();
    POINT cursor{};
    GetCursorPos(&cursor);
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    const int x = info.rcWork.left + (info.rcWork.right - info.rcWork.left - 590) / 2;
    const int y = info.rcWork.top + (info.rcWork.bottom - info.rcWork.top - 575) / 2;
    SetWindowPos(window_, HWND_TOPMOST, x, y, 590, 575, SWP_SHOWWINDOW);
    SetForegroundWindow(window_);
    SetFocus(capture_);
}

LRESULT SettingsWindow::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_COMMAND: {
            const int id = LOWORD(wparam);
            if (id == 4001 && HIWORD(wparam) == BN_CLICKED) {
                controller_.preferences().exclude_palette_from_capture =
                    SendMessageW(capture_, BM_GETCHECK, 0, 0) == BST_CHECKED;
                controller_.apply_capture_preference();
                controller_.save_preferences();
                return 0;
            }
            if (id == 4002 && HIWORD(wparam) == BN_CLICKED) {
                controller_.preferences().confirm_clear =
                    SendMessageW(confirm_clear_, BM_GETCHECK, 0, 0) == BST_CHECKED;
                controller_.save_preferences();
                return 0;
            }
            if (id == 4003 && HIWORD(wparam) == BN_CLICKED) {
                controller_.preferences().start_in_interact_mode =
                    SendMessageW(start_interact_, BM_GETCHECK, 0, 0) == BST_CHECKED;
                controller_.save_preferences();
                return 0;
            }
            if (id == 4008 && HIWORD(wparam) == BN_CLICKED) {
                controller_.preferences().highlight_cursor =
                    SendMessageW(highlight_cursor_, BM_GETCHECK, 0, 0) == BST_CHECKED;
                controller_.state().cursor_highlight =
                    controller_.preferences().highlight_cursor;
                controller_.invalidate_document();
                controller_.save_preferences();
                return 0;
            }
            if (id == 4009 && HIWORD(wparam) == CBN_SELCHANGE) {
                constexpr std::array<int, 4> fade_values{0, 3, 8, 15};
                const LRESULT selection = SendMessageW(fade_, CB_GETCURSEL, 0, 0);
                if (selection >= 0 && static_cast<std::size_t>(selection) < fade_values.size()) {
                    controller_.preferences().fade_seconds =
                        fade_values[static_cast<std::size_t>(selection)];
                    controller_.save_preferences();
                }
                return 0;
            }
            if (id == 4010 && HIWORD(wparam) == CBN_SELCHANGE) {
                constexpr std::array<float, 5> thickness_values{
                    2.0F, 4.0F, 7.0F, 12.0F, 20.0F};
                const LRESULT selection = SendMessageW(thickness_, CB_GETCURSEL, 0, 0);
                if (selection >= 0 &&
                    static_cast<std::size_t>(selection) < thickness_values.size()) {
                    controller_.set_thickness(
                        thickness_values[static_cast<std::size_t>(selection)]);
                    controller_.save_preferences();
                }
                return 0;
            }
            if (id == 4004 && HIWORD(wparam) == CBN_SELCHANGE) {
                constexpr std::array<float, 6> factors{1.5F, 2.0F, 3.0F, 4.0F, 6.0F, 8.0F};
                const LRESULT selection = SendMessageW(zoom_, CB_GETCURSEL, 0, 0);
                if (selection >= 0 && static_cast<std::size_t>(selection) < factors.size()) {
                    controller_.state().zoom_factor = factors[static_cast<std::size_t>(selection)];
                    controller_.preferences().zoom_factor = controller_.state().zoom_factor;
                    controller_.save_preferences();
                }
                return 0;
            }
            if (id == 4005 && HIWORD(wparam) == BN_CLICKED) {
                controller_.reset_palette_position();
                return 0;
            }
            if (id == 4006 && HIWORD(wparam) == CBN_SELCHANGE) {
                const LRESULT selection = SendMessageW(zoom_view_, CB_GETCURSEL, 0, 0);
                if (selection >= 0 && selection <= 2) {
                    controller_.preferences().zoom_view = static_cast<int>(selection);
                    controller_.save_preferences();
                }
                return 0;
            }
            if (id == 4007 && HIWORD(wparam) == BN_CLICKED) {
                controller_.preferences().zoom_invert =
                    SendMessageW(zoom_invert_, BM_GETCHECK, 0, 0) == BST_CHECKED;
                controller_.save_preferences();
                return 0;
            }
            if (id == IDOK) {
                ShowWindow(window_, SW_HIDE);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            ShowWindow(window_, SW_HIDE);
            return 0;
        case WM_ERASEBKGND: {
            RECT client{};
            GetClientRect(window_, &client);
            FillRect(reinterpret_cast<HDC>(wparam), &client, GetSysColorBrush(COLOR_WINDOW));
            return 1;
        }
        default: break;
    }
    return WindowBase::handle_message(message, wparam, lparam);
}

bool ZoomWindow::load_magnification() {
    if (magnification_module_) return true;
    magnification_module_ = LoadLibraryW(L"Magnification.dll");
    if (!magnification_module_) return false;
    mag_initialize_ = reinterpret_cast<MagInitializePointer>(
        GetProcAddress(magnification_module_, "MagInitialize"));
    mag_uninitialize_ = reinterpret_cast<MagUninitializePointer>(
        GetProcAddress(magnification_module_, "MagUninitialize"));
    mag_set_source_ = reinterpret_cast<MagSetWindowSourcePointer>(
        GetProcAddress(magnification_module_, "MagSetWindowSource"));
    mag_set_transform_ = reinterpret_cast<MagSetWindowTransformPointer>(
        GetProcAddress(magnification_module_, "MagSetWindowTransform"));
    mag_set_filter_ = reinterpret_cast<MagSetWindowFilterListPointer>(
        GetProcAddress(magnification_module_, "MagSetWindowFilterList"));
    mag_set_color_effect_ = reinterpret_cast<MagSetColorEffectPointer>(
        GetProcAddress(magnification_module_, "MagSetColorEffect"));
    return mag_initialize_ && mag_uninitialize_ && mag_set_source_ && mag_set_transform_;
}

ZoomWindow::~ZoomWindow() {
    if (initialized_ && mag_uninitialize_) mag_uninitialize_();
    if (magnification_module_) FreeLibrary(magnification_module_);
}

bool ZoomWindow::initialize(GraphicsDevice&) {
    if (!load_magnification() || !mag_initialize_()) return false;
    initialized_ = true;
    RECT initial{0, 0, 1, 1};
    if (!create(L"ElitePen.Zoom", L"Zoom — Elite Pen",
                WS_EX_TOPMOST | WS_EX_TOOLWINDOW, WS_POPUP, initial)) return false;
    magnifier_ = CreateWindowW(L"Magnifier", L"Elite Pen Magnifier",
                               WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, window_, nullptr,
                               GetModuleHandleW(nullptr), nullptr);
    if (!magnifier_) return false;
#ifndef ELITE_PEN_DEBUG
    SetWindowDisplayAffinity(window_, WDA_EXCLUDEFROMCAPTURE);
#endif
    return true;
}

bool ZoomWindow::show_zoom() {
    if (!initialized_ || active_) return initialized_;
    POINT cursor{};
    GetCursorPos(&cursor);
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    monitor_rect_ = info.rcMonitor;
    if (mag_set_filter_) {
        std::vector<HWND> excluded{window_};
        if (controller_.palette()) excluded.push_back(controller_.palette()->hwnd());
        mag_set_filter_(magnifier_, MW_FILTERMODE_EXCLUDE,
                        static_cast<int>(excluded.size()), excluded.data());
    }
    active_ = true;
    overview_ = false;
    SetTimer(window_, 1, 16, nullptr);
    refresh_source();
    apply_color_effect();
    ShowWindow(window_, SW_SHOW);
    SetForegroundWindow(window_);
    SetFocus(window_);
    return true;
}

void ZoomWindow::hide_zoom() {
    if (!active_) return;
    active_ = false;
    KillTimer(window_, 1);
    ShowWindow(window_, SW_HIDE);
}

void ZoomWindow::apply_color_effect() {
    if (!mag_set_color_effect_ || !magnifier_) return;
    MAGCOLOREFFECT effect{};
    if (controller_.preferences().zoom_invert) {
        const MAGCOLOREFFECT inverted{{
            {-1.0F, 0, 0, 0, 0},
            {0, -1.0F, 0, 0, 0},
            {0, 0, -1.0F, 0, 0},
            {0, 0, 0, 1.0F, 0},
            {1.0F, 1.0F, 1.0F, 0, 1.0F}}};
        effect = inverted;
    } else {
        const MAGCOLOREFFECT identity{{
            {1.0F, 0, 0, 0, 0},
            {0, 1.0F, 0, 0, 0},
            {0, 0, 1.0F, 0, 0},
            {0, 0, 0, 1.0F, 0},
            {0, 0, 0, 0, 1.0F}}};
        effect = identity;
    }
    mag_set_color_effect_(magnifier_, &effect);
}

void ZoomWindow::cycle_view() {
    controller_.preferences().zoom_view =
        (controller_.preferences().zoom_view + 1) % 3;
    controller_.save_preferences();
    refresh_source();
}

void ZoomWindow::refresh_source() {
    if (!active_) return;
    POINT cursor{};
    GetCursorPos(&cursor);
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    monitor_rect_ = info.rcMonitor;
    const auto view = static_cast<ZoomView>(controller_.preferences().zoom_view);
    const int monitor_width = monitor_rect_.right - monitor_rect_.left;
    const int monitor_height = monitor_rect_.bottom - monitor_rect_.top;
    RECT zoom_rect = monitor_rect_;
    if (view == ZoomView::Lens) {
        const int lens_width = std::min(640, std::max(320, monitor_width / 2));
        const int lens_height = std::min(420, std::max(220, monitor_height / 3));
        int left = cursor.x + 36;
        int top = cursor.y + 36;
        if (left + lens_width > monitor_rect_.right) left = cursor.x - lens_width - 36;
        if (top + lens_height > monitor_rect_.bottom) top = cursor.y - lens_height - 36;
        left = std::clamp(left, static_cast<int>(monitor_rect_.left),
                          static_cast<int>(monitor_rect_.right) - lens_width);
        top = std::clamp(top, static_cast<int>(monitor_rect_.top),
                         static_cast<int>(monitor_rect_.bottom) - lens_height);
        zoom_rect = {left, top, left + lens_width, top + lens_height};
    } else if (view == ZoomView::Docked) {
        const int dock_height = std::min(360, std::max(220, monitor_height / 3));
        zoom_rect.bottom = zoom_rect.top + dock_height;
    }
    const int zoom_width = zoom_rect.right - zoom_rect.left;
    const int zoom_height = zoom_rect.bottom - zoom_rect.top;
    SetWindowPos(window_, HWND_TOPMOST, zoom_rect.left, zoom_rect.top,
                 zoom_width, zoom_height, SWP_SHOWWINDOW);
    SetWindowPos(magnifier_, nullptr, 0, 0, zoom_width, zoom_height,
                 SWP_NOZORDER | SWP_SHOWWINDOW);

    const float factor = overview_ ? 1.0F : controller_.state().zoom_factor;
    MAGTRANSFORM transform{{{factor, 0, 0}, {0, factor, 0}, {0, 0, 1}}};
    mag_set_transform_(magnifier_, &transform);
    const int source_width = std::max(1, static_cast<int>(
        static_cast<float>(zoom_width) / factor));
    const int source_height = std::max(1, static_cast<int>(
        static_cast<float>(zoom_height) / factor));
    int left = cursor.x - source_width / 2;
    int top = cursor.y - source_height / 2;
    left = std::clamp(left, static_cast<int>(monitor_rect_.left),
                      static_cast<int>(monitor_rect_.right) - source_width);
    top = std::clamp(top, static_cast<int>(monitor_rect_.top),
                     static_cast<int>(monitor_rect_.bottom) - source_height);
    mag_set_source_(magnifier_, RECT{left, top, left + source_width, top + source_height});
    InvalidateRect(magnifier_, nullptr, TRUE);
}

LRESULT ZoomWindow::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_TIMER: refresh_source(); return 0;
        case WM_MOUSEWHEEL: {
            const float direction = GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 0.25F : -0.25F;
            controller_.state().zoom_factor = std::clamp(
                controller_.state().zoom_factor + direction, 1.25F, 8.0F);
            refresh_source();
            return 0;
        }
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE || wparam == VK_F4) {
                controller_.toggle_zoom();
                return 0;
            }
            if (wparam == 'F') {
                controller_.preferences().zoom_view = static_cast<int>(ZoomView::Fullscreen);
                controller_.save_preferences(); refresh_source(); return 0;
            }
            if (wparam == 'L') {
                controller_.preferences().zoom_view = static_cast<int>(ZoomView::Lens);
                controller_.save_preferences(); refresh_source(); return 0;
            }
            if (wparam == 'D') {
                controller_.preferences().zoom_view = static_cast<int>(ZoomView::Docked);
                controller_.save_preferences(); refresh_source(); return 0;
            }
            if (wparam == 'M' || wparam == VK_SPACE) { cycle_view(); return 0; }
            if (wparam == 'I') {
                controller_.preferences().zoom_invert =
                    !controller_.preferences().zoom_invert;
                controller_.save_preferences(); apply_color_effect(); return 0;
            }
            if (wparam == '0') {
                overview_ = !overview_; refresh_source(); return 0;
            }
            if (wparam == VK_ADD || wparam == VK_OEM_PLUS) {
                controller_.state().zoom_factor = std::min(8.0F,
                    controller_.state().zoom_factor + 0.25F);
                refresh_source();
                return 0;
            }
            if (wparam == VK_SUBTRACT || wparam == VK_OEM_MINUS) {
                controller_.state().zoom_factor = std::max(1.25F,
                    controller_.state().zoom_factor - 0.25F);
                refresh_source();
                return 0;
            }
            break;
        case WM_KILLFOCUS:
            // Keep zoom active; global shortcut or Esc returns safely.
            return 0;
        case WM_RBUTTONDOWN:
            controller_.toggle_zoom();
            return 0;
        default: break;
    }
    return WindowBase::handle_message(message, wparam, lparam);
}

BOOL CALLBACK Controller::collect_monitor(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
    auto* rectangles = reinterpret_cast<std::vector<RECT>*>(data);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)) rectangles->push_back(info.rcMonitor);
    return TRUE;
}

void Controller::create_overlays(std::wstring& error) {
    std::vector<RECT> rectangles;
    EnumDisplayMonitors(nullptr, nullptr, &Controller::collect_monitor,
                        reinterpret_cast<LPARAM>(&rectangles));
    if (rectangles.empty()) {
        rectangles.push_back({0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)});
    }
    for (const auto& rectangle : rectangles) {
        auto overlay = std::make_unique<OverlayWindow>(*this, rectangle);
        if (!overlay->initialize(graphics_)) {
            error = L"No se pudo crear una superposicion para uno de los monitores.";
            return;
        }
        overlays_.push_back(std::move(overlay));
    }
}

bool Controller::initialize(std::wstring& error) {
    preferences_ = preferences_store_.load();
    state_.color = preferences_.color;
    state_.thickness = preferences_.thickness;
    state_.zoom_factor = preferences_.zoom_factor;
    state_.tool = preferences_.start_in_interact_mode ? Tool::Interact : Tool::Pen;
    state_.cursor_highlight = preferences_.highlight_cursor;
    if (!graphics_.initialize(error)) return false;
    create_overlays(error);
    if (!error.empty()) return false;
    palette_ = std::make_unique<PaletteWindow>(*this);
    if (!palette_->initialize(graphics_)) {
        error = L"No se pudo crear la paleta de Elite Pen.";
        return false;
    }
    if (preferences_.has_palette_position) {
        POINT saved{preferences_.palette_x, preferences_.palette_y};
        HMONITOR monitor = MonitorFromPoint(saved, MONITOR_DEFAULTTONULL);
        if (monitor) {
            MONITORINFO info{};
            info.cbSize = sizeof(info);
            GetMonitorInfoW(monitor, &info);
            const int x = std::clamp(preferences_.palette_x,
                                     static_cast<int>(info.rcWork.left),
                                     static_cast<int>(info.rcWork.right) - kPaletteWidth);
            const int y = std::clamp(preferences_.palette_y,
                                     static_cast<int>(info.rcWork.top),
                                     static_cast<int>(info.rcWork.bottom) - kPaletteHeight);
            SetWindowPos(palette_->hwnd(), HWND_TOPMOST, x, y, 0, 0,
                         SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }
    colors_ = std::make_unique<ColorWindow>(*this);
    if (!colors_->initialize(graphics_)) {
        error = L"No se pudo crear el selector de colores.";
        return false;
    }
    tools_ = std::make_unique<ToolWindow>(*this);
    if (!tools_->initialize(graphics_)) {
        error = L"No se pudo crear el selector de herramientas.";
        return false;
    }
    text_input_ = std::make_unique<TextInputWindow>(*this);
    if (!text_input_->initialize(graphics_)) {
        error = L"No se pudo crear el editor de texto.";
        return false;
    }
    settings_ = std::make_unique<SettingsWindow>(*this);
    if (!settings_->initialize()) {
        error = L"No se pudo crear la ventana de configuracion.";
        return false;
    }
    zoom_ = std::make_unique<ZoomWindow>(*this);
    if (!zoom_->initialize(graphics_)) {
        // Zoom failure is isolated: drawing remains available and an error is shown on use.
        zoom_.reset();
    }
    invalidate_all();
    return true;
}

int Controller::message_loop() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

void Controller::shutdown() {
    if (shutting_down_) return;
    shutting_down_ = true;
    save_palette_position();
    preferences_.zoom_factor = state_.zoom_factor;
    preferences_.thickness = state_.thickness;
    preferences_.color = state_.color;
    preferences_store_.save(preferences_);
    if (zoom_) zoom_->hide_zoom();
    settings_.reset();
    text_input_.reset();
    tools_.reset();
    colors_.reset();
    overlays_.clear();
    palette_.reset();
    zoom_.reset();
}

void Controller::invalidate_document() {
    for (const auto& overlay : overlays_) overlay->invalidate();
    if (palette_) palette_->invalidate();
}

void Controller::invalidate_all() { invalidate_document(); }

void Controller::update_overlay_interaction() {
    for (const auto& overlay : overlays_) overlay->update_interaction();
    // Every overlay is topmost while drawing, so explicitly restore the palette
    // above them. Its controls must remain selectable in every tool mode.
    if (palette_) {
        SetWindowPos(palette_->hwnd(), HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

bool Controller::route_palette_command(PointF screen_point) {
    if (!palette_) return false;
    POINT point{static_cast<LONG>(std::lround(screen_point.x)),
                static_cast<LONG>(std::lround(screen_point.y))};
    if (!palette_->contains_screen_point(point)) return false;
    ScreenToClient(palette_->hwnd(), &point);
    return palette_->activate_command_at(point);
}

void Controller::set_tool(Tool tool) {
    if (text_input_ && text_input_->active() && tool != Tool::Text) text_input_->commit();
    if (tool == Tool::Zoom) {
        toggle_zoom();
        return;
    }
    state_.tool = tool;
    close_panels();
    update_overlay_interaction();
    invalidate_all();
}

void Controller::set_color(Color color) {
    state_.color = color;
    preferences_.color = color;
    if (state_.tool == Tool::Interact || state_.tool == Tool::Eraser) set_tool(Tool::Pen);
    if (text_input_) text_input_->update_style(state_.color, state_.thickness);
    invalidate_all();
}

void Controller::set_thickness(float thickness) {
    state_.thickness = thickness;
    preferences_.thickness = thickness;
    if (text_input_) text_input_->update_style(state_.color, state_.thickness);
    invalidate_all();
}

void Controller::toggle_visibility() {
    if (text_input_ && text_input_->active()) text_input_->commit();
    state_.annotations_visible = !state_.annotations_visible;
    invalidate_all();
}

void Controller::toggle_whiteboard() {
    if (text_input_ && text_input_->active()) text_input_->commit();
    state_.whiteboard = !state_.whiteboard;
    if (state_.whiteboard) state_.blackboard = false;
    if (state_.whiteboard && state_.color == Color{255, 255, 255, 255}) {
        state_.color = kBlack;
        preferences_.color = kBlack;
    }
    if (state_.whiteboard && state_.tool == Tool::Interact) state_.tool = Tool::Pen;
    if (text_input_) text_input_->update_style(state_.color, state_.thickness);
    update_overlay_interaction();
    invalidate_all();
}

void Controller::toggle_blackboard() {
    if (text_input_ && text_input_->active()) text_input_->commit();
    state_.blackboard = !state_.blackboard;
    if (state_.blackboard) state_.whiteboard = false;
    if (state_.blackboard && state_.color == kBlack) {
        state_.color = kYellow;
        preferences_.color = kYellow;
    }
    if (state_.blackboard && state_.tool == Tool::Interact) state_.tool = Tool::Pen;
    if (text_input_) text_input_->update_style(state_.color, state_.thickness);
    update_overlay_interaction();
    invalidate_all();
}

void Controller::clear_document() {
    if (text_input_ && text_input_->active()) text_input_->cancel();
    if (state_.document.empty() && transient_drawables_.empty()) return;
    if (preferences_.confirm_clear &&
        MessageBoxW(palette_ ? palette_->hwnd() : nullptr,
                    L"¿Limpiar todas las anotaciones? Podras deshacer la accion.",
                    L"Limpiar — Elite Pen", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) {
        return;
    }
    const bool cleared = state_.document.clear();
    const bool cleared_transient = !transient_drawables_.empty();
    transient_drawables_.clear();
    if (cleared || cleared_transient) invalidate_document();
}

void Controller::undo() {
    if (state_.document.undo()) invalidate_document();
}

void Controller::redo() {
    if (state_.document.redo()) invalidate_document();
}

void Controller::toggle_zoom() {
    if (text_input_ && text_input_->active()) text_input_->commit();
    if (!zoom_) {
        MessageBoxW(palette_ ? palette_->hwnd() : nullptr,
                    L"La ampliacion nativa de Windows no esta disponible en este equipo.",
                    L"Elite Pen", MB_OK | MB_ICONWARNING);
        return;
    }
    if (zoom_->active()) zoom_->hide_zoom();
    else zoom_->show_zoom();
    if (palette_) {
        SetWindowPos(palette_->hwnd(), HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        palette_->invalidate();
    }
}

void Controller::toggle_color_panel() {
    if (!colors_ || !palette_) return;
    const bool was_visible = colors_->visible();
    close_panels();
    if (!was_visible) colors_->toggle_near(palette_->hwnd());
}

void Controller::toggle_tool_panel() {
    if (!tools_ || !palette_) return;
    const bool was_visible = tools_->visible() && !tools_->geometry_only();
    close_panels();
    if (!was_visible) tools_->toggle_near(palette_->hwnd());
}

void Controller::toggle_geometry_panel() {
    if (!tools_ || !palette_) return;
    const bool was_visible = tools_->visible() && tools_->geometry_only();
    close_panels();
    if (!was_visible) tools_->toggle_geometry_near(palette_->hwnd());
}

void Controller::close_panels() {
    if (colors_) colors_->hide();
    if (tools_) tools_->hide();
}

void Controller::stop_transient_mode() {
    close_panels();
    if (text_input_ && text_input_->active()) {
        text_input_->cancel();
        set_tool(Tool::Interact);
        return;
    }
    if (zoom_ && zoom_->active()) {
        zoom_->hide_zoom();
        return;
    }
    for (const auto& overlay : overlays_) overlay->cancel_gesture();
    set_tool(Tool::Interact);
}

void Controller::begin_text(PointF position) {
    close_panels();
    if (text_input_) text_input_->show_at(position, state_.color, state_.thickness);
}

void Controller::commit_text(PointF position, Color color, float thickness,
                             std::wstring text) {
    Drawable drawable;
    drawable.kind = Tool::Text;
    drawable.color = color;
    drawable.width = thickness;
    drawable.points.push_back(position);
    drawable.text = std::move(text);
    commit_drawable(std::move(drawable));
}

void Controller::commit_drawable(Drawable drawable) {
    if (preferences_.fade_seconds > 0) {
        transient_drawables_.push_back({std::move(drawable),
            monotonic_milliseconds() + static_cast<std::uint64_t>(preferences_.fade_seconds) * 1000U});
        invalidate_document();
    } else {
        state_.document.add(std::move(drawable));
        invalidate_document();
    }
}

void Controller::update_transient_ink() {
    if (transient_drawables_.empty()) return;
    const std::uint64_t now = monotonic_milliseconds();
    const auto old_size = transient_drawables_.size();
    std::erase_if(transient_drawables_, [now](const TransientDrawable& item) {
        return item.expires_at_ms <= now;
    });
    if (transient_drawables_.size() != old_size || !transient_drawables_.empty()) {
        invalidate_document();
    }
}

void Controller::capture_region(PointF first, PointF second) {
    const int left = static_cast<int>(std::floor(std::min(first.x, second.x)));
    const int top = static_cast<int>(std::floor(std::min(first.y, second.y)));
    const int right = static_cast<int>(std::ceil(std::max(first.x, second.x)));
    const int bottom = static_cast<int>(std::ceil(std::max(first.y, second.y)));
    const int width = right - left;
    const int height = bottom - top;
    if (width < 8 || height < 8) {
        if (palette_) palette_->show_notification(L"Captura cancelada",
                                                   L"Selecciona una region mas grande.");
        return;
    }

    HDC screen = GetDC(nullptr);
    HDC memory = CreateCompatibleDC(screen);
    BITMAPINFO information{};
    information.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    information.bmiHeader.biWidth = width;
    information.bmiHeader.biHeight = -height;
    information.bmiHeader.biPlanes = 1;
    information.bmiHeader.biBitCount = 32;
    information.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(screen, &information, DIB_RGB_COLORS,
                                      &pixels, nullptr, 0);
    if (!screen || !memory || !bitmap || !pixels) {
        if (bitmap) DeleteObject(bitmap);
        if (memory) DeleteDC(memory);
        if (screen) ReleaseDC(nullptr, screen);
        if (palette_) palette_->show_notification(L"No se pudo capturar",
                                                   L"Windows no entrego una superficie de captura.");
        return;
    }
    HGDIOBJ previous = SelectObject(memory, bitmap);
    wchar_t synthetic_capture[4]{};
    const bool use_synthetic_capture = GetEnvironmentVariableW(
        L"ELITE_PEN_QA_SYNTHETIC_CAPTURE", synthetic_capture,
        static_cast<DWORD>(std::size(synthetic_capture))) > 0;
    bool captured = false;
    if (use_synthetic_capture) {
        auto* destination = static_cast<std::uint32_t*>(pixels);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const std::uint8_t blue = static_cast<std::uint8_t>(40 + (x * 120 / width));
                const std::uint8_t green = static_cast<std::uint8_t>(55 + (y * 110 / height));
                const std::uint8_t red_channel = static_cast<std::uint8_t>(80 +
                    ((x + y) * 90 / std::max(width + height, 1)));
                destination[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                            static_cast<std::size_t>(x)] =
                    0xFF000000U | (static_cast<std::uint32_t>(red_channel) << 16U) |
                    (static_cast<std::uint32_t>(green) << 8U) | blue;
            }
        }
        captured = true;
    } else {
        captured = BitBlt(memory, 0, 0, width, height, screen, left, top,
                          SRCCOPY | CAPTUREBLT) != FALSE;
        if (!captured) {
            captured = capture_desktop_duplication(graphics_, left, top,
                                                    width, height, pixels);
        }
    }

    std::wstring saved_path;
    if (captured) {
        std::filesystem::path directory;
        wchar_t qa_directory[32768]{};
        const DWORD qa_length = GetEnvironmentVariableW(
            L"ELITE_PEN_QA_CAPTURE_DIR", qa_directory,
            static_cast<DWORD>(std::size(qa_directory)));
        if (qa_length > 0 && qa_length < std::size(qa_directory)) {
            directory = qa_directory;
        } else {
            PWSTR pictures = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, KF_FLAG_CREATE,
                                               nullptr, &pictures))) {
                directory = std::filesystem::path(pictures) / L"Elite Pen Captures";
                CoTaskMemFree(pictures);
            }
        }
        if (!directory.empty()) {
            std::error_code directory_error;
            std::filesystem::create_directories(directory, directory_error);
            if (!directory_error) {
                SYSTEMTIME time{};
                GetLocalTime(&time);
                wchar_t filename[96]{};
                swprintf_s(filename, std::size(filename),
                           L"Elite Pen %04u-%02u-%02u %02u-%02u-%02u-%03u.png",
                           time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute,
                           time.wSecond, time.wMilliseconds);
                const std::filesystem::path destination = directory / filename;
                ComPtr<IWICImagingFactory> factory;
                ComPtr<IWICStream> stream;
                ComPtr<IWICBitmapEncoder> encoder;
                ComPtr<IWICBitmapFrameEncode> frame;
                ComPtr<IPropertyBag2> properties;
                ComPtr<IWICBitmap> source_bitmap;
                ComPtr<IWICFormatConverter> converter;
                HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                    CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
                if (SUCCEEDED(result)) result = factory->CreateStream(stream.GetAddressOf());
                if (SUCCEEDED(result)) result = stream->InitializeFromFilename(
                    destination.c_str(), GENERIC_WRITE);
                if (SUCCEEDED(result)) result = factory->CreateEncoder(
                    GUID_ContainerFormatPng, nullptr, encoder.GetAddressOf());
                if (SUCCEEDED(result)) result = encoder->Initialize(
                    stream.Get(), WICBitmapEncoderNoCache);
                if (SUCCEEDED(result)) result = encoder->CreateNewFrame(
                    frame.GetAddressOf(), properties.GetAddressOf());
                if (SUCCEEDED(result)) result = frame->Initialize(properties.Get());
                if (SUCCEEDED(result)) result = frame->SetSize(
                    static_cast<UINT>(width), static_cast<UINT>(height));
                WICPixelFormatGUID pixel_format = GUID_WICPixelFormat32bppBGRA;
                if (SUCCEEDED(result)) result = frame->SetPixelFormat(&pixel_format);
                if (SUCCEEDED(result)) result = factory->CreateBitmapFromHBITMAP(
                    bitmap, nullptr, WICBitmapIgnoreAlpha, source_bitmap.GetAddressOf());
                if (SUCCEEDED(result)) result = factory->CreateFormatConverter(
                    converter.GetAddressOf());
                if (SUCCEEDED(result)) result = converter->Initialize(
                    source_bitmap.Get(), pixel_format, WICBitmapDitherTypeNone,
                    nullptr, 0.0, WICBitmapPaletteTypeCustom);
                if (SUCCEEDED(result)) result = frame->WriteSource(converter.Get(), nullptr);
                if (SUCCEEDED(result)) result = frame->Commit();
                if (SUCCEEDED(result)) result = encoder->Commit();
                if (SUCCEEDED(result)) saved_path = destination.wstring();
            }
        }
    }

    SelectObject(memory, previous);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);

    bool copied = false;
    if (captured && OpenClipboard(palette_ ? palette_->hwnd() : nullptr)) {
        EmptyClipboard();
        copied = SetClipboardData(CF_BITMAP, bitmap) != nullptr;
        CloseClipboard();
    }
    if (!copied) DeleteObject(bitmap);  // Clipboard owns it after a successful transfer.

    if (!captured) {
        if (palette_) palette_->show_notification(L"No se pudo capturar",
                                                   L"La copia de pantalla fallo.");
        return;
    }
    std::wstring message;
    if (copied && !saved_path.empty()) message = L"Copiada y guardada en " + saved_path;
    else if (copied) message = L"Copiada al portapapeles.";
    else if (!saved_path.empty()) message = L"Guardada en " + saved_path;
    else message = L"No se pudo guardar ni copiar la captura.";
    if (palette_) palette_->show_notification(L"Captura de Elite Pen", message);
}

void Controller::rebuild_overlays() {
    if (zoom_ && zoom_->active()) zoom_->hide_zoom();
    preview_.reset();
    state_.document.end_compound();
    overlays_.clear();
    std::wstring error;
    create_overlays(error);
    if (!error.empty()) report_runtime_error(error);
    if (palette_) SetWindowPos(palette_->hwnd(), HWND_TOPMOST, 0, 0, 0, 0,
                               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    invalidate_all();
}

void Controller::report_runtime_error(const std::wstring& message) {
    OutputDebugStringW((L"Elite Pen: " + message + L"\n").c_str());
}

void Controller::request_exit() {
    if (palette_) PostMessageW(palette_->hwnd(), kExitMessage, 0, 0);
}

void Controller::save_palette_position() {
    if (!palette_ || !palette_->hwnd()) return;
    RECT bounds{};
    if (!GetWindowRect(palette_->hwnd(), &bounds)) return;
    preferences_.has_palette_position = true;
    preferences_.palette_x = bounds.left;
    preferences_.palette_y = bounds.top;
}

void Controller::show_settings_window() {
    if (text_input_ && text_input_->active()) text_input_->commit();
    close_panels();
    if (settings_) settings_->show_settings();
}

void Controller::apply_capture_preference() {
    const DWORD affinity = preferences_.exclude_palette_from_capture
        ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE;
    if (palette_ && palette_->hwnd()) SetWindowDisplayAffinity(palette_->hwnd(), affinity);
    if (colors_ && colors_->hwnd()) SetWindowDisplayAffinity(colors_->hwnd(), affinity);
    if (tools_ && tools_->hwnd()) SetWindowDisplayAffinity(tools_->hwnd(), affinity);
    if (text_input_ && text_input_->hwnd()) SetWindowDisplayAffinity(text_input_->hwnd(), affinity);
}

void Controller::reset_palette_position() {
    if (!palette_) return;
    POINT origin{0, 0};
    HMONITOR monitor = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    SetWindowPos(palette_->hwnd(), HWND_TOPMOST, info.rcWork.left + 28,
                 info.rcWork.top + 28, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    save_palette_position();
    save_preferences();
}

void Controller::save_preferences() {
    preferences_.zoom_factor = state_.zoom_factor;
    preferences_.thickness = state_.thickness;
    preferences_.color = state_.color;
    preferences_store_.save(preferences_);
}

}  // namespace

struct Application::Impl {
    Controller controller;
    HANDLE instance_mutex{};

    ~Impl() {
        controller.shutdown();
        if (instance_mutex) CloseHandle(instance_mutex);
    }
};

Application::Application() : impl_(std::make_unique<Impl>()) {}
Application::~Application() = default;

int Application::run() {
    impl_->instance_mutex = CreateMutexW(nullptr, TRUE,
        L"Local\\PowerEliteStudio.ElitePen.Singleton.v1");
    if (!impl_->instance_mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND palette = FindWindowW(L"ElitePen.Palette", nullptr);
        if (palette) {
            ShowWindow(palette, SW_SHOWNOACTIVATE);
            SetWindowPos(palette, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return 0;
    }

    std::wstring error;
    if (!impl_->controller.initialize(error)) {
        MessageBoxW(nullptr, error.c_str(), L"Elite Pen no pudo iniciar",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    return impl_->controller.message_loop();
}

}  // namespace elite_pen::win
