// Independent native board. The screen overlay remains an independent document.
namespace board_ui {
constexpr UINT_PTR kSaveTimer = 0xB01, kCaptureTimer = 0xB02;
constexpr UINT kQa = WM_APP + 200;
std::wstring wide(const std::string &text) {
    if (text.empty())
        return {};
    const int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), count);
    return result;
}
std::string utf8(const std::wstring &text) {
    const int count = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0,
                                          nullptr, nullptr);
    std::string result(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), count, nullptr,
                        nullptr);
    return result;
}
RectF box(PointF a, PointF b) {
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y)};
}
D2D1_RECT_F rect(RectF r) { return D2D1::RectF(r.left, r.top, r.right, r.bottom); }
bool down(int key) { return (GetKeyState(key) & 0x8000) != 0; }
ComPtr<IWICImagingFactory> factory() {
    ComPtr<IWICImagingFactory> result;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                     IID_PPV_ARGS(result.GetAddressOf()));
    return result;
}
std::shared_ptr<const BoardImage> encode_image(IWICBitmapSource *source) {
    if (!source)
        return {};
    UINT width{}, height{};
    source->GetSize(&width, &height);
    if (!width || !height || width > 16384 || height > 16384 ||
        static_cast<std::uint64_t>(width) * height > 64000000)
        return {};
    auto wic = factory();
    if (!wic)
        return {};
    ComPtr<IStream> stream;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, stream.GetAddressOf())))
        return {};
    ComPtr<IWICBitmapEncoder> encoder;
    if (FAILED(wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.GetAddressOf())) ||
        FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)))
        return {};
    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> properties;
    if (FAILED(encoder->CreateNewFrame(frame.GetAddressOf(), properties.GetAddressOf())) ||
        FAILED(frame->Initialize(properties.Get())) || FAILED(frame->SetSize(width, height)))
        return {};
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    if (FAILED(frame->SetPixelFormat(&format)) || FAILED(frame->WriteSource(source, nullptr)) ||
        FAILED(frame->Commit()) || FAILED(encoder->Commit()))
        return {};
    STATSTG stat{};
    if (FAILED(stream->Stat(&stat, STATFLAG_NONAME)) || stat.cbSize.QuadPart > 64U * 1024U * 1024U)
        return {};
    auto result = std::make_shared<BoardImage>();
    result->width = width;
    result->height = height;
    result->png.resize(static_cast<std::size_t>(stat.cbSize.QuadPart));
    LARGE_INTEGER zero{};
    stream->Seek(zero, STREAM_SEEK_SET, nullptr);
    ULONG read{};
    if (FAILED(stream->Read(result->png.data(), static_cast<ULONG>(result->png.size()), &read)) ||
        read != result->png.size())
        return {};
    return result;
}
ComPtr<IWICFormatConverter> decode_image(const BoardImage &image) {
    auto wic = factory();
    if (!wic)
        return {};
    ComPtr<IWICStream> stream;
    if (FAILED(wic->CreateStream(stream.GetAddressOf())) ||
        FAILED(stream->InitializeFromMemory(const_cast<BYTE *>(image.png.data()),
                                            static_cast<DWORD>(image.png.size()))))
        return {};
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(wic->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad,
                                            decoder.GetAddressOf())) ||
        FAILED(decoder->GetFrame(0, frame.GetAddressOf())))
        return {};
    UINT width{}, height{};
    if (FAILED(frame->GetSize(&width, &height)) || width != image.width || height != image.height)
        return {};
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(wic->CreateFormatConverter(converter.GetAddressOf())) ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                                     nullptr, 0, WICBitmapPaletteTypeCustom)))
        return {};
    return converter;
}
} // namespace board_ui

class BoardCaptureWindow final : public WindowBase {
  public:
    BoardCaptureWindow(Controller &c, BoardWindow &owner) : WindowBase(c), owner_(owner) {}
    bool start();

  protected:
    LRESULT handle_message(UINT, WPARAM, LPARAM) override;
    void render() override;

  private:
    BoardWindow &owner_;
    RECT desktop_{};
    PointF first_{}, last_{};
    bool dragging_{};
};

class BoardWindow final : public WindowBase {
  public:
    explicit BoardWindow(Controller &c) : WindowBase(c) {}
    ~BoardWindow() override {
        flush();
        if (edit_)
            DestroyWindow(edit_);
    }
    bool initialize();
    void show();
    void hide();
    bool active() const noexcept {
        return capturing_ || (window_ && IsWindowVisible(window_) && !IsIconic(window_));
    }
    bool execute(HotkeyAction action);
    void finish_capture(std::optional<RECT> area);

  protected:
    LRESULT handle_message(UINT, WPARAM, LPARAM) override;
    void render() override;

  private:
    enum class Mode { Ink, Select, Hand };
    enum class Drag { None, Ink, Erase, Pan, Area, Move, Resize };
    struct Button {
        int command;
        std::wstring label;
        RectF bounds;
    };
    struct ImageCache {
        std::shared_ptr<const BoardImage> asset;
        ComPtr<ID2D1Bitmap1> bitmap;
    };
    BoardDocument document_;
    BoardCamera camera_;
    BoardIds selected_;
    std::optional<BoardObject> preview_;
    Tool tool_{Tool::Pen};
    Mode mode_{Mode::Ink};
    Drag drag_{Drag::None};
    Color color_{kBlack};
    float thickness_{4};
    bool dark_{}, pressure_{true}, grid_{}, capturing_{}, pointer_{}, pointer_controls_{}, toolbar_{true},
        fullscreen_{};
    int scale_{100}, toolbar_scroll_{};
    UINT32 pointer_id_{};
    std::array<unsigned, 6> pointer_events_{};
    float toolbar_height_{}, toolbar_total_{};
    PointF start_{}, last_{}, current_{};
    RectF selection_start_{}, selection_target_{};
    RECT normal_bounds_{};
    DWORD normal_style_{};
    HWND edit_{};
    PointF text_position_{};
    std::wstring status_{L"Listo · Espacio + arrastrar: mover · Rueda: zoom · Ctrl+S: guardar"};
    std::filesystem::path filename_, recovery_;
    std::uint64_t change_{}, saved_change_{}, last_edit_{}, cache_revision_{~0ULL}, cache_generation_{};
    std::uint64_t last_snapshot_at_{};
    std::size_t cache_count_{};
    BoardCamera cache_camera_{};
    bool cache_dark_{}, cache_grid_{};
    ComPtr<ID2D1Bitmap1> cache_;
    std::vector<ImageCache> images_;
    std::size_t image_bytes_{};
    std::future<std::pair<bool, std::string>> saving_;
    std::uint64_t saving_change_{};
    std::unique_ptr<BoardCaptureWindow> capture_;
    std::optional<RECT> capture_area_;
    std::vector<Button> buttons_;
    void layout();
    void command(int id);
    int menu(const std::vector<std::pair<int, std::wstring>> &entries);
    void shape_menu();
    void emoji_menu();
    void file_menu();
    void options_menu();
    void context_menu(PointF p);
    void changed(const wchar_t *status = nullptr);
    BoardFile snapshot() const;
    void collect_save();
    void autosave();
    bool flush();
    bool archive();
    bool load(const std::filesystem::path &path, bool recovery = false);
    void save(bool choose = false);
    void open();
    void new_board();
    std::filesystem::path dialog(bool save, const wchar_t *filter, const wchar_t *ext);
    void error(const std::string &value);
    void insert_image(std::shared_ptr<const BoardImage> image);
    void import_image();
    void paste();
    void begin_capture();
    void capture_pixels();
    void export_png(const std::filesystem::path &target = {});
    void begin(PointF view, float pressure, bool erase = false, bool pan = false);
    void move(PointF view, float pressure);
    void finish(PointF view, float pressure);
    void cancel();
    void pen_samples(WPARAM wparam, bool finish);
    PointF view_point(LPARAM p) const {
        return {static_cast<float>(GET_X_LPARAM(p)), static_cast<float>(GET_Y_LPARAM(p)) - toolbar_height_};
    }
    PointF viewport() const {
        return {static_cast<float>(surface_.width()),
                std::max(1.0F, static_cast<float>(surface_.height()) - toolbar_height_ - 28)};
    }
    void set_tool(Tool tool) {
        commit_text();
        tool_ = tool;
        mode_ = Mode::Ink;
        invalidate();
    }
    void text(PointF p);
    void commit_text();
    static LRESULT CALLBACK edit_proc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
    void toggle_fullscreen();
    bool update_cache();
    void draw_object(ID2D1DeviceContext *, DrawableRenderResources &, const BoardObject &);
};

