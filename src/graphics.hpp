#pragma once

#include <windows.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <string>

namespace elite_pen::win {

using Microsoft::WRL::ComPtr;

class GraphicsDevice {
public:
    bool initialize(std::wstring& error);

    [[nodiscard]] ID3D11Device* d3d() const noexcept { return d3d_.Get(); }
    [[nodiscard]] ID3D11DeviceContext* d3d_context() const noexcept {
        return d3d_context_.Get();
    }
    [[nodiscard]] ID2D1Factory1* d2d_factory() const noexcept { return d2d_factory_.Get(); }
    [[nodiscard]] ID2D1Device* d2d_device() const noexcept { return d2d_device_.Get(); }
    [[nodiscard]] IDWriteFactory* dwrite() const noexcept { return dwrite_.Get(); }
    [[nodiscard]] IDCompositionDevice* composition() const noexcept { return composition_.Get(); }
    [[nodiscard]] bool using_warp() const noexcept { return using_warp_; }

private:
    ComPtr<ID3D11Device> d3d_;
    ComPtr<ID3D11DeviceContext> d3d_context_;
    ComPtr<IDXGIDevice> dxgi_device_;
    ComPtr<ID2D1Factory1> d2d_factory_;
    ComPtr<ID2D1Device> d2d_device_;
    ComPtr<IDWriteFactory> dwrite_;
    ComPtr<IDCompositionDevice> composition_;
    bool using_warp_{};
};

class Surface {
public:
    bool initialize(GraphicsDevice& device, HWND window, UINT width, UINT height,
                    std::wstring& error);
    bool resize(UINT width, UINT height, std::wstring& error);
    ID2D1DeviceContext* begin_draw(D2D1_COLOR_F clear_color = D2D1::ColorF(0, 0.0F));
    bool end_draw(std::wstring& error);
    void invalidate_device_objects();

    [[nodiscard]] ID2D1DeviceContext* context() const noexcept { return context_.Get(); }
    [[nodiscard]] UINT width() const noexcept { return width_; }
    [[nodiscard]] UINT height() const noexcept { return height_; }

private:
    bool create_target(std::wstring& error);

    GraphicsDevice* device_{};
    HWND window_{};
    UINT width_{};
    UINT height_{};
    ComPtr<IDXGISwapChain1> swap_chain_;
    ComPtr<IDCompositionTarget> composition_target_;
    ComPtr<IDCompositionVisual> visual_;
    ComPtr<ID2D1DeviceContext> context_;
    ComPtr<ID2D1Bitmap1> target_bitmap_;
};

[[nodiscard]] std::wstring hresult_message(HRESULT result);

}  // namespace elite_pen::win
