// Included inside application.cpp's Windows implementation namespace. Keeping
// focus presentation here isolates its state machine from the classic zoom.

void ZoomWindow::save_focus() {
    if (valid_focus_rect(focus_scene_.area))
        controller_.preferences().focus_last = focus_scene_;
    controller_.save_preferences();
    if (focus_) focus_->invalidate();
}

void ZoomWindow::animate_focus(RectF target) {
    focus_from_ = focus_current_;
    focus_target_ = target;
    focus_started_ = std::chrono::steady_clock::now();
    focus_animating_ = focus_from_.left != target.left || focus_from_.top != target.top ||
        focus_from_.right != target.right || focus_from_.bottom != target.bottom;
    source_initialized_ = false;
    if (focus_) focus_->invalidate();
    if (active_) refresh_source();
}

void ZoomWindow::start_focus(FocusScene scene, bool select) {
    if (!initialized_ || !focus_) return;
    controller_.commit_pending_text();
    RECT requested = rounded_rect(scene.area);
    HMONITOR monitor{};
    if (valid_focus_rect(scene.area)) monitor = MonitorFromRect(&requested, MONITOR_DEFAULTTONEAREST);
    else {
        POINT cursor{};
        GetCursorPos(&cursor);
        monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    }
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) return;
    const RectF bounds = floating_rect(info.rcMonitor);
    if (valid_focus_rect(scene.area)) {
        const auto original = scene.area;
        scene.area = clamp_focus_area(scene.area, bounds);
        const float dx = scene.area.left - original.left;
        const float dy = scene.area.top - original.top;
        for (std::size_t index = 0; index < std::min(scene.mask_count, kFocusMaskCount); ++index) {
            auto& mask = scene.masks[index];
            mask = clamp_focus_area({mask.left + dx, mask.top + dy,
                mask.right + dx, mask.bottom + dy}, bounds);
        }
    }
    const bool starting = !focus_active();
    if (active_ && !focus_mode_) hide_zoom();
    const bool changed_monitor = !EqualRect(&focus_monitor_, &info.rcMonitor);
    focus_previous_masks_ = starting || changed_monitor ? FocusScene{} : focus_scene_;
    focus_mode_ = true;
    focus_closing_ = false;
    focus_overview_ = select;
    focus_pending_select_ = select;
    focus_zone_ = -1;
    focus_scene_ = scene;
    focus_monitor_ = info.rcMonitor;
    if (starting || changed_monitor) focus_current_ = bounds;
    focus_->select(FocusWindow::Selection::None);
    animate_focus(select ? bounds : fit_focus_source(scene.area, bounds));
    if (starting) {
        if (!show_zoom()) { focus_mode_ = false; return; }
    } else {
        refresh_source();
        bring_to_front();
    }
    if (valid_focus_rect(scene.area)) save_focus();
}

void ZoomWindow::exit_focus() {
    if (!focus_active()) return;
    focus_closing_ = true;
    focus_pending_select_ = false;
    focus_->select(FocusWindow::Selection::None);
    focus_overview_ = true;
    animate_focus(floating_rect(focus_monitor_));
    if (!focus_animating_) hide_zoom();
}