bool BoardWindow::initialize() {
    RECT bounds{100, 80, 1500, 980};
    HMONITOR monitor = MonitorFromWindow(controller_.palette()->hwnd(), MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)) {
        bounds = info.rcWork;
        const int margin = std::max(10, static_cast<int>((bounds.right - bounds.left) / 18));
        InflateRect(&bounds, -margin, -margin);
    }
    if (!create(L"ElitePenBoardWindow", L"Elite Pen · Whiteboard / Blackboard",
                WS_EX_APPWINDOW | WS_EX_NOREDIRECTIONBITMAP, WS_OVERLAPPEDWINDOW, bounds) ||
        !initialize_surface(controller_.graphics()))
        return false;
    apply_premium_window_chrome(window_);
    DragAcceptFiles(window_, TRUE);
    recovery_ = controller_.board_directory() / L"recovery.epboard";
    if (std::filesystem::exists(recovery_) || std::filesystem::exists(recovery_.wstring() + L".previous"))
        load(recovery_, true);
    last_snapshot_at_ = monotonic_milliseconds();
    SetTimer(window_, board_ui::kSaveTimer, 1000, nullptr);
    return true;
}
void BoardWindow::show() {
    ShowWindow(window_, SW_RESTORE);
    controller_.sync_board_visibility();
    SetForegroundWindow(window_);
    SetFocus(window_);
    invalidate();
}
void BoardWindow::hide() {
    if (capturing_) {
        KillTimer(window_, board_ui::kCaptureTimer);
        if (capture_) {
            if (GetCapture() == capture_->hwnd())
                ReleaseCapture();
            ShowWindow(capture_->hwnd(), SW_HIDE);
        }
        capture_area_.reset();
        capturing_ = false;
    }
    commit_text();
    cancel();
    if (!flush()) {
        MessageBoxW(window_,
                    L"No se pudo guardar la recuperación. La pizarra sigue abierta para que puedas guardarla "
                    L"en otra carpeta.",
                    L"Elite Pen", MB_OK | MB_ICONERROR);
        return;
    }
    ShowWindow(window_, SW_HIDE);
    controller_.sync_board_visibility();
}
void BoardWindow::error(const std::string &value) {
    status_ = board_ui::wide(value);
    invalidate();
    MessageBoxW(window_, status_.c_str(), L"Pizarra de Elite Pen", MB_OK | MB_ICONERROR);
}
void BoardWindow::changed(const wchar_t *status) {
    ++change_;
    last_edit_ = monotonic_milliseconds();
    if (status)
        status_ = status;
    else
        status_ = L"Cambios pendientes de recuperación";
    invalidate();
}
BoardFile BoardWindow::snapshot() const {
    BoardFile file;
    file.objects = document_.objects();
    file.camera = camera_;
    file.dark = dark_;
    file.pressure = pressure_;
    file.grid = grid_;
    file.panel_scale = static_cast<std::uint32_t>(scale_);
    file.panel_visible = toolbar_;
    file.pen_width = thickness_;
    file.pen_color = color_;
    file.source_path = board_ui::utf8(filename_.wstring());
    return file;
}
void BoardWindow::collect_save() {
    if (saving_.valid() && saving_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        const auto [ok, message] = saving_.get();
        if (ok) {
            saved_change_ = saving_change_;
            if (change_ == saved_change_)
                status_ = L"Recuperación local al día";
        } else
            status_ = L"No se pudo guardar: " + board_ui::wide(message);
        invalidate();
    }
}
void BoardWindow::autosave() {
    collect_save();
    const auto now = monotonic_milliseconds();
    // A continuous explanation still checkpoints completed objects at least every 15 s.
    if (change_ == saved_change_ || saving_.valid() ||
        (now - last_edit_ < 2000 && now - last_snapshot_at_ < 15000))
        return;
    const auto file = snapshot();
    const auto path = recovery_;
    saving_change_ = change_;
    last_snapshot_at_ = now;
    saving_ = std::async(std::launch::async, [file, path] {
        std::string message;
        bool ok = write_board(path, file, message);
        return std::make_pair(ok, message);
    });
}
bool BoardWindow::flush() {
    if (recovery_.empty())
        return true;
    if (saving_.valid()) {
        saving_.wait();
        collect_save();
    }
    if (change_ == saved_change_)
        return true;
    std::string message;
    if (!write_board(recovery_, snapshot(), message)) {
        status_ = board_ui::wide(message);
        return false;
    }
    saved_change_ = change_;
    return true;
}
bool BoardWindow::archive() {
    commit_text();
    cancel();
    if (!flush()) {
        error(board_ui::utf8(status_));
        return false;
    }
    if (document_.objects().empty())
        return true;
    SYSTEMTIME t{};
    GetLocalTime(&t);
    wchar_t name[100]{};
    swprintf_s(name, L"sesion-%04u%02u%02u-%02u%02u%02u-%03u.epboard", t.wYear, t.wMonth, t.wDay, t.wHour,
               t.wMinute, t.wSecond, t.wMilliseconds);
    std::string message;
    if (!write_board(recovery_.parent_path() / name, snapshot(), message)) {
        error(message);
        return false;
    }
    return true;
}
bool BoardWindow::load(const std::filesystem::path &path, bool recovery) {
    BoardFile file;
    std::string message;
    if (!read_board(path, file, message)) {
        error(message);
        return false;
    }
    std::unordered_set<const BoardImage *> checked_images;
    for (const auto &object : file.objects)
        if (object->image && checked_images.insert(object->image.get()).second &&
            !board_ui::decode_image(*object->image)) {
            error("La pizarra contiene una imagen dañada o no admitida.");
            return false;
        }
    document_.restore(std::move(file.objects));
    camera_ = file.camera;
    dark_ = file.dark;
    pressure_ = file.pressure;
    grid_ = file.grid;
    scale_ = static_cast<int>(file.panel_scale);
    toolbar_ = file.panel_visible;
    thickness_ = file.pen_width;
    filename_ = recovery ? std::filesystem::path(board_ui::wide(file.source_path)) : path;
    color_ = file.pen_color;
    selected_.clear();
    images_.clear();
    cache_.Reset();
    changed(recovery ? L"Sesión recuperada · puedes continuar dibujando" : L"Pizarra abierta");
    if (!message.empty())
        status_ = board_ui::wide(message);
    return true;
}
std::filesystem::path BoardWindow::dialog(bool save_file, const wchar_t *filter, const wchar_t *ext) {
    wchar_t name[32768]{};
    if (save_file && !filename_.empty()) {
        auto suggested = filename_;
        suggested.replace_extension(ext);
        wcsncpy_s(name, suggested.c_str(), _TRUNCATE);
    }
    OPENFILENAMEW f{};
    f.lStructSize = sizeof(f);
    f.hwndOwner = window_;
    f.lpstrFilter = filter;
    f.lpstrFile = name;
    f.nMaxFile = 32768;
    f.lpstrDefExt = ext;
    f.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST |
              (save_file ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    return (save_file ? GetSaveFileNameW(&f) : GetOpenFileNameW(&f)) ? std::filesystem::path(name)
                                                                     : std::filesystem::path{};
}
void BoardWindow::save(bool choose) {
    commit_text();
    const auto path = (choose || filename_.empty())
                          ? dialog(true, L"Pizarra editable de Elite Pen\0*.epboard\0\0", L"epboard")
                          : filename_;
    if (path.empty())
        return;
    auto file = snapshot();
    file.source_path = board_ui::utf8(path.wstring());
    std::string message;
    if (!write_board(path, file, message)) {
        error(message);
        return;
    }
    filename_ = path;
    changed(L"Archivo guardado · ");
    status_ += path.filename().wstring();
    flush();
}
void BoardWindow::open() {
    const auto path = dialog(false, L"Pizarra editable de Elite Pen\0*.epboard\0\0", L"epboard");
    if (path.empty() || !archive())
        return;
    if (load(path))
        flush();
}
void BoardWindow::new_board() {
    if (!archive())
        return;
    document_.restore({});
    selected_.clear();
    images_.clear();
    filename_.clear();
    camera_ = {};
    changed(L"Pizarra nueva · la sesión anterior está en Recuperaciones");
    flush();
}

void BoardWindow::layout() {
    buttons_.clear();
    if (!toolbar_) {
        toolbar_height_ = toolbar_total_ = 0;
        return;
    }
    const float scale = static_cast<float>(scale_) / 100, width = static_cast<float>(surface_.width());
    const float bw = 105 * scale, bh = 33 * scale, gap = 5 * scale;
    float x = gap, y = gap;
    const auto add = [&](int id, const std::wstring &label, float w) {
        if (x + w + gap > width && x > gap) {
            x = gap;
            y += bh + gap;
        }
        buttons_.push_back({id, label, {x, y, x + w, y + bh}});
        x += w + gap;
    };
    add(1, L"Archivo", bw);
    add(2, L"Lápiz", bw);
    add(3, L"Resaltador", bw);
    add(4, L"Borrador", bw);
    add(5, L"Selección", bw);
    add(6, L"Mano", bw);
    add(7, L"Figuras", bw);
    add(8, L"Texto", bw);
    add(9, L"Emojis", bw);
    add(10, L"Captura", bw);
    add(11, L"Deshacer", bw);
    add(12, L"Rehacer", bw);
    add(13, dark_ ? L"Whiteboard" : L"Blackboard", bw);
    add(14, L"Opciones", bw);
    x = gap;
    y += bh + gap;
    for (int n = 0; n < 8; ++n)
        add(100 + n, L"", bh);
    add(15, L"Color…", bw * .85F);
    add(16, L"− Trazo", bw * .85F);
    add(17, L"+ Trazo", bw * .85F);
    add(18, L"− Zoom", bw * .85F);
    add(19, L"+ Zoom", bw * .85F);
    add(20, L"Encuadrar", bw);
    toolbar_total_ = y + bh + gap;
    toolbar_height_ = std::min(toolbar_total_, static_cast<float>(surface_.height()) * .55F);
    toolbar_scroll_ =
        std::clamp(toolbar_scroll_, 0, std::max(0, static_cast<int>(toolbar_total_ - toolbar_height_)));
    for (auto &b : buttons_) {
        b.bounds.top -= static_cast<float>(toolbar_scroll_);
        b.bounds.bottom -= static_cast<float>(toolbar_scroll_);
    }
}
int BoardWindow::menu(const std::vector<std::pair<int, std::wstring>> &entries) {
    HMENU popup = CreatePopupMenu();
    for (const auto &[id, label] : entries) {
        if (!id)
            AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
        else
            AppendMenuW(popup, MF_STRING, static_cast<UINT_PTR>(id), label.c_str());
    }
    POINT p{};
    GetCursorPos(&p);
    const int chosen = TrackPopupMenu(popup, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, window_, nullptr);
    DestroyMenu(popup);
    return chosen;
}
void BoardWindow::file_menu() {
    const int id = menu({{1, L"Nueva pizarra\tCtrl+N"},
                         {2, L"Abrir…\tCtrl+O"},
                         {3, L"Guardar\tCtrl+S"},
                         {4, L"Guardar como…\tCtrl+Shift+S"},
                         {0, L""},
                         {5, L"Insertar imagen…"},
                         {6, L"Pegar imagen\tCtrl+V"},
                         {7, L"Exportar vista como PNG…"},
                         {8, L"Recuperaciones locales…"},
                         {0, L""},
                         {9, L"Volver al escritorio\tEsc"}});
    switch (id) {
    case 1:
        new_board();
        break;
    case 2:
        open();
        break;
    case 3:
        save();
        break;
    case 4:
        save(true);
        break;
    case 5:
        import_image();
        break;
    case 6:
        paste();
        break;
    case 7:
        export_png();
        break;
    case 8:
        ShellExecuteW(window_, L"open", recovery_.parent_path().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case 9:
        hide();
        break;
    default:
        break;
    }
}
void BoardWindow::shape_menu() {
    std::vector<std::pair<int, std::wstring>> entries;
    for (const auto tool : kGeometryTools)
        entries.push_back({static_cast<int>(tool) + 1000, tool_name(tool)});
    const int id = menu(entries);
    if (id >= 1000)
        set_tool(static_cast<Tool>(id - 1000));
}
void BoardWindow::emoji_menu() {
    HMENU root = CreatePopupMenu();
    for (std::size_t n = 0; n < kEmojiFavoriteCount; ++n) {
        const int id = controller_.preferences().emoji_favorites[n];
        const auto label =
            L"Favorito " + std::to_wstring(n + 1) + L" · " +
            (valid_emoji(id) ? std::wstring(kEmojis[static_cast<std::size_t>(id)].glyph) : L"Vacío");
        AppendMenuW(root, MF_STRING | (valid_emoji(id) ? 0 : MF_GRAYED), 2000 + static_cast<UINT_PTR>(id),
                    label.c_str());
    }
    for (std::size_t offset = 0; offset < kEmojis.size(); offset += 18) {
        HMENU page = CreatePopupMenu();
        for (std::size_t n = offset; n < std::min(offset + 18, kEmojis.size()); ++n) {
            const std::wstring name = std::wstring(kEmojis[n].glyph) + L"  " + kEmojis[n].name;
            AppendMenuW(page, MF_STRING, 2000 + n, name.c_str());
        }
        const std::wstring label = L"Emojis " + std::to_wstring(offset + 1) + L"–" +
                                   std::to_wstring(std::min(offset + 18, kEmojis.size()));
        AppendMenuW(root, MF_POPUP, reinterpret_cast<UINT_PTR>(page), label.c_str());
    }
    POINT p{};
    GetCursorPos(&p);
    const int id = TrackPopupMenu(root, TPM_RETURNCMD, p.x, p.y, 0, window_, nullptr);
    DestroyMenu(root);
    if (id >= 2000 && valid_emoji(id - 2000)) {
        controller_.preferences().selected_emoji = id - 2000;
        set_tool(Tool::Emoji);
    }
}
void BoardWindow::options_menu() {
    std::vector<std::pair<int, std::wstring>> e{
        {1, pressure_ ? L"✓ Presión del lápiz" : L"Presión del lápiz"},
        {2, grid_ ? L"✓ Cuadrícula" : L"Cuadrícula"},
        {3, L"Pantalla completa\tF11"},
        {4, L"Ocultar herramientas\tTab"},
        {0, L""}};
    for (const int size : {80, 100, 125, 150, 200, 300})
        e.push_back({size, L"Panel al " + std::to_wstring(size) + L" %" + (scale_ == size ? L" ✓" : L"")});
    e.push_back({0, L""});
    e.push_back({5, L"Limpiar pizarra…"});
    e.push_back({6, L"Ayuda de controles"});
    const int id = menu(e);
    if (id == 1) {
        pressure_ = !pressure_;
        changed();
    } else if (id == 2) {
        grid_ = !grid_;
        cache_.Reset();
        changed();
    } else if (id == 3)
        toggle_fullscreen();
    else if (id == 4) {
        toolbar_ = false;
        changed();
    } else if (id >= 80) {
        scale_ = id;
        toolbar_scroll_ = 0;
        changed();
    } else if (id == 5 && MessageBoxW(window_, L"¿Limpiar la pizarra? Puedes deshacer esta acción.",
                                      L"Elite Pen", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        document_.clear();
        selected_.clear();
        changed();
    } else if (id == 6)
        MessageBoxW(
            window_,
            L"Lápiz: presión por punto con Windows Ink.\nEspacio + arrastrar, botón central o Mano: "
            L"desplazar.\nRueda: zoom alrededor del cursor. Shift + rueda: horizontal.\nSelección: clic o "
            L"arrastrar un área; Shift añade o quita.\nArrastra la selección para mover; esquina inferior "
            L"derecha para escalar.\nClic derecho: capas, bloquear, duplicar o eliminar.\nCtrl+A: "
            L"seleccionar todo; Supr: eliminar; Ctrl+D: duplicar.\nCtrl+Z / Ctrl+Y: deshacer / rehacer. "
            L"Ctrl+V: pegar imagen.\nTab: mostrar/ocultar panel; F11: pantalla completa.\nEsc: cancelar o "
            L"volver al escritorio.\nLos atajos globales de Elite Pen también funcionan aquí.\nLa "
            L"recuperación se guarda automáticamente en el equipo.",
            L"Controles de la pizarra", MB_OK);
}
void BoardWindow::command(int id) {
    static constexpr std::array<Color, 8> colors{
        {kBlack, {255, 255, 255, 255}, kYellow, kBlue, kRed, kGreen, kPurple, {249, 115, 22, 255}}};
    if (id >= 100 && id < 108) {
        color_ = colors[static_cast<std::size_t>(id - 100)];
        changed();
        return;
    }
    switch (id) {
    case 1:
        file_menu();
        break;
    case 2:
        set_tool(Tool::Pen);
        break;
    case 3:
        set_tool(Tool::Highlighter);
        break;
    case 4:
        set_tool(Tool::Eraser);
        break;
    case 5:
        commit_text();
        mode_ = Mode::Select;
        break;
    case 6:
        commit_text();
        mode_ = Mode::Hand;
        break;
    case 7:
        shape_menu();
        break;
    case 8:
        set_tool(Tool::Text);
        break;
    case 9:
        emoji_menu();
        break;
    case 10:
        begin_capture();
        break;
    case 11:
        commit_text();
        document_.undo();
        selected_.clear();
        changed();
        break;
    case 12:
        document_.redo();
        selected_.clear();
        changed();
        break;
    case 13:
        dark_ = !dark_;
        if (color_ == kBlack || color_ == Color{245, 245, 247, 255})
            color_ = dark_ ? Color{245, 245, 247, 255} : kBlack;
        cache_.Reset();
        changed();
        break;
    case 14:
        options_menu();
        break;
    case 15: {
        CHOOSECOLORW chooser{};
        chooser.lStructSize = sizeof(chooser);
        COLORREF custom[16]{};
        chooser.hwndOwner = window_;
        chooser.rgbResult = RGB(color_.r, color_.g, color_.b);
        chooser.lpCustColors = custom;
        chooser.Flags = CC_RGBINIT | CC_FULLOPEN;
        if (ChooseColorW(&chooser))
            color_ = {GetRValue(chooser.rgbResult), GetGValue(chooser.rgbResult),
                      GetBValue(chooser.rgbResult), 255};
        break;
    }
    case 16:
        thickness_ = std::max(.5F, thickness_ / 1.4F);
        break;
    case 17:
        thickness_ = std::min(100.0F, thickness_ * 1.4F);
        break;
    case 18:
    case 19:
        camera_.zoom_at({viewport().x / 2, viewport().y / 2}, id == 19 ? 1.25F : .8F);
        changed();
        break;
    case 20:
        camera_.frame(document_.bounds(), viewport());
        changed();
        break;
    default:
        break;
    }
    if (id == 15 || id == 16 || id == 17)
        changed();
    invalidate();
}

void BoardWindow::insert_image(std::shared_ptr<const BoardImage> image) {
    if (!image) {
        error("No se pudo insertar la imagen. Usa PNG, JPEG, BMP o una captura válida (máximo 64 "
              "megapíxeles).");
        return;
    }
    BoardObject object;
    object.image = std::move(image);
    const auto view = viewport();
    const float factor = std::min({1.0F, view.x * .75F / static_cast<float>(object.image->width),
                                   view.y * .75F / static_cast<float>(object.image->height)}) /
                         camera_.zoom;
    const auto center = camera_.to_world({view.x / 2, view.y / 2});
    const float width = static_cast<float>(object.image->width) * factor,
                height = static_cast<float>(object.image->height) * factor;
    object.image_bounds = {center.x - width / 2, center.y - height / 2, center.x + width / 2,
                           center.y + height / 2};
    if (document_.add(std::move(object))) {
        selected_ = {document_.objects().back()->id};
        mode_ = Mode::Select;
        changed(L"Imagen insertada · arrastra para mover y usa la esquina para escalar");
    } else
        error("No se pudo añadir la imagen al documento.");
}
void BoardWindow::import_image() {
    const auto path = dialog(false, L"Imágenes\0*.png;*.jpg;*.jpeg;*.bmp;*.gif\0\0", L"png");
    if (path.empty())
        return;
    auto wic = board_ui::factory();
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (wic &&
        SUCCEEDED(wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                 WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf())) &&
        SUCCEEDED(decoder->GetFrame(0, frame.GetAddressOf())))
        insert_image(board_ui::encode_image(frame.Get()));
    else
        error("No se pudo abrir esta imagen.");
}
void BoardWindow::paste() {
    if (!OpenClipboard(window_))
        return;
    std::shared_ptr<const BoardImage> image;
    const auto bitmap = reinterpret_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
    if (bitmap) {
        auto wic = board_ui::factory();
        ComPtr<IWICBitmap> source;
        if (wic && SUCCEEDED(wic->CreateBitmapFromHBITMAP(bitmap, nullptr, WICBitmapIgnoreAlpha,
                                                          source.GetAddressOf())))
            image = board_ui::encode_image(source.Get());
    }
    CloseClipboard();
    insert_image(std::move(image));
}
void BoardWindow::begin_capture() {
    commit_text();
    cancel();
    capturing_ = true;
    ShowWindow(window_, SW_HIDE);
    controller_.sync_board_visibility();
    DwmFlush();
    if (!capture_)
        capture_ = std::make_unique<BoardCaptureWindow>(controller_, *this);
    if (!capture_->start()) {
        capturing_ = false;
        show();
        error("No se pudo abrir el selector de captura.");
    }
}
void BoardWindow::finish_capture(std::optional<RECT> area) {
    capture_area_ = area;
    if (!area) {
        capturing_ = false;
        show();
        return;
    }
    SetTimer(window_, board_ui::kCaptureTimer, 100, nullptr);
}
void BoardWindow::capture_pixels() {
    KillTimer(window_, board_ui::kCaptureTimer);
    if (capture_area_) {
        const auto r = *capture_area_;
        const int width = r.right - r.left, height = r.bottom - r.top;
        HDC screen = GetDC(nullptr), memory = CreateCompatibleDC(screen);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void *pixels{};
        HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        HGDIOBJ old = bitmap ? SelectObject(memory, bitmap) : nullptr;
        bool ok = bitmap && BitBlt(memory, 0, 0, width, height, screen, r.left, r.top, SRCCOPY | CAPTUREBLT);
        GdiFlush();
        if (!ok && pixels)
            ok = capture_desktop_duplication(controller_.graphics(), r.left, r.top, width, height, pixels);
        std::shared_ptr<const BoardImage> image;
        if (ok) {
            auto wic = board_ui::factory();
            ComPtr<IWICBitmap> source;
            if (wic && SUCCEEDED(wic->CreateBitmapFromHBITMAP(bitmap, nullptr, WICBitmapIgnoreAlpha,
                                                              source.GetAddressOf())))
                image = board_ui::encode_image(source.Get());
        }
        if (old)
            SelectObject(memory, old);
        if (bitmap)
            DeleteObject(bitmap);
        DeleteDC(memory);
        ReleaseDC(nullptr, screen);
        capturing_ = false;
        show();
        insert_image(std::move(image));
    } else {
        capturing_ = false;
        show();
    }
    capture_area_.reset();
}
void BoardWindow::export_png(const std::filesystem::path &target) {
    commit_text();
    const auto path = target.empty() ? dialog(true, L"Imagen PNG\0*.png\0\0", L"png") : target;
    if (path.empty() || !update_cache())
        return;
    auto *context = surface_.context();
    ComPtr<ID2D1Bitmap1> cpu;
    const auto size = cache_->GetPixelSize();
    const auto properties =
        D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    if (FAILED(context->CreateBitmap(size, nullptr, 0, properties, cpu.GetAddressOf())) ||
        FAILED(cpu->CopyFromBitmap(nullptr, cache_.Get(), nullptr))) {
        error("No se pudo preparar la exportación.");
        return;
    }
    D2D1_MAPPED_RECT mapped{};
    if (FAILED(cpu->Map(D2D1_MAP_OPTIONS_READ, &mapped))) {
        error("No se pudo leer la vista.");
        return;
    }
    auto wic = board_ui::factory();
    ComPtr<IWICBitmap> bitmap;
    std::shared_ptr<const BoardImage> image;
    if (wic && SUCCEEDED(wic->CreateBitmapFromMemory(size.width, size.height, GUID_WICPixelFormat32bppPBGRA,
                                                     mapped.pitch, mapped.pitch * size.height, mapped.bits,
                                                     bitmap.GetAddressOf())))
        image = board_ui::encode_image(bitmap.Get());
    cpu->Unmap();
    if (!image) {
        error("No se pudo codificar el PNG.");
        return;
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char *>(image->png.data()),
              static_cast<std::streamsize>(image->png.size()));
    out.close();
    if (!out)
        error("No se pudo guardar el PNG.");
    else {
        status_ = L"Vista exportada como PNG";
        invalidate();
    }
}

void BoardWindow::context_menu(PointF p) {
    const auto hit = document_.hit(camera_.to_world(p), 6 / camera_.zoom);
    if (hit && std::find(selected_.begin(), selected_.end(), hit->id) == selected_.end())
        selected_ = {hit->id};
    if (selected_.empty())
        return;
    const int id = menu({{1, L"Duplicar\tCtrl+D"},
                         {2, L"Traer al frente"},
                         {3, L"Enviar al fondo"},
                         {4, L"Bloquear"},
                         {5, L"Desbloquear"},
                         {6, L"Eliminar\tSupr"}});
    if (id == 1)
        document_.duplicate(selected_);
    else if (id == 2 || id == 3)
        document_.reorder(selected_, id == 2);
    else if (id == 4 || id == 5)
        document_.lock(selected_, id == 4);
    else if (id == 6) {
        document_.remove(selected_);
        selected_.clear();
    }
    if (id)
        changed();
    invalidate();
}
void BoardWindow::begin(PointF view, float pressure, bool erase, bool pan) {
    commit_text();
    SetFocus(window_);
    if (view.y < 0)
        return;
    start_ = last_ = current_ = view;
    const auto world = camera_.to_world(view);
    if (pan || mode_ == Mode::Hand || board_ui::down(VK_SPACE)) {
        drag_ = Drag::Pan;
        SetCapture(window_);
        return;
    }
    if (erase || (mode_ == Mode::Ink && tool_ == Tool::Eraser)) {
        drag_ = Drag::Erase;
        document_.begin_compound();
        if (document_.erase(world, 12 / camera_.zoom))
            changed();
        SetCapture(window_);
        return;
    }
    if (mode_ == Mode::Select) {
        selection_start_ = document_.selection_bounds(selected_);
        selection_target_ = selection_start_;
        const auto corner = camera_.to_view({selection_start_.right, selection_start_.bottom});
        if (!selected_.empty() && distance(view, corner) < 12) {
            drag_ = Drag::Resize;
            SetCapture(window_);
            return;
        }
        const auto hit = document_.hit(world, 6 / camera_.zoom);
        if (hit && !hit->locked) {
            const auto found = std::find(selected_.begin(), selected_.end(), hit->id);
            if (board_ui::down(VK_SHIFT)) {
                if (found == selected_.end())
                    selected_.push_back(hit->id);
                else
                    selected_.erase(found);
            } else if (found == selected_.end())
                selected_ = {hit->id};
            selection_start_ = document_.selection_bounds(selected_);
            selection_target_ = selection_start_;
            drag_ = Drag::Move;
        } else {
            if (!board_ui::down(VK_SHIFT))
                selected_.clear();
            drag_ = Drag::Area;
        }
        SetCapture(window_);
        invalidate();
        return;
    }
    if (tool_ == Tool::Text) {
        text(world);
        return;
    }
    BoardObject object;
    object.ink.kind = gesture_tool(tool_, board_ui::down(VK_SHIFT), board_ui::down(VK_CONTROL), false,
                                   board_ui::down(VK_TAB));
    object.ink.color = color_;
    object.ink.width = thickness_;
    object.ink.points = {world};
    if (tool_ == Tool::Emoji)
        object.ink.text = kEmojis[static_cast<std::size_t>(controller_.preferences().selected_emoji)].glyph;
    if (uses_bounding_pair(object.ink.kind))
        object.ink.points.push_back(world);
    else if (pressure_)
        object.pressures.push_back(std::clamp(pressure, 0.0F, 1.0F));
    preview_ = std::move(object);
    drag_ = Drag::Ink;
    selected_.clear();
    SetCapture(window_);
    invalidate();
}
void BoardWindow::move(PointF view, float pressure) {
    current_ = view;
    if (drag_ == Drag::None)
        return;
    const auto world = camera_.to_world(view);
    if (drag_ == Drag::Pan) {
        camera_.pan({view.x - last_.x, view.y - last_.y});
        changed();
    } else if (drag_ == Drag::Erase) {
        if (document_.erase(world, 12 / camera_.zoom))
            changed();
    } else if (drag_ == Drag::Move) {
        const float dx = (view.x - start_.x) / camera_.zoom, dy = (view.y - start_.y) / camera_.zoom;
        selection_target_ = {selection_start_.left + dx, selection_start_.top + dy,
                             selection_start_.right + dx, selection_start_.bottom + dy};
    } else if (drag_ == Drag::Resize) {
        const auto first = camera_.to_world(start_);
        const float width = std::max(1.0F, selection_start_.width());
        const float factor = std::clamp((width + world.x - first.x) / width, .02F, 100.0F);
        selection_target_ = {selection_start_.left, selection_start_.top,
                             selection_start_.left + selection_start_.width() * factor,
                             selection_start_.top + selection_start_.height() * factor};
    } else if (drag_ == Drag::Ink && preview_) {
        auto &ink = preview_->ink;
        if (uses_bounding_pair(ink.kind)) {
            auto p = world;
            if (board_ui::down(VK_SHIFT) && supports_equal_axes(ink.kind)) {
                const float dx = p.x - ink.points.front().x, dy = p.y - ink.points.front().y,
                            side = std::max(std::abs(dx), std::abs(dy));
                p = {ink.points.front().x + std::copysign(side, dx),
                     ink.points.front().y + std::copysign(side, dy)};
            }
            ink.points.back() = p;
        } else if (distance(ink.points.back(), world) * camera_.zoom >= .35F && ink.points.size() < 200000) {
            ink.points.push_back(world);
            if (!preview_->pressures.empty())
                preview_->pressures.push_back(std::clamp(pressure, 0.0F, 1.0F));
        }
        ink.invalidate_bounds_cache();
    }
    last_ = view;
    invalidate();
}
void BoardWindow::finish(PointF view, float pressure) {
    move(view, pressure);
    if (drag_ == Drag::Ink && preview_) {
        auto &ink = preview_->ink;
        if (ink.kind == Tool::Emoji && distance(ink.points.front(), ink.points.back()) < 5 / camera_.zoom)
            ink.points.back() = {ink.points.front().x + 72 / camera_.zoom,
                                 ink.points.front().y + 72 / camera_.zoom};
        if (document_.add(std::move(*preview_)))
            changed();
        else
            status_ = L"No se pudo añadir el trazo";
    } else if (drag_ == Drag::Move || drag_ == Drag::Resize) {
        if (distance(start_, view) > 1 && document_.transform(selected_, selection_start_, selection_target_))
            changed();
    } else if (drag_ == Drag::Area) {
        const auto ids = document_.in_area(board_ui::box(camera_.to_world(start_), camera_.to_world(view)));
        for (auto id : ids)
            if (std::find(selected_.begin(), selected_.end(), id) == selected_.end())
                selected_.push_back(id);
    }
    document_.end_compound();
    drag_ = Drag::None;
    preview_.reset();
    pointer_ = false;
    pointer_controls_ = false;
    if (GetCapture() == window_)
        ReleaseCapture();
    invalidate();
}
void BoardWindow::cancel() {
    if (drag_ == Drag::Erase)
        document_.end_compound();
    drag_ = Drag::None;
    pointer_ = false;
    pointer_controls_ = false;
    preview_.reset();
    if (GetCapture() == window_)
        ReleaseCapture();
    invalidate();
}
void BoardWindow::pen_samples(WPARAM wparam, bool end) {
    if (!pointer_ || GET_POINTERID_WPARAM(wparam) != pointer_id_)
        return;
    if (pointer_controls_) {
        if (end) {
            pointer_ = false;
            pointer_controls_ = false;
        }
        return;
    }
    UINT32 count{};
    GetPointerPenInfoHistory(pointer_id_, &count, nullptr);
    count = std::min<UINT32>(count, 1024);
    std::vector<POINTER_PEN_INFO> samples(count);
    if (count && GetPointerPenInfoHistory(pointer_id_, &count, samples.data())) {
        for (std::size_t n = count; n > 0; --n) {
            const auto &s = samples[n - 1];
            POINT p = s.pointerInfo.ptPixelLocation;
            ScreenToClient(window_, &p);
            move({static_cast<float>(p.x), static_cast<float>(p.y) - toolbar_height_},
                 (s.penMask & PEN_MASK_PRESSURE) ? static_cast<float>(s.pressure) / 1024 : 1);
        }
    } else {
        POINTER_PEN_INFO sample{};
        if (GetPointerPenInfo(pointer_id_, &sample)) {
            POINT p = sample.pointerInfo.ptPixelLocation;
            ScreenToClient(window_, &p);
            move({static_cast<float>(p.x), static_cast<float>(p.y) - toolbar_height_},
                 (sample.penMask & PEN_MASK_PRESSURE) ? static_cast<float>(sample.pressure) / 1024 : 1);
        }
    }
    if (end)
        finish(current_, preview_ && !preview_->pressures.empty() ? preview_->pressures.back() : 1);
}

void BoardWindow::text(PointF p) {
    text_position_ = p;
    const auto v = camera_.to_view(p);
    edit_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
        static_cast<int>(v.x), static_cast<int>(v.y + toolbar_height_),
        std::min(500, std::max(150, static_cast<int>(static_cast<float>(surface_.width()) - v.x))), 130,
        window_, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (edit_) {
        SendMessageW(edit_, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SendMessageW(edit_, EM_SETLIMITTEXT, 100000, 0);
        SetWindowSubclass(edit_, edit_proc, 1, reinterpret_cast<DWORD_PTR>(this));
        SetFocus(edit_);
        status_ = L"Texto · Ctrl+Enter: insertar · Esc: cancelar";
        invalidate();
    }
}
void BoardWindow::commit_text() {
    if (!edit_)
        return;
    const int length = GetWindowTextLengthW(edit_);
    std::wstring value(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(edit_, value.data(), length + 1);
    value.resize(static_cast<std::size_t>(length));
    DestroyWindow(edit_);
    edit_ = nullptr;
    if (!value.empty()) {
        BoardObject object;
        object.ink.kind = Tool::Text;
        object.ink.color = color_;
        object.ink.width = thickness_;
        object.ink.points = {text_position_};
        object.ink.text = std::move(value);
        if (document_.add(std::move(object)))
            changed();
    }
    SetFocus(window_);
}
LRESULT CALLBACK BoardWindow::edit_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR,
                                        DWORD_PTR data) {
    auto *board = reinterpret_cast<BoardWindow *>(data);
    if (message == WM_KEYDOWN && wparam == VK_RETURN && board_ui::down(VK_CONTROL)) {
        board->commit_text();
        return 0;
    }
    if (message == WM_KEYDOWN && wparam == VK_ESCAPE) {
        DestroyWindow(window);
        board->edit_ = nullptr;
        SetFocus(board->hwnd());
        return 0;
    }
    return DefSubclassProc(window, message, wparam, lparam);
}
void BoardWindow::toggle_fullscreen() {
    commit_text();
    if (!fullscreen_) {
        GetWindowRect(window_, &normal_bounds_);
        normal_style_ = static_cast<DWORD>(GetWindowLongPtrW(window_, GWL_STYLE));
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        GetMonitorInfoW(MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST), &info);
        SetWindowLongPtrW(window_, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(window_, HWND_TOP, info.rcMonitor.left, info.rcMonitor.top,
                     info.rcMonitor.right - info.rcMonitor.left, info.rcMonitor.bottom - info.rcMonitor.top,
                     SWP_FRAMECHANGED);
    } else {
        SetWindowLongPtrW(window_, GWL_STYLE, normal_style_);
        SetWindowPos(window_, HWND_NOTOPMOST, normal_bounds_.left, normal_bounds_.top,
                     normal_bounds_.right - normal_bounds_.left, normal_bounds_.bottom - normal_bounds_.top,
                     SWP_FRAMECHANGED);
    }
    fullscreen_ = !fullscreen_;
    invalidate();
}

bool BoardWindow::execute(HotkeyAction a) {
    if (capturing_)
        return true;
    if (a >= HotkeyAction::EmojiFavorite1 && a <= HotkeyAction::EmojiFavorite8) {
        const int id =
            controller_.preferences().emoji_favorites[static_cast<std::size_t>(a) -
                                                      static_cast<std::size_t>(HotkeyAction::EmojiFavorite1)];
        if (valid_emoji(id)) {
            controller_.preferences().selected_emoji = id;
            set_tool(Tool::Emoji);
        }
        return true;
    }
    switch (a) {
    case HotkeyAction::Interact:
        mode_ = mode_ == Mode::Ink ? Mode::Select : Mode::Ink;
        break;
    case HotkeyAction::Whiteboard:
        if (dark_)
            command(13);
        break;
    case HotkeyAction::Blackboard:
        if (!dark_)
            command(13);
        break;
    case HotkeyAction::Undo:
        command(11);
        break;
    case HotkeyAction::Redo:
        command(12);
        break;
    case HotkeyAction::Clear:
        if (MessageBoxW(window_, L"¿Limpiar la pizarra? Puedes deshacer.", L"Elite Pen", MB_YESNO) == IDYES) {
            document_.clear();
            changed();
        }
        break;
    case HotkeyAction::Pen:
        set_tool(Tool::Pen);
        break;
    case HotkeyAction::Highlighter:
        set_tool(Tool::Highlighter);
        break;
    case HotkeyAction::Eraser:
        set_tool(Tool::Eraser);
        break;
    case HotkeyAction::Line:
        set_tool(Tool::Line);
        break;
    case HotkeyAction::Rectangle:
        set_tool(Tool::Rectangle);
        break;
    case HotkeyAction::Ellipse:
        set_tool(Tool::Ellipse);
        break;
    case HotkeyAction::Arrow:
        set_tool(Tool::Arrow);
        break;
    case HotkeyAction::CurvedArrow:
        set_tool(Tool::CurvedArrow);
        break;
    case HotkeyAction::Pentagon:
        set_tool(Tool::Pentagon);
        break;
    case HotkeyAction::Hexagon:
        set_tool(Tool::Hexagon);
        break;
    case HotkeyAction::Cross:
        set_tool(Tool::Cross);
        break;
    case HotkeyAction::Checkmark:
        set_tool(Tool::Checkmark);
        break;
    case HotkeyAction::Warning:
        set_tool(Tool::Warning);
        break;
    case HotkeyAction::Prohibition:
        set_tool(Tool::Prohibition);
        break;
    case HotkeyAction::Text:
        set_tool(Tool::Text);
        break;
    case HotkeyAction::Screenshot:
        begin_capture();
        break;
    case HotkeyAction::GeometryPanel:
        shape_menu();
        break;
    case HotkeyAction::EmojiPanel:
        emoji_menu();
        break;
    case HotkeyAction::ToolPanel:
    case HotkeyAction::PaletteCollapse:
        toolbar_ = !toolbar_;
        break;
    case HotkeyAction::ColorPanel:
    case HotkeyAction::ColorPanelAlternate:
        command(15);
        break;
    case HotkeyAction::ColorBlack:
        color_ = kBlack;
        break;
    case HotkeyAction::ColorYellow:
        color_ = kYellow;
        break;
    case HotkeyAction::ColorBlue:
        color_ = kBlue;
        break;
    case HotkeyAction::ColorRed:
        color_ = kRed;
        break;
    case HotkeyAction::ColorGreen:
        color_ = kGreen;
        break;
    case HotkeyAction::ColorPurple:
        color_ = kPurple;
        break;
    case HotkeyAction::Visibility:
        toolbar_ = !toolbar_;
        break;
    default:
        return false;
    }
    invalidate();
    return true;
}

void BoardWindow::draw_object(ID2D1DeviceContext *context, DrawableRenderResources &resources,
                              const BoardObject &object) {
    if (object.image) {
        const auto found = std::find_if(images_.begin(), images_.end(),
                                        [&](const auto &item) { return item.asset == object.image; });
        ID2D1Bitmap1 *bitmap{};
        if (found != images_.end())
            bitmap = found->bitmap.Get();
        else {
            auto source = board_ui::decode_image(*object.image);
            ImageCache item;
            item.asset = object.image;
            ComPtr<IWICBitmapScaler> scaled;
            IWICBitmapSource *pixels = source.Get();
            const float factor = std::min(
                1.0F, 4096.0F / static_cast<float>(std::max(object.image->width, object.image->height)));
            const UINT width =
                std::max(1U, static_cast<UINT>(static_cast<float>(object.image->width) * factor));
            const UINT height =
                std::max(1U, static_cast<UINT>(static_cast<float>(object.image->height) * factor));
            const std::size_t bytes = static_cast<std::size_t>(width) * height * 4;
            if (image_bytes_ + bytes > 128U * 1024U * 1024U || images_.size() >= 64) {
                images_.clear();
                image_bytes_ = 0;
            }
            auto wic = board_ui::factory();
            if (source && factor < 1) {
                if (wic && SUCCEEDED(wic->CreateBitmapScaler(scaled.GetAddressOf())) &&
                    SUCCEEDED(
                        scaled->Initialize(source.Get(), width, height, WICBitmapInterpolationModeFant)))
                    pixels = scaled.Get();
                else
                    pixels = nullptr;
            }
            if (pixels &&
                SUCCEEDED(context->CreateBitmapFromWicBitmap(pixels, nullptr, item.bitmap.GetAddressOf()))) {
                image_bytes_ += bytes;
                images_.push_back(std::move(item));
                bitmap = images_.back().bitmap.Get();
            }
        }
        if (bitmap) {
            const auto a = camera_.to_view({object.image_bounds.left, object.image_bounds.top}),
                       b = camera_.to_view({object.image_bounds.right, object.image_bounds.bottom});
            context->DrawBitmap(bitmap, D2D1::RectF(a.x, a.y, b.x, b.y), 1, D2D1_INTERPOLATION_MODE_LINEAR);
        }
        return;
    }
    const auto &ink = object.ink;
    if ((ink.kind == Tool::Pen || ink.kind == Tool::Highlighter) && !object.pressures.empty()) {
        resources.brush->SetColor(d2d_color(ink.color, ink.kind == Tool::Highlighter ? .35F : 1));
        if (ink.points.size() == 1) {
            const auto p = camera_.to_view(ink.points.front());
            const float radius =
                ink.width * camera_.zoom * board_pressure_width(object.pressures.front()) * .5F;
            context->FillEllipse(D2D1::Ellipse(D2D1::Point2F(p.x, p.y), radius, radius),
                                 resources.brush.Get());
        }
        for (std::size_t n = 1; n < ink.points.size(); ++n) {
            const auto a = camera_.to_view(ink.points[n - 1]), b = camera_.to_view(ink.points[n]);
            const float w = ink.width * camera_.zoom *
                            board_pressure_width((object.pressures[n - 1] + object.pressures[n]) * .5F);
            context->DrawLine(D2D1::Point2F(a.x, a.y), D2D1::Point2F(b.x, b.y), resources.brush.Get(), w,
                              resources.stroke_style.Get());
        }
    } else
        draw_drawable(controller_.graphics(), context, resources, ink, camera_.origin.x, camera_.origin.y, 1,
                      camera_.zoom);
}
bool BoardWindow::update_cache() {
    auto *context = surface_.context();
    if (!context)
        return false;
    const auto size = viewport();
    const UINT width = surface_.width(), height = static_cast<UINT>(size.y);
    if (!width || !height)
        return false;
    const bool same = cache_ && cache_generation_ == surface_.generation() &&
                      cache_->GetPixelSize().width == width && cache_->GetPixelSize().height == height &&
                      cache_camera_.origin.x == camera_.origin.x &&
                      cache_camera_.origin.y == camera_.origin.y && cache_camera_.zoom == camera_.zoom &&
                      cache_dark_ == dark_ && cache_grid_ == grid_;
    if (same && cache_revision_ == document_.revision())
        return true;
    if (!same) {
        cache_.Reset();
        if (cache_generation_ != surface_.generation()) {
            images_.clear();
            image_bytes_ = 0;
        }
        const auto properties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        if (FAILED(context->CreateBitmap(D2D1::SizeU(width, height), nullptr, 0, properties,
                                         cache_.GetAddressOf())))
            return false;
    }
    ComPtr<ID2D1Image> target;
    context->GetTarget(target.GetAddressOf());
    context->SetTarget(cache_.Get());
    context->BeginDraw();
    context->SetTransform(D2D1::Matrix3x2F::Identity());
    DrawableRenderResources resources;
    if (!resources.initialize(controller_.graphics(), context)) {
        context->EndDraw();
        context->SetTarget(target.Get());
        return false;
    }
    const bool append = same && document_.append_only() && cache_revision_ + 1 == document_.revision() &&
                        cache_count_ + 1 == document_.objects().size();
    if (!append) {
        context->Clear(D2D1::ColorF(dark_ ? 0x11141B : 0xFFFFFF));
        if (grid_) {
            resources.brush->SetColor(D2D1::ColorF(dark_ ? 0x303440 : 0xE6E9EF));
            float step = 40 * camera_.zoom;
            while (step < 16)
                step *= 4;
            while (step > 160)
                step /= 4;
            for (float x = std::fmod(-camera_.origin.x * camera_.zoom, step); x < size.x; x += step)
                context->DrawLine(D2D1::Point2F(x, 0), D2D1::Point2F(x, size.y), resources.brush.Get(), 1);
            for (float y = std::fmod(-camera_.origin.y * camera_.zoom, step); y < size.y; y += step)
                context->DrawLine(D2D1::Point2F(0, y), D2D1::Point2F(size.x, y), resources.brush.Get(), 1);
        }
    }
    const auto top = camera_.to_world({0, 0}), bottom = camera_.to_world(size);
    const RectF visible{top.x, top.y, bottom.x, bottom.y};
    for (std::size_t n = append ? cache_count_ : 0; n < document_.objects().size(); ++n)
        if (document_.objects()[n]->bounds().intersects(visible))
            draw_object(context, resources, *document_.objects()[n]);
    const HRESULT hr = context->EndDraw();
    context->SetTarget(target.Get());
    if (FAILED(hr)) {
        cache_.Reset();
        return false;
    }
    cache_generation_ = surface_.generation();
    cache_revision_ = document_.revision();
    cache_count_ = document_.objects().size();
    cache_camera_ = camera_;
    cache_dark_ = dark_;
    cache_grid_ = grid_;
    return true;
}
void BoardWindow::render() {
    if (!surface_.context())
        return;
    layout();
    const bool cached = update_cache();
    auto *context = surface_.begin_draw(theme_color(current_ui_theme().surface_1));
    if (!context)
        return;
    DrawableRenderResources resources;
    if (!resources.initialize(controller_.graphics(), context)) {
        std::wstring error;
        surface_.end_draw(error);
        return;
    }
    context->SetTransform(D2D1::Matrix3x2F::Translation(0, toolbar_height_));
    context->PushAxisAlignedClip(D2D1::RectF(0, 0, viewport().x, viewport().y), D2D1_ANTIALIAS_MODE_ALIASED);
    if (cached)
        context->DrawBitmap(cache_.Get());
    else {
        resources.brush->SetColor(D2D1::ColorF(dark_ ? 0x11141B : 0xFFFFFF));
        context->FillRectangle(D2D1::RectF(0, 0, viewport().x, viewport().y), resources.brush.Get());
        const auto a = camera_.to_world({0, 0}), b = camera_.to_world(viewport());
        for (const auto &object : document_.objects())
            if (object->bounds().intersects({a.x, a.y, b.x, b.y}))
                draw_object(context, resources, *object);
    }
    if (preview_)
        draw_object(context, resources, *preview_);
    if ((drag_ == Drag::Move || drag_ == Drag::Resize) && distance(start_, current_) > 1) {
        for (const auto &object : document_.objects())
            if (std::find(selected_.begin(), selected_.end(), object->id) != selected_.end())
                draw_object(context, resources,
                            transform_board_object(*object, selection_start_, selection_target_));
    }
    if (!selected_.empty()) {
        const auto r = (drag_ == Drag::Move || drag_ == Drag::Resize) ? selection_target_
                                                                      : document_.selection_bounds(selected_);
        const auto a = camera_.to_view({r.left, r.top}), b = camera_.to_view({r.right, r.bottom});
        resources.brush->SetColor(theme_color(current_ui_theme().violet));
        context->DrawRectangle(D2D1::RectF(a.x, a.y, b.x, b.y), resources.brush.Get(), 2);
        context->FillRectangle(D2D1::RectF(b.x - 6, b.y - 6, b.x + 6, b.y + 6), resources.brush.Get());
    }
    if (drag_ == Drag::Area) {
        resources.brush->SetColor(theme_color(current_ui_theme().violet, .14F));
        context->FillRectangle(board_ui::rect(board_ui::box(start_, current_)), resources.brush.Get());
        resources.brush->SetColor(theme_color(current_ui_theme().violet));
        context->DrawRectangle(board_ui::rect(board_ui::box(start_, current_)), resources.brush.Get(), 1.5F);
    }
    context->PopAxisAlignedClip();
    context->SetTransform(D2D1::Matrix3x2F::Identity());
    const auto &theme = current_ui_theme();
    resources.brush->SetColor(theme_color(theme.surface_1));
    context->FillRectangle(D2D1::RectF(0, 0, static_cast<float>(surface_.width()), toolbar_height_),
                           resources.brush.Get());
    static constexpr std::array<Color, 8> colors{
        {kBlack, {255, 255, 255, 255}, kYellow, kBlue, kRed, kGreen, kPurple, {249, 115, 22, 255}}};
    context->PushAxisAlignedClip(D2D1::RectF(0, 0, static_cast<float>(surface_.width()), toolbar_height_),
                                 D2D1_ANTIALIAS_MODE_ALIASED);
    for (const auto &b : buttons_) {
        const bool active = (b.command == 2 && mode_ == Mode::Ink && tool_ == Tool::Pen) ||
                            (b.command == 3 && mode_ == Mode::Ink && tool_ == Tool::Highlighter) ||
                            (b.command == 4 && mode_ == Mode::Ink && tool_ == Tool::Eraser) ||
                            (b.command == 5 && mode_ == Mode::Select) ||
                            (b.command == 6 && mode_ == Mode::Hand);
        resources.brush->SetColor(theme_color(active ? theme.violet_strong : theme.surface_3));
        if (b.command >= 100)
            resources.brush->SetColor(d2d_color(colors[static_cast<std::size_t>(b.command - 100)]));
        const auto r = board_ui::rect(b.bounds);
        context->FillRoundedRectangle(D2D1::RoundedRect(r, 5, 5), resources.brush.Get());
        resources.brush->SetColor(theme_color(theme.line));
        context->DrawRoundedRectangle(D2D1::RoundedRect(r, 5, 5), resources.brush.Get(), 1);
        resources.brush->SetColor(theme_color(active ? 0xFFFFFF : theme.text));
        auto *format =
            resources.text_format(controller_.graphics(), 12.5F * static_cast<float>(scale_) / 100);
        if (format) {
            format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            context->DrawTextW(b.label.c_str(), static_cast<UINT32>(b.label.size()), format, r,
                               resources.brush.Get());
        }
    }
    context->PopAxisAlignedClip();
    const float bottom = static_cast<float>(surface_.height());
    resources.brush->SetColor(theme_color(theme.surface_2));
    context->FillRectangle(D2D1::RectF(0, bottom - 28, static_cast<float>(surface_.width()), bottom),
                           resources.brush.Get());
    resources.brush->SetColor(theme_color(theme.text_soft));
    const std::wstring description = std::to_wstring(static_cast<int>(camera_.zoom * 100)) + L" % · " +
                                     std::to_wstring(document_.objects().size()) + L" objetos · " +
                                     tool_name(tool_) + L" " + std::to_wstring(static_cast<int>(thickness_)) +
                                     L" px · " + status_;
    auto *format = resources.text_format(controller_.graphics(), 12);
    if (format) {
        format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        context->DrawTextW(
            description.c_str(), static_cast<UINT32>(description.size()), format,
            D2D1::RectF(10, bottom - 26, static_cast<float>(surface_.width()) - 10, bottom - 1),
            resources.brush.Get());
    }
    std::wstring message;
    surface_.end_draw(message);
}

LRESULT BoardWindow::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CLOSE:
        hide();
        return 0;
    case WM_QUERYENDSESSION:
        commit_text();
        cancel();
        return flush() ? TRUE : FALSE;
    case WM_SIZE:
        WindowBase::handle_message(message, wparam, lparam);
        if (wparam == SIZE_MINIMIZED) {
            commit_text();
            cancel();
            flush();
        }
        controller_.sync_board_visibility();
        return 0;
    case WM_SHOWWINDOW:
        invalidate();
        break;
    case WM_GETMINMAXINFO: {
        auto *info = reinterpret_cast<MINMAXINFO *>(lparam);
        info->ptMinTrackSize = {640, 480};
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_TIMER:
        if (wparam == board_ui::kSaveTimer) {
            autosave();
            return 0;
        }
        if (wparam == board_ui::kCaptureTimer) {
            capture_pixels();
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        // Compatibility drivers can deliver their entire pen contact as mouse
        // messages. Discard only duplicates of a native contact we actually own.
        if (pointer_)
            return 0;
        layout();
        if (static_cast<float>(GET_Y_LPARAM(lparam)) < toolbar_height_) {
            for (const auto &b : buttons_)
                if (b.bounds.contains({static_cast<float>(GET_X_LPARAM(lparam)),
                                       static_cast<float>(GET_Y_LPARAM(lparam))})) {
                    command(b.command);
                    break;
                }
            return 0;
        }
        begin(view_point(lparam), 1);
        return 0;
    case WM_MBUTTONDOWN:
        begin(view_point(lparam), 1, false, true);
        return 0;
    case WM_MOUSEMOVE:
        if (!pointer_)
            move(view_point(lparam), 1);
        return 0;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
        if (!pointer_ && drag_ != Drag::None)
            finish(view_point(lparam), 1);
        return 0;
    case WM_RBUTTONUP:
        if (drag_ == Drag::None)
            context_menu(view_point(lparam));
        return 0;
    case WM_POINTERDOWN: {
        ++pointer_events_[0];
        POINTER_PEN_INFO pen{};
        if (!GetPointerPenInfo(GET_POINTERID_WPARAM(wparam), &pen)) {
            pointer_events_[4] = GetLastError();
            break;
        }
        POINT p = pen.pointerInfo.ptPixelLocation;
        ScreenToClient(window_, &p);
        layout();
        pointer_id_ = GET_POINTERID_WPARAM(wparam);
        pointer_ = true;
        pointer_controls_ = static_cast<float>(p.y) < toolbar_height_;
        if (pointer_controls_) {
            for (const auto &b : buttons_)
                if (b.bounds.contains({static_cast<float>(p.x), static_cast<float>(p.y)})) {
                    command(b.command);
                    break;
                }
            return 0;
        }
        begin({static_cast<float>(p.x), static_cast<float>(p.y) - toolbar_height_},
              (pen.penMask & PEN_MASK_PRESSURE) ? static_cast<float>(pen.pressure) / 1024 : 1,
              (pen.penFlags & PEN_FLAG_ERASER) != 0, (pen.penFlags & PEN_FLAG_BARREL) != 0);
        return 0;
    }
    case WM_POINTERUPDATE:
        ++pointer_events_[1];
        if (!pointer_ || GET_POINTERID_WPARAM(wparam) != pointer_id_)
            break; // Leave unclaimed pointer sequences to Windows' mouse fallback.
        pen_samples(wparam, false);
        return 0;
    case WM_POINTERUP:
        ++pointer_events_[2];
        if (!pointer_ || GET_POINTERID_WPARAM(wparam) != pointer_id_)
            break;
        pen_samples(wparam, true);
        return 0;
    case WM_POINTERCAPTURECHANGED:
        ++pointer_events_[3];
        if (!pointer_ || GET_POINTERID_WPARAM(wparam) != pointer_id_)
            break;
        if (drag_ == Drag::Ink || drag_ == Drag::Erase)
            finish(current_, preview_ && !preview_->pressures.empty() ? preview_->pressures.back() : 1);
        else
            cancel();
        return 0;
    case WM_CANCELMODE:
        cancel();
        return 0;
    case WM_CAPTURECHANGED:
        // WM_POINTERCAPTURECHANGED owns pen cancellation; mouse capture is separate.
        if (!pointer_ && drag_ != Drag::None)
            cancel();
        return 0;
    case WM_MOUSEWHEEL: {
        POINT p{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        ScreenToClient(window_, &p);
        const float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / WHEEL_DELTA;
        if (static_cast<float>(p.y) < toolbar_height_) {
            toolbar_scroll_ -= static_cast<int>(delta * 60);
            invalidate();
            return 0;
        }
        if (drag_ != Drag::None)
            return 0;
        commit_text();
        if (board_ui::down(VK_SHIFT))
            camera_.pan({delta * 80, 0});
        else
            camera_.zoom_at({static_cast<float>(p.x), static_cast<float>(p.y) - toolbar_height_},
                            std::pow(1.2F, delta));
        changed();
        return 0;
    }
    case WM_KEYDOWN: {
        const bool ctrl = board_ui::down(VK_CONTROL), shift = board_ui::down(VK_SHIFT);
        if (wparam == VK_ESCAPE) {
            if (drag_ != Drag::None)
                cancel();
            else if (!selected_.empty()) {
                selected_.clear();
                invalidate();
            } else
                hide();
            return 0;
        }
        if (wparam == VK_F11) {
            toggle_fullscreen();
            return 0;
        }
        if (wparam == VK_TAB) {
            toolbar_ = !toolbar_;
            changed();
            return 0;
        }
        if (ctrl) {
            switch (wparam) {
            case 'S':
                save(shift);
                return 0;
            case 'O':
                open();
                return 0;
            case 'N':
                new_board();
                return 0;
            case 'V':
                paste();
                return 0;
            case 'Z':
                command(shift ? 12 : 11);
                return 0;
            case 'Y':
                command(12);
                return 0;
            case 'A':
                selected_.clear();
                for (const auto &object : document_.objects())
                    if (!object->locked)
                        selected_.push_back(object->id);
                mode_ = Mode::Select;
                invalidate();
                return 0;
            case 'D':
                if (document_.duplicate(selected_))
                    changed();
                return 0;
            default:
                break;
            }
        } else {
            switch (wparam) {
            case VK_DELETE:
                if (document_.remove(selected_)) {
                    selected_.clear();
                    changed();
                }
                return 0;
            case 'P':
                set_tool(Tool::Pen);
                return 0;
            case 'E':
                set_tool(Tool::Eraser);
                return 0;
            case 'V':
                mode_ = Mode::Select;
                invalidate();
                return 0;
            case 'H':
                mode_ = Mode::Hand;
                invalidate();
                return 0;
            case '0':
                command(20);
                return 0;
            case VK_OEM_PLUS:
                command(19);
                return 0;
            case VK_OEM_MINUS:
                command(18);
                return 0;
            default:
                break;
            }
        }
        break;
    }
    case WM_DROPFILES: {
        const auto drop = reinterpret_cast<HDROP>(wparam);
        wchar_t path[32768]{};
        DragQueryFileW(drop, 0, path, 32768);
        DragFinish(drop);
        const std::filesystem::path file(path);
        if (file.extension() == L".epboard") {
            if (archive() && load(file))
                flush();
        } else {
            auto wic = board_ui::factory();
            ComPtr<IWICBitmapDecoder> decoder;
            ComPtr<IWICBitmapFrameDecode> frame;
            if (wic &&
                SUCCEEDED(wic->CreateDecoderFromFilename(
                    path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf())) &&
                SUCCEEDED(decoder->GetFrame(0, frame.GetAddressOf())))
                insert_image(board_ui::encode_image(frame.Get()));
            else
                error("No se pudo abrir el archivo arrastrado.");
        }
        return 0;
    }
    default:
        break;
    }
    // Only isolated QA processes opt into test messages; normal user sessions ignore them.
    wchar_t enabled[2]{};
    if (message >= board_ui::kQa && message < board_ui::kQa + 50 &&
        GetEnvironmentVariableW(L"ELITE_PEN_BOARD_QA", enabled, 2) == 1) {
        switch (message - board_ui::kQa) {
        case 0:
            return static_cast<LRESULT>(document_.objects().size());
        case 1:
            return static_cast<LRESULT>(std::lround(camera_.zoom * 1000));
        case 2:
            command(static_cast<int>(wparam));
            return 1;
        case 3:
            begin({static_cast<float>(LOWORD(lparam)), static_cast<float>(HIWORD(lparam))},
                  static_cast<float>(wparam) / 1024);
            return 1;
        case 4:
            move({static_cast<float>(LOWORD(lparam)), static_cast<float>(HIWORD(lparam))},
                 static_cast<float>(wparam) / 1024);
            return 1;
        case 5:
            finish({static_cast<float>(LOWORD(lparam)), static_cast<float>(HIWORD(lparam))},
                   static_cast<float>(wparam) / 1024);
            return 1;
        case 6:
            return flush();
        case 7:
            return static_cast<LRESULT>(toolbar_height_);
        case 8:
            return document_.objects().empty()
                       ? 0
                       : static_cast<LRESULT>(document_.objects().back()->pressures.size());
        case 9:
            scale_ = static_cast<int>(wparam);
            changed();
            return 1;
        case 10:
            return dark_;
        case 11:
            command(13);
            return 1;
        case 12:
            new_board();
            return 1;
        case 13:
            begin_capture();
            return 1;
        case 14:
            filename_ = recovery_.parent_path() / L"qa-named.epboard";
            save();
            return 1;
        case 15:
            if (archive())
                load(recovery_.parent_path() / L"qa-named.epboard");
            return 1;
        case 16:
            paste();
            return 1;
        case 17:
            document_.undo();
            selected_.clear();
            changed();
            return 1;
        case 18:
            document_.redo();
            changed();
            return 1;
        case 19:
            return static_cast<LRESULT>(
                document_.objects().empty() ? 0 : document_.objects().back()->image != nullptr);
        case 20:
            export_png(recovery_.parent_path() / L"qa-view.png");
            return 1;
        case 21:
            return document_.objects().empty() || document_.objects().back()->pressures.empty()
                       ? 0
                       : static_cast<LRESULT>(document_.objects().back()->pressures.front() * 1024);
        case 22:
            return document_.objects().empty() || document_.objects().back()->pressures.empty()
                       ? 0
                       : static_cast<LRESULT>(document_.objects().back()->pressures.back() * 1024);
        case 23:
            return scale_;
        case 24:
            return static_cast<LRESULT>(selected_.size());
        case 25:
            return static_cast<LRESULT>(document_.selection_bounds(selected_).left * 100);
        case 26:
            return static_cast<LRESULT>(document_.selection_bounds(selected_).top * 100);
        case 27:
            return static_cast<LRESULT>(document_.selection_bounds(selected_).width() * 100);
        case 28:
            thickness_ = std::clamp(static_cast<float>(wparam), .5F, 100.0F);
            return 1;
        case 29:
            return IsWindowVisible(window_);
        case 30:
            return wparam < 6 ? pointer_events_[wparam] : 0;
        case 31:
            return static_cast<LRESULT>(
                std::lround(camera_.to_view({0, document_.selection_bounds(selected_).bottom}).y));
        case 32:
            return GetCapture() != nullptr;
        default:
            break;
        }
    }
    return WindowBase::handle_message(message, wparam, lparam);
}

bool BoardCaptureWindow::start() {
    desktop_ = {GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
                GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN),
                GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN)};
    if (!window_ &&
        (!create(L"ElitePenBoardCaptureWindow", L"Elite Pen · Seleccionar captura",
                 WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP, WS_POPUP, desktop_) ||
         !initialize_surface(controller_.graphics())))
        return false;
    SetWindowPos(window_, HWND_TOPMOST, desktop_.left, desktop_.top, desktop_.right - desktop_.left,
                 desktop_.bottom - desktop_.top, SWP_SHOWWINDOW);
    dragging_ = false;
    first_ = last_ = {};
    SetForegroundWindow(window_);
    SetFocus(window_);
    invalidate();
    return true;
}
LRESULT BoardCaptureWindow::handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, IDC_CROSS));
        return TRUE;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) {
            dragging_ = false;
            if (GetCapture() == window_)
                ReleaseCapture();
            ShowWindow(window_, SW_HIDE);
            owner_.finish_capture(std::nullopt);
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        first_ = last_ = {static_cast<float>(GET_X_LPARAM(lparam)), static_cast<float>(GET_Y_LPARAM(lparam))};
        dragging_ = true;
        SetCapture(window_);
        invalidate();
        return 0;
    case WM_MOUSEMOVE:
        if (dragging_) {
            last_ = {static_cast<float>(GET_X_LPARAM(lparam)), static_cast<float>(GET_Y_LPARAM(lparam))};
            invalidate();
        }
        return 0;
    case WM_LBUTTONUP:
        if (dragging_) {
            last_ = {static_cast<float>(GET_X_LPARAM(lparam)), static_cast<float>(GET_Y_LPARAM(lparam))};
            dragging_ = false;
            ReleaseCapture();
            ShowWindow(window_, SW_HIDE);
            auto area = rounded_rect(board_ui::box(first_, last_));
            OffsetRect(&area, desktop_.left, desktop_.top);
            DwmFlush();
            owner_.finish_capture(area.right - area.left >= 4 && area.bottom - area.top >= 4
                                      ? std::optional<RECT>(area)
                                      : std::nullopt);
        }
        return 0;
    case WM_CLOSE:
        ShowWindow(window_, SW_HIDE);
        owner_.finish_capture(std::nullopt);
        return 0;
    default:
        break;
    }
    return WindowBase::handle_message(message, wparam, lparam);
}
void BoardCaptureWindow::render() {
    auto *context = surface_.begin_draw(D2D1::ColorF(0, 0));
    if (!context)
        return;
    ComPtr<ID2D1SolidColorBrush> shade, accent;
    context->CreateSolidColorBrush(D2D1::ColorF(0, .3F), shade.GetAddressOf());
    context->CreateSolidColorBrush(D2D1::ColorF(.5F, .4F, 1), accent.GetAddressOf());
    const auto r = board_ui::box(first_, last_);
    const float w = static_cast<float>(surface_.width()), h = static_cast<float>(surface_.height());
    if (dragging_) {
        for (const auto box :
             {D2D1::RectF(0, 0, w, r.top), D2D1::RectF(0, r.bottom, w, h),
              D2D1::RectF(0, r.top, r.left, r.bottom), D2D1::RectF(r.right, r.top, w, r.bottom)})
            context->FillRectangle(box, shade.Get());
        context->DrawRectangle(board_ui::rect(r), accent.Get(), 2);
    } else
        context->FillRectangle(D2D1::RectF(0, 0, w, h), shade.Get());
    std::wstring error;
    surface_.end_draw(error);
}

