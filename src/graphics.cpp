#include "graphics.hpp"

#include <array>
#include <sstream>

namespace elite_pen::win {

std::wstring hresult_message(HRESULT result) {
    wchar_t* system_message = nullptr;
    const DWORD count = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, static_cast<DWORD>(result), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&system_message), 0, nullptr);
    std::wostringstream stream;
    stream << L"0x" << std::hex << static_cast<unsigned long>(result);
    if (count != 0 && system_message != nullptr) {
        stream << L": " << system_message;
        LocalFree(system_message);
    }
    return stream.str();
}

namespace {

HRESULT create_d3d(D3D_DRIVER_TYPE type, ID3D11Device** device,
                   ID3D11DeviceContext** context) {
    constexpr std::array feature_levels{
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };
    D3D_FEATURE_LEVEL selected{};
    HRESULT result = D3D11CreateDevice(
        nullptr, type, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED,
        feature_levels.data(), static_cast<UINT>(feature_levels.size()),
        D3D11_SDK_VERSION, device, &selected, context);
    if (result == E_INVALIDARG) {
        result = D3D11CreateDevice(
            nullptr, type, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED,
            feature_levels.data() + 1,
            static_cast<UINT>(feature_levels.size() - 1), D3D11_SDK_VERSION,
            device, &selected, context);
    }
    return result;
}

}  // namespace

bool GraphicsDevice::initialize(std::wstring& error) {
    HRESULT result = create_d3d(D3D_DRIVER_TYPE_HARDWARE, d3d_.GetAddressOf(),
                                d3d_context_.GetAddressOf());
    if (FAILED(result)) {
        using_warp_ = true;
        result = create_d3d(D3D_DRIVER_TYPE_WARP, d3d_.GetAddressOf(),
                            d3d_context_.GetAddressOf());
    }
    if (FAILED(result)) {
        error = L"No se pudo crear el dispositivo grafico: " + hresult_message(result);
        return false;
    }

    result = d3d_.As(&dxgi_device_);
    if (FAILED(result)) {
        error = L"No se pudo obtener DXGI: " + hresult_message(result);
        return false;
    }
    // Every graphics call is issued by the Win32 UI thread. A one-frame DXGI
    // queue keeps pointer-to-pixel latency low and prevents stale frames from
    // accumulating behind DirectComposition on slower integrated GPUs.
    ComPtr<IDXGIDevice1> low_latency_device;
    if (SUCCEEDED(dxgi_device_.As(&low_latency_device))) {
        low_latency_device->SetMaximumFrameLatency(1);
    }

    D2D1_FACTORY_OPTIONS factory_options{};
#ifdef ELITE_PEN_DEBUG
    factory_options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    result = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                               __uuidof(ID2D1Factory1), &factory_options,
                               reinterpret_cast<void**>(d2d_factory_.GetAddressOf()));
    if (FAILED(result)) {
        error = L"No se pudo iniciar Direct2D: " + hresult_message(result);
        return false;
    }
    result = d2d_factory_->CreateDevice(dxgi_device_.Get(), d2d_device_.GetAddressOf());
    if (FAILED(result)) {
        error = L"No se pudo conectar Direct2D con la GPU: " + hresult_message(result);
        return false;
    }
    result = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                 reinterpret_cast<IUnknown**>(dwrite_.GetAddressOf()));
    if (FAILED(result)) {
        error = L"No se pudo iniciar DirectWrite: " + hresult_message(result);
        return false;
    }
    result = DCompositionCreateDevice(dxgi_device_.Get(), __uuidof(IDCompositionDevice),
                                      reinterpret_cast<void**>(composition_.GetAddressOf()));
    if (FAILED(result)) {
        error = L"No se pudo iniciar la composicion transparente: " + hresult_message(result);
        return false;
    }
    return true;
}