void ZoomWindow::refresh_focus() {
    if (!focus_active()) return;
    const bool was_animating = focus_animating_;
    const auto elapsed = std::chrono::steady_clock::now() - focus_started_;
    const float duration = static_cast<float>(controller_.preferences().focus_duration_ms);
    const float progress = focus_animating_
        ? std::chrono::duration<float, std::milli>(elapsed).count() / duration : 1.0F;
    focus_current_ = interpolate_focus(focus_from_, focus_target_, progress);
    if (progress >= 1.0F) {
        focus_current_ = focus_target_;
        focus_animating_ = false;
        focus_previous_masks_ = {};
        if (focus_closing_) { hide_zoom(); return; }
        if (focus_pending_select_) {
            focus_pending_select_ = false;
            focus_->select(FocusWindow::Selection::Zone);
        }
    }
    monitor_rect_ = focus_monitor_;
    const bool geometry_changed = !source_initialized_ || !EqualRect(&zoom_rect_, &focus_monitor_);
    const RECT source = rounded_rect(focus_current_);
    const float factor = static_cast<float>(focus_monitor_.right - focus_monitor_.left) /
        std::max(1.0F, focus_current_.width());
    if (source_initialized_ && EqualRect(&source, &source_rect_) &&
        factor == last_source_factor_ && !was_animating) {
        if (recordable_output_) {
            RECT bounds{};
            if (!GetWindowRect(foreground_before_zoom_, &bounds) ||
                !EqualRect(&bounds, &recordable_source_bounds_)) update_recordable_output();
        }
        // A fixed source still needs fresh pixels. Magnifier otherwise keeps
        // its last bitmap when the pointer and frame are both stationary.
        // This leaves geometry/transform untouched and performs no CPU capture.
        if (!recordable_output_) update_native_output(factor);
        return;
    }
    zoom_rect_ = focus_monitor_;
    source_rect_ = source;
    presented_factor_ = factor;
    if (geometry_changed) {
        const int width = zoom_rect_.right - zoom_rect_.left;
        const int height = zoom_rect_.bottom - zoom_rect_.top;
        SetWindowPos(window_, HWND_TOPMOST, zoom_rect_.left, zoom_rect_.top,
            width, height, SWP_SHOWWINDOW | SWP_NOACTIVATE);
        apply_view_regions(ZoomView::Fullscreen, width, height);
        focus_->show_for(zoom_rect_);
    }
    if (recordable_output_) update_recordable_output();
    if (!recordable_output_) update_native_output(factor);
    source_initialized_ = true;
    last_source_factor_ = factor;
    if (was_animating || geometry_changed) focus_->invalidate();
    if (geometry_changed) bring_to_front();
}

void ZoomWindow::focus_pick(RectF view_rect, bool mask) {
    if (!focus_active() || focus_animating_) return;
    const float factor = static_cast<float>(zoom_rect_.right - zoom_rect_.left) /
        std::max(1.0F, focus_current_.width());
    const ZoomViewportTransform transform{focus_current_, factor};
    RectF area = ordered_focus_rect(transform.view_to_source({view_rect.left, view_rect.top}),
        transform.view_to_source({view_rect.right, view_rect.bottom}));
    if (!valid_focus_rect(area)) {
        if (mask) return;
        const auto center = transform.view_to_source({view_rect.left, view_rect.top});
        const auto monitor = floating_rect(focus_monitor_);
        area = {center.x - monitor.width() * 0.25F, center.y - monitor.height() * 0.25F,
                center.x + monitor.width() * 0.25F, center.y + monitor.height() * 0.25F};
    }
    area = clamp_focus_area(area, floating_rect(focus_monitor_));
    if (mask) {
        if (focus_scene_.mask_count >= kFocusMaskCount) {
            if (controller_.palette()) controller_.palette()->show_notification(L"Ocho áreas ocultas",
                L"Haz clic derecho sobre una máscara para quitarla, o usa Borrar máscaras en el menú Velocidad.");
            return;
        }
        focus_scene_.masks[focus_scene_.mask_count++] = area;
        focus_->select(FocusWindow::Selection::None);
        save_focus();
    } else {
        FocusScene next;
        next.area = area;
        start_focus(next);
    }
}

void ZoomWindow::focus_remove_mask(PointF view_point) {
    if (!focus_active() || focus_animating_) return;
    const ZoomViewportTransform transform{focus_current_, presented_factor_};
    const PointF point = transform.view_to_source(view_point);
    for (std::size_t index = focus_scene_.mask_count; index > 0; --index) {
        if (!focus_scene_.masks[index - 1].contains(point)) continue;
        for (std::size_t next = index; next < focus_scene_.mask_count; ++next)
            focus_scene_.masks[next - 1] = focus_scene_.masks[next];
        --focus_scene_.mask_count;
        save_focus();
        break;
    }
}

