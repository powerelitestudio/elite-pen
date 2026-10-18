// Native UI QA helper. WGC observes DWM thumbnails which GDI/DXGI may omit.
// Usage: wgc-capture output.bmp monitor-x monitor-y [pixel-x pixel-y samples]
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <roapi.h>
#include <winstring.h>
// LLVM-MinGW 20260616 aliases boolean and BYTE. Its generated Foundation header
// duplicates their IReference specialization; this helper uses neither reference.
#define ____FIReference_1_boolean_INTERFACE_DEFINED__
#include <windows.graphics.capture.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cstdint>

using Microsoft::WRL::ComPtr;
namespace capture = ABI::Windows::Graphics::Capture;
namespace directx = ABI::Windows::Graphics::DirectX;

void checked(HRESULT result) {
    if (FAILED(result)) {
        std::cerr << "WGC HRESULT=0x" << std::hex << static_cast<unsigned long>(result) << '\n';
        throw std::runtime_error("Windows Graphics Capture failed");
    }
}

template <typename T> ComPtr<T> factory(const wchar_t* name) {
    HSTRING text{};
    checked(WindowsCreateString(name, static_cast<UINT32>(wcslen(name)), &text));
    ComPtr<T> result;
    const auto status = RoGetActivationFactory(text, IID_PPV_ARGS(&result));
    WindowsDeleteString(text);
    checked(status);
    return result;
}

template <typename T> void close_object(T* object) {
    ComPtr<ABI::Windows::Foundation::IClosable> closable;
    if (object && SUCCEEDED(object->QueryInterface(IID_PPV_ARGS(&closable)))) closable->Close();
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 4 && argc != 7) return 2;
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    try {
        checked(RoInitialize(RO_INIT_MULTITHREADED));
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        checked(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
            &device, nullptr, &context));
        ComPtr<IDXGIDevice> dxgi;
        checked(device.As(&dxgi));
        ComPtr<IInspectable> inspectable;
        checked(CreateDirect3D11DeviceFromDXGIDevice(dxgi.Get(), &inspectable));
        ComPtr<directx::Direct3D11::IDirect3DDevice> winrt_device;
        checked(inspectable.As(&winrt_device));
        const POINT point{_wtoi(argv[2]), _wtoi(argv[3])};
        const auto monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
        auto interop = factory<IGraphicsCaptureItemInterop>(L"Windows.Graphics.Capture.GraphicsCaptureItem");
        ComPtr<capture::IGraphicsCaptureItem> item;
        checked(interop->CreateForMonitor(monitor, IID_PPV_ARGS(&item)));
        ABI::Windows::Graphics::SizeInt32 size{};
        checked(item->get_Size(&size));
        auto statics = factory<capture::IDirect3D11CaptureFramePoolStatics2>(
            L"Windows.Graphics.Capture.Direct3D11CaptureFramePool");
        ComPtr<capture::IDirect3D11CaptureFramePool> pool;
        checked(statics->CreateFreeThreaded(winrt_device.Get(),
            directx::DirectXPixelFormat_B8G8R8A8UIntNormalized, 2, size, &pool));
        ComPtr<capture::IGraphicsCaptureSession> session;
        checked(pool->CreateCaptureSession(item.Get(), &session));
        checked(session->StartCapture());
        const int count = argc == 7 ? std::clamp(_wtoi(argv[6]), 1, 30) : 1;
        for (int sample = 0; sample < count; ++sample) {
            ComPtr<capture::IDirect3D11CaptureFrame> frame;
            for (int attempt = 0; attempt < 200 && !frame; ++attempt) {
                checked(pool->TryGetNextFrame(&frame));
                if (!frame) Sleep(15);
            }
            if (!frame) throw std::runtime_error("WGC frame timeout");
            ComPtr<directx::Direct3D11::IDirect3DSurface> surface;
            checked(frame->get_Surface(&surface));
            ComPtr<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess> access;
            checked(surface.As(&access));
            ComPtr<ID3D11Texture2D> texture;
            checked(access->GetInterface(IID_PPV_ARGS(&texture)));
            D3D11_TEXTURE2D_DESC description{};
            texture->GetDesc(&description);
            description.Usage = D3D11_USAGE_STAGING;
            description.BindFlags = 0;
            description.MiscFlags = 0;
            description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            ComPtr<ID3D11Texture2D> staging;
            checked(device->CreateTexture2D(&description, nullptr, &staging));
            context->CopyResource(staging.Get(), texture.Get());
            D3D11_MAPPED_SUBRESOURCE mapped{};
            checked(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
            if (argc == 7) {
                const int x = std::clamp(_wtoi(argv[4]), 0, size.Width - 1);
                const int y = std::clamp(_wtoi(argv[5]), 0, size.Height - 1);
                const auto* pixel = static_cast<const unsigned char*>(mapped.pData) +
                    static_cast<std::size_t>(y) * mapped.RowPitch + static_cast<std::size_t>(x) * 4;
                const auto value = static_cast<std::int32_t>(0xff000000U |
                    (static_cast<UINT32>(pixel[2]) << 16U) |
                    (static_cast<UINT32>(pixel[1]) << 8U) | pixel[0]);
                std::cout << value << '\n';
            }
            if (sample == count - 1) {
                BITMAPFILEHEADER header{};
                header.bfType = 0x4d42;
                header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
                header.bfSize = header.bfOffBits + static_cast<DWORD>(size.Width * size.Height * 4);
                BITMAPINFOHEADER bitmap{};
                bitmap.biSize = sizeof(bitmap);
                bitmap.biWidth = size.Width;
                bitmap.biHeight = -size.Height;
                bitmap.biPlanes = 1;
                bitmap.biBitCount = 32;
                bitmap.biCompression = BI_RGB;
                std::ofstream output(std::filesystem::path(argv[1]), std::ios::binary);
                output.write(reinterpret_cast<const char*>(&header), sizeof(header));
                output.write(reinterpret_cast<const char*>(&bitmap), sizeof(bitmap));
                for (int y = 0; y < size.Height; ++y)
                    output.write(static_cast<const char*>(mapped.pData) +
                        static_cast<std::size_t>(y) * mapped.RowPitch, size.Width * 4);
                if (!output) throw std::runtime_error("WGC output write failed");
            }
            context->Unmap(staging.Get(), 0);
            close_object(frame.Get());
            Sleep(110);
        }
        close_object(session.Get());
        close_object(pool.Get());
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