bool Surface::initialize(GraphicsDevice& device, HWND window, UINT width, UINT height,
                         std::wstring& error) {
    device_ = &device;
    window_ = window;
    width_ = std::max(width, 1U);
    height_ = std::max(height, 1U);

    ComPtr<IDXGIDevice> dxgi_device;
    HRESULT result = device.d3d()->QueryInterface(IID_PPV_ARGS(&dxgi_device));
    if (FAILED(result)) {
        error = L"No se pudo consultar el dispositivo DXGI: " + hresult_message(result);
        return false;
    }
    ComPtr<IDXGIAdapter> adapter;
    result = dxgi_device->GetAdapter(adapter.GetAddressOf());
    if (FAILED(result)) {
        error = L"No se pudo consultar el adaptador grafico: " + hresult_message(result);
        return false;
    }
    ComPtr<IDXGIFactory2> factory;
    result = adapter->GetParent(IID_PPV_ARGS(&factory));
    if (FAILED(result)) {
        error = L"No se pudo consultar la fabrica DXGI: " + hresult_message(result);
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 description{};
    description.Width = width_;
    description.Height = height_;
    description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    description.Stereo = FALSE;
    description.SampleDesc = {1, 0};
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.BufferCount = 2;
    description.Scaling = DXGI_SCALING_STRETCH;
    description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    description.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    result = factory->CreateSwapChainForComposition(device.d3d(), &description, nullptr,
                                                     swap_chain_.GetAddressOf());
    if (FAILED(result)) {
        error = L"No se pudo crear la superficie: " + hresult_message(result);
        return false;
    }

    result = device.d2d_device()->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE, context_.GetAddressOf());
    if (FAILED(result)) {
        error = L"No se pudo crear el contexto Direct2D: " + hresult_message(result);
        return false;
    }
    context_->SetUnitMode(D2D1_UNIT_MODE_PIXELS);

    result = device.composition()->CreateTargetForHwnd(window_, TRUE,
                                                       composition_target_.GetAddressOf());
    if (FAILED(result)) {
        error = L"No se pudo enlazar la ventana transparente: " + hresult_message(result);
        return false;
    }
    result = device.composition()->CreateVisual(visual_.GetAddressOf());
    if (FAILED(result) || FAILED(visual_->SetContent(swap_chain_.Get())) ||
        FAILED(composition_target_->SetRoot(visual_.Get())) ||
        FAILED(device.composition()->Commit())) {
        error = L"No se pudo componer la ventana transparente: " + hresult_message(result);
        return false;
    }
    return create_target(error);
}

bool Surface::create_target(std::wstring& error) {
    ComPtr<IDXGISurface> surface;
    HRESULT result = swap_chain_->GetBuffer(0, IID_PPV_ARGS(&surface));
    if (FAILED(result)) {
        error = L"No se pudo obtener el buffer de dibujo: " + hresult_message(result);
        return false;
    }
    const D2D1_BITMAP_PROPERTIES1 properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                         D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0F, 96.0F);
    result = context_->CreateBitmapFromDxgiSurface(surface.Get(), &properties,
                                                    target_bitmap_.GetAddressOf());
    if (FAILED(result)) {
        error = L"No se pudo crear el destino Direct2D: " + hresult_message(result);
        return false;
    }
    context_->SetTarget(target_bitmap_.Get());
    ++generation_;
    return true;
}

void Surface::invalidate_device_objects() {
    if (context_) context_->SetTarget(nullptr);
    target_bitmap_.Reset();
}

bool Surface::resize(UINT width, UINT height, std::wstring& error) {
    width = std::max(width, 1U);
    height = std::max(height, 1U);
    if (!swap_chain_ || (width == width_ && height == height_)) return true;
    invalidate_device_objects();
    const HRESULT result = swap_chain_->ResizeBuffers(0, width, height,
                                                       DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(result)) {
        error = L"No se pudo redimensionar la superficie: " + hresult_message(result);
        return false;
    }
    width_ = width;
    height_ = height;
    return create_target(error);
}

ID2D1DeviceContext* Surface::begin_draw(D2D1_COLOR_F clear_color) {
    if (!context_ || !target_bitmap_) return nullptr;
    context_->BeginDraw();
    context_->SetTransform(D2D1::Matrix3x2F::Identity());
    context_->Clear(clear_color);
    return context_.Get();
}

bool Surface::end_draw(std::wstring& error) {
    HRESULT result = context_->EndDraw();
    if (result == D2DERR_RECREATE_TARGET) {
        invalidate_device_objects();
        if (!create_target(error)) return false;
        return true;
    }
    if (FAILED(result)) {
        error = L"Direct2D no pudo finalizar el cuadro: " + hresult_message(result);
        return false;
    }
    // Composition already paces frames with DWM. A zero sync interval avoids
    // serially blocking the UI thread once per transparent top-level surface.
    result = swap_chain_->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
    if (result == DXGI_ERROR_WAS_STILL_DRAWING) {
        // Never make pointer input wait for the compositor. Keep the newest
        // frame dirty so it is presented as soon as a swap-chain buffer frees.
        SetTimer(window_, kPresentRetryTimer, 8, nullptr);
        return true;
    }
    if (FAILED(result)) {
        error = L"No se pudo presentar el cuadro: " + hresult_message(result);
        return false;
    }
    return true;
}

}  // namespace elite_pen::win