void ZoomWindow::focus_cancel_selection() {
    if (!focus_active()) return;
    if (focus_->selection() != FocusWindow::Selection::None || focus_pending_select_) {
        focus_pending_select_ = false;
        focus_->select(FocusWindow::Selection::None);
        if (valid_focus_rect(focus_scene_.area)) {
            focus_overview_ = false;
            animate_focus(fit_focus_source(focus_scene_.area, floating_rect(focus_monitor_)));
            return;
        }
    }
    exit_focus();
}

void ZoomWindow::execute_focus(HotkeyAction action) {
    auto& p = controller_.preferences();
    if (action == HotkeyAction::FocusToggle) {
        if (focus_active()) exit_focus();
        else start_focus(p.focus_last, !valid_focus_rect(p.focus_last.area));
        return;
    }
    if (action >= HotkeyAction::FocusRecall1 && action <= HotkeyAction::FocusRecall8) {
        const auto slot = static_cast<std::size_t>(action) - static_cast<std::size_t>(HotkeyAction::FocusRecall1);
        if (valid_focus_rect(p.focus_zones[slot].area)) {
            start_focus(p.focus_zones[slot]);
            focus_zone_ = static_cast<int>(slot);
            focus_->invalidate();
        } else if (controller_.palette()) controller_.palette()->show_notification(L"Zona vacía",
            L"Selecciona un área y guárdala en uno de los ocho espacios del Zoom de enfoque.");
        return;
    }
    if (action >= HotkeyAction::FocusSave1 && action <= HotkeyAction::FocusSave8) {
        if (focus_active() && valid_focus_rect(focus_scene_.area)) {
            const auto slot = static_cast<std::size_t>(action) - static_cast<std::size_t>(HotkeyAction::FocusSave1);
            p.focus_zones[slot] = focus_scene_;
            focus_zone_ = static_cast<int>(slot);
            save_focus();
        }
        return;
    }
    if (action == HotkeyAction::FocusNext || action == HotkeyAction::FocusPrevious) {
        const int direction = action == HotkeyAction::FocusNext ? 1 : -1;
        for (int step = 1; step <= static_cast<int>(kFocusZoneCount); ++step) {
            const int base = focus_zone_ < 0 ? (direction > 0 ? -1 : 0) : focus_zone_;
            const auto slot = static_cast<std::size_t>((base + direction * step + 16) % 8);
            if (valid_focus_rect(p.focus_zones[slot].area)) {
                execute_focus(static_cast<HotkeyAction>(static_cast<std::size_t>(HotkeyAction::FocusRecall1) + slot));
                break;
            }
        }
        return;
    }
    if (action == HotkeyAction::FocusMasksToggle) {
        p.focus_masks_visible = !p.focus_masks_visible;
        save_focus();
        return;
    }
    if (action == HotkeyAction::FocusIsolation) {
        p.focus_isolation = !p.focus_isolation;
        save_focus();
        return;
    }
    if (action == HotkeyAction::FocusSelect) {
        if (!focus_active()) start_focus({}, true);
        else {
            focus_->select(FocusWindow::Selection::None);
            focus_pending_select_ = true;
            focus_overview_ = true;
            animate_focus(floating_rect(focus_monitor_));
        }
        return;
    }
    if (action == HotkeyAction::FocusMask) {
        if (!focus_active()) {
            if (!valid_focus_rect(p.focus_last.area)) { start_focus({}, true); return; }
            start_focus(p.focus_last);
        }
        focus_->select(FocusWindow::Selection::Mask);
        return;
    }
    if (action == HotkeyAction::FocusOverview && focus_active() && valid_focus_rect(focus_scene_.area)) {
        focus_pending_select_ = false;
        focus_->select(FocusWindow::Selection::None);
        focus_overview_ = !focus_overview_;
        animate_focus(focus_overview_ ? floating_rect(focus_monitor_)
            : fit_focus_source(focus_scene_.area, floating_rect(focus_monitor_)));
    }
}