bool Controller::board_active() const noexcept { return board_ && board_->active(); }
void Controller::open_board() {
    if (board_ && board_->active()) {
        board_->hide();
        return;
    }
    commit_pending_text();
    if (zoom_ && zoom_->active())
        zoom_->hide_zoom();
    for (const auto &overlay : overlays_)
        overlay->cancel_gesture();
    if (!board_) {
        board_ = std::make_unique<BoardWindow>(*this);
        if (!board_->initialize()) {
            board_.reset();
            report_runtime_error(L"No se pudo abrir la pizarra editable.");
            return;
        }
    }
    board_->show();
}
void Controller::sync_board_visibility() {
    if (shutting_down_)
        return;
    if (board_active()) {
        if (board_hidden_windows_.empty()) {
            const std::array<HWND, 7> windows{
                {palette_ ? palette_->hwnd() : nullptr, tools_ ? tools_->hwnd() : nullptr,
                 geometry_ ? geometry_->hwnd() : nullptr, emojis_ ? emojis_->hwnd() : nullptr,
                 colors_ ? colors_->hwnd() : nullptr, settings_ ? settings_->hwnd() : nullptr,
                 text_input_ ? text_input_->hwnd() : nullptr}};
            for (const auto window : windows)
                if (window && IsWindowVisible(window)) {
                    board_hidden_windows_.push_back(window);
                    ShowWindow(window, SW_HIDE);
                }
        }
        for (const auto &overlay : overlays_)
            ShowWindow(overlay->hwnd(), SW_HIDE);
    } else {
        for (const auto window : board_hidden_windows_)
            if (IsWindow(window))
                ShowWindow(window, SW_SHOWNOACTIVATE);
        board_hidden_windows_.clear();
        update_overlay_interaction();
    }
}