void ZoomWindow::focus_command(UINT command) {
    constexpr std::array<HotkeyAction, 7> actions{HotkeyAction::FocusSelect, HotkeyAction::FocusMask,
        HotkeyAction::FocusMasksToggle, HotkeyAction::FocusIsolation, HotkeyAction::FocusOverview,
        HotkeyAction::Count, HotkeyAction::FocusToggle};
    if (command >= 6100 && command < 6107) {
        if (command != 6105) execute_focus(actions[command - 6100]);
    } else if (command >= 6200 && command < 6208) {
        const auto slot = command - 6200;
        const bool saved = valid_focus_rect(controller_.preferences().focus_zones[slot].area);
        execute_focus(static_cast<HotkeyAction>(static_cast<UINT>(saved
            ? HotkeyAction::FocusRecall1 : HotkeyAction::FocusSave1) + slot));
    } else if (command >= 6300 && command < 6308) {
        execute_focus(static_cast<HotkeyAction>(static_cast<UINT>(HotkeyAction::FocusSave1) + command - 6300));
    } else if (command >= 6400 && command < 6408) {
        controller_.preferences().focus_zones[command - 6400] = {};
        if (focus_zone_ == static_cast<int>(command - 6400)) focus_zone_ = -1;
        save_focus();
    } else if (command >= 6500 && command <= 6504) {
        constexpr std::array<int, 5> durations{120, 220, 280, 420, 600};
        controller_.preferences().focus_duration_ms = durations[command - 6500];
        save_focus();
    } else if (command == 6600) {
        focus_scene_.mask_count = 0;
        save_focus();
    }
}

constexpr std::array<RECT, 7> kFocusButtons{{
    {14, 30, 126, 66}, {132, 30, 252, 66}, {258, 30, 370, 66},
    {376, 30, 470, 66}, {476, 30, 612, 66}, {618, 30, 752, 66}, {758, 30, 886, 66}
}};

bool FocusWindow::initialize(GraphicsDevice& graphics) {
    if (!create(L"ElitePen.Focus", L"Zoom de enfoque — Elite Pen",
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP,
        WS_POPUP, {0, 0, 1, 1})) return false;
    SetWindowDisplayAffinity(window_, WDA_NONE);
    return initialize_surface(graphics);
}

void FocusWindow::show_for(RECT bounds) {
    SetWindowPos(window_, HWND_TOPMOST, bounds.left, bounds.top,
        bounds.right - bounds.left, bounds.bottom - bounds.top,
        SWP_SHOWWINDOW | SWP_NOACTIVATE);
    invalidate();
}

void FocusWindow::bring_to_front() {
    if (IsWindowVisible(window_)) SetWindowPos(window_, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

float FocusWindow::ui_scale() const noexcept {
    const float dpi = static_cast<float>(GetDpiForWindow(window_)) / 96.0F;
    return std::max(0.25F, std::min(std::clamp(dpi, 1.0F, 2.0F),
        (static_cast<float>(surface_.width()) - 24.0F) / 900.0F));
}

D2D1_POINT_2F FocusWindow::ui_origin() const noexcept {
    const float scale = ui_scale();
    return D2D1::Point2F((static_cast<float>(surface_.width()) - 900.0F * scale) * 0.5F,
        std::max(8.0F, static_cast<float>(surface_.height()) - 120.0F * scale - 16.0F));
}

int FocusWindow::command_at(POINT point) const noexcept {
    const auto origin = ui_origin();
    const float scale = ui_scale();
    const POINT local{static_cast<LONG>((static_cast<float>(point.x) - origin.x) / scale),
                       static_cast<LONG>((static_cast<float>(point.y) - origin.y) / scale)};
    for (std::size_t index = 0; index < kFocusButtons.size(); ++index)
        if (PtInRect(&kFocusButtons[index], local)) return 6100 + static_cast<int>(index);
    for (int slot = 0; slot < 8; ++slot) {
        const RECT bounds{14 + slot * 110, 76, 116 + slot * 110, 110};
        if (PtInRect(&bounds, local)) return 6200 + slot;
    }
    return -1;
}

void FocusWindow::show_menu(POINT point, int zone) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    const auto& p = controller_.preferences();
    if (zone >= 0) {
        const auto slot = static_cast<UINT>(zone);
        const bool selected = valid_focus_rect(owner_.focus_scene().area);
        AppendMenuW(menu, MF_STRING | (selected ? 0 : MF_GRAYED), 6300 + slot, L"Guardar encuadre y máscaras aquí");
        AppendMenuW(menu, MF_STRING, 6200 + slot, L"Ir a esta zona");
        AppendMenuW(menu, MF_STRING, 6400 + slot, L"Vaciar esta zona");
    } else {
        constexpr std::array<int, 5> durations{120, 220, 280, 420, 600};
        constexpr std::array<const wchar_t*, 5> titles{
            L"Muy rápida · 120 ms", L"Rápida · 220 ms", L"Equilibrada · 280 ms",
            L"Suave · 420 ms", L"Lenta · 600 ms"};
        for (std::size_t index = 0; index < durations.size(); ++index)
            AppendMenuW(menu, MF_STRING | (p.focus_duration_ms == durations[index] ? MF_CHECKED : 0),
                6500 + index, titles[index]);
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | (owner_.focus_scene().mask_count ? 0 : MF_GRAYED),
            6600, L"Borrar todas las áreas ocultas de este encuadre");
    }
    ClientToScreen(window_, &point);
    HWND previous = GetForegroundWindow();
    SetForegroundWindow(window_);
    const auto command = static_cast<UINT>(TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY,
        point.x, point.y, 0, window_, nullptr));
    DestroyMenu(menu);
    PostMessageW(window_, WM_NULL, 0, 0);
    if (previous && previous != window_ && IsWindowVisible(previous)) SetForegroundWindow(previous);
    if (command) owner_.focus_command(command);
}

LRESULT FocusWindow::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_APP + 137: return static_cast<LRESULT>(std::lround(ui_scale() * 1000.0F));
        case WM_APP + 138: return static_cast<LRESULT>(std::lround(ui_origin().x));
        case WM_APP + 139: return static_cast<LRESULT>(std::lround(ui_origin().y));
        case WM_COMMAND: owner_.focus_command(LOWORD(wparam)); return 0;
        case WM_SETCURSOR:
            SetCursor(LoadCursorW(nullptr, selection_ == Selection::None ? IDC_ARROW : IDC_CROSS));
            return TRUE;
        case WM_LBUTTONDOWN: {
            POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            const int command = command_at(point);
            if (command >= 0) {
                if (command == 6105) show_menu(point);
                else owner_.focus_command(static_cast<UINT>(command));
                return 0;
            }
            if (selection_ == Selection::None || owner_.focus_animating()) return 0;
            dragging_ = true;
            drag_start_ = drag_end_ = {static_cast<float>(point.x), static_cast<float>(point.y)};
            SetCapture(window_);
            invalidate();
            return 0;
        }
        case WM_MOUSEMOVE:
            if (dragging_) {
                drag_end_ = {static_cast<float>(GET_X_LPARAM(lparam)), static_cast<float>(GET_Y_LPARAM(lparam))};
                invalidate();
            }
            return 0;
        case WM_LBUTTONUP:
            if (dragging_) {
                drag_end_ = {static_cast<float>(GET_X_LPARAM(lparam)), static_cast<float>(GET_Y_LPARAM(lparam))};
                dragging_ = false;
                if (GetCapture() == window_) ReleaseCapture();
                owner_.focus_pick(ordered_focus_rect(drag_start_, drag_end_), selection_ == Selection::Mask);
                invalidate();
            }
            return 0;
        case WM_CAPTURECHANGED:
            if (dragging_) { dragging_ = false; invalidate(); }
            return 0;
        case WM_RBUTTONDOWN: {
            const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            const int command = command_at(point);
            if (command >= 6200) show_menu(point, command - 6200);
            else if (command >= 6100) show_menu(point);
            else owner_.focus_remove_mask({static_cast<float>(point.x), static_cast<float>(point.y)});
            return 0;
        }
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) owner_.focus_cancel_selection();
            return 0;
        case WM_CLOSE: owner_.execute_focus(HotkeyAction::FocusToggle); return 0;
    }
    return WindowBase::handle_message(message, wparam, lparam);
}

void FocusWindow::render() {
    auto* context = surface_.begin_draw(D2D1::ColorF(0, 0.0F));
    if (!context) return;
    context->SetTransform(D2D1::Matrix3x2F::Identity());
    const auto& p = controller_.preferences();
    const auto& scene = owner_.focus_scene();
    const auto& theme = current_ui_theme();
    ComPtr<ID2D1SolidColorBrush> shade, mask, accent, panel, border, text, muted, raised, selected;
    context->CreateSolidColorBrush(D2D1::ColorF(0.01F, 0.015F, 0.03F, 0.64F), shade.GetAddressOf());
    context->CreateSolidColorBrush(D2D1::ColorF(0.035F, 0.045F, 0.065F, 1.0F), mask.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.violet), accent.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.surface_1, 0.99F), panel.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.line), border.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.text), text.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.text_muted), muted.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.surface_2), raised.GetAddressOf());
    context->CreateSolidColorBrush(theme_color(theme.light ? 0xE5E0FA : 0x29233F), selected.GetAddressOf());
    const float width = static_cast<float>(surface_.width());
    const float height = static_cast<float>(surface_.height());
    const auto source = owner_.focus_source();
    const float factor = width / std::max(1.0F, source.width());
    const auto map = [&](RectF rect) {
        return D2D1::RectF((rect.left - source.left) * factor, (rect.top - source.top) * factor,
            (rect.right - source.left) * factor, (rect.bottom - source.top) * factor);
    };
    const auto outside = [&](D2D1_RECT_F hole) {
        hole.left = std::clamp(hole.left, 0.0F, width);
        hole.right = std::clamp(hole.right, hole.left, width);
        hole.top = std::clamp(hole.top, 0.0F, height);
        hole.bottom = std::clamp(hole.bottom, hole.top, height);
        context->FillRectangle(D2D1::RectF(0, 0, width, hole.top), shade.Get());
        context->FillRectangle(D2D1::RectF(0, hole.bottom, width, height), shade.Get());
        context->FillRectangle(D2D1::RectF(0, hole.top, hole.left, hole.bottom), shade.Get());
        context->FillRectangle(D2D1::RectF(hole.right, hole.top, width, hole.bottom), shade.Get());
    };
    if (selection_ == Selection::Zone) {
        if (dragging_) {
            const auto rect = ordered_focus_rect(drag_start_, drag_end_);
            outside(D2D1::RectF(rect.left, rect.top, rect.right, rect.bottom));
        } else outside(D2D1::RectF(0, 0, 0, 0));
    } else if (p.focus_isolation && !owner_.focus_overview() && valid_focus_rect(scene.area)) {
        outside(map(scene.area));
    }
    const auto draw_masks = [&](const FocusScene& content) {
        for (std::size_t index = 0; index < std::min(content.mask_count, kFocusMaskCount); ++index) {
            auto rect = map(content.masks[index]);
            rect.left = std::floor(rect.left) - 1;
            rect.top = std::floor(rect.top) - 1;
            rect.right = std::ceil(rect.right) + 1;
            rect.bottom = std::ceil(rect.bottom) + 1;
            context->FillRectangle(rect, mask.Get());
        }
    };
    if (p.focus_masks_visible) {
        draw_masks(scene);
        if (owner_.focus_animating()) draw_masks(owner_.focus_previous_masks());
    }
    if (dragging_) {
        const auto rect = ordered_focus_rect(drag_start_, drag_end_);
        const auto box = D2D1::RectF(rect.left, rect.top, rect.right, rect.bottom);
        if (selection_ == Selection::Mask) context->FillRectangle(box, shade.Get());
        context->DrawRectangle(box, accent.Get(), 2.0F);
    }
    const auto origin = ui_origin();
    const float scale = ui_scale();
    context->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) *
        D2D1::Matrix3x2F::Translation(origin.x, origin.y));
    const auto background = D2D1::RoundedRect(D2D1::RectF(0, 0, 900, 120), 14, 14);
    context->FillRoundedRectangle(background, panel.Get());
    context->DrawRoundedRectangle(background, border.Get(), 1.0F);
    ComPtr<IDWriteTextFormat> font, small;
    controller_.graphics().dwrite()->CreateTextFormat(L"Segoe UI Variable Text", nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        13.0F, L"es-CO", font.GetAddressOf());
    controller_.graphics().dwrite()->CreateTextFormat(L"Segoe UI Variable Text", nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        11.0F, L"es-CO", small.GetAddressOf());
    const auto label = [&](const std::wstring& value, D2D1_RECT_F bounds, bool dim = false) {
        if (font) context->DrawTextW(value.c_str(), static_cast<UINT32>(value.size()), font.Get(), bounds,
            dim ? muted.Get() : text.Get());
    };
    std::wstring hint = L"Encuadre fijo · Clic derecho en una zona: guardar o vaciar · Esc: volver";
    if (selection_ == Selection::Zone) hint = L"Arrastra para elegir el área · Un clic: acercamiento rápido 2× · Esc: cancelar";
    if (selection_ == Selection::Mask) hint = L"Arrastra para ocultar · Clic derecho en una máscara: quitar · Esc: cancelar";
    label(L"ZOOM DE ENFOQUE", D2D1::RectF(15, 7, 205, 27));
    if (small) context->DrawTextW(hint.c_str(), static_cast<UINT32>(hint.size()), small.Get(),
        D2D1::RectF(206, 9, 889, 28), muted.Get());
    const auto button = [&](RECT bounds, const std::wstring& title, bool active) {
        const auto box = D2D1::RoundedRect(D2D1::RectF(static_cast<float>(bounds.left),
            static_cast<float>(bounds.top), static_cast<float>(bounds.right), static_cast<float>(bounds.bottom)), 7, 7);
        context->FillRoundedRectangle(box, active ? selected.Get() : raised.Get());
        context->DrawRoundedRectangle(box, active ? accent.Get() : border.Get(), 0.8F);
        label(title, D2D1::RectF(static_cast<float>(bounds.left + 10), static_cast<float>(bounds.top + 9),
            static_cast<float>(bounds.right - 3), static_cast<float>(bounds.bottom)));
    };
    std::array<std::wstring, 7> labels{L"Elegir zona", L"Ocultar área", L"Máscaras", L"Aislar",
        owner_.focus_overview() ? L"Volver al área" : L"Vista completa",
        L"Velocidad · " + std::to_wstring(p.focus_duration_ms), L"Cerrar"};
    for (std::size_t index = 0; index < labels.size(); ++index) {
        const bool active = (index == 0 && selection_ == Selection::Zone) ||
            (index == 1 && selection_ == Selection::Mask) ||
            (index == 2 && p.focus_masks_visible) || (index == 3 && p.focus_isolation);
        button(kFocusButtons[index], labels[index], active);
    }
    for (int slot = 0; slot < 8; ++slot) {
        const bool saved = valid_focus_rect(p.focus_zones[static_cast<std::size_t>(slot)].area);
        button({14 + slot * 110, 76, 116 + slot * 110, 110},
            std::to_wstring(slot + 1) + (saved ? L" · Zona" : L" · Guardar"), owner_.focus_zone() == slot);
    }
    context->SetTransform(D2D1::Matrix3x2F::Identity());
    std::wstring error;
    if (!surface_.end_draw(error)) controller_.report_runtime_error(error);
}
