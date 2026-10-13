#pragma once

#include "Diesel/Types.hpp"

#include <Windows.h>
#include <d3d9.h>

#include <cstddef>
#include <cstdint>

namespace Diesel {

#pragma pack(push, 1)
struct Win32Window {
    HWND hwnd;                            // +0x00
};

struct RenderInterface {
    void* vtable;                         // +0x00
    void* device;                        // +0x04 generic RenderDevice*
    void* renderingEnvironment;          // +0x08
    void* renderSettings;                 // +0x0C
    D3DRenderDevice* d3dDevice;           // +0x10
    Win32Window* window;                  // +0x14
    std::uint8_t unknown_018[0x120];
};

struct D3DRenderDevice {
    void* vtable;                         // +0x00
    Win32Window* window;                  // +0x04
    ComPtr32<IDirect3D9> d3d;             // +0x08
    ComPtr32<IDirect3DDevice9> device;    // +0x10
    ComPtr32<IDirect3DSurface9> backBuffer;// +0x18
    IDirect3DSurface9* depthStencilBuffer;// +0x20
    std::uint8_t unknown_024[0x150];
    D3DPRESENT_PARAMETERS presentation;  // +0x174
    std::uint8_t unknown_1AC[0x104];
    std::uint8_t resourceManager[0x4F30];// +0x2B0 opaque D3DResourceManager
    std::uint32_t presentCount;           // +0x51E0
    std::uint32_t frameCount;             // +0x51E4
    std::uint8_t unknown_51E8[0x058];
};
#pragma pack(pop)

static_assert(offsetof(RenderInterface, d3dDevice) == 0x10,
              "RenderInterface::_d3d_device");
static_assert(offsetof(RenderInterface, window) == 0x14,
              "RenderInterface::_window");
static_assert(sizeof(RenderInterface) == 0x138, "RenderInterface size");
static_assert(offsetof(D3DRenderDevice, device) == 0x10,
              "D3DRenderDevice::_device");
static_assert(offsetof(D3DRenderDevice, presentation) == 0x174,
              "D3DRenderDevice::_presentation");
static_assert(offsetof(D3DRenderDevice, presentCount) == 0x51E0,
              "D3DRenderDevice::_present_count");
static_assert(sizeof(D3DRenderDevice) == 0x5240, "D3DRenderDevice size");

namespace Renderer {

SCREAM_API RenderInterface* GetRenderInterface();
SCREAM_API D3DRenderDevice* GetRenderDevice();
SCREAM_API IDirect3DDevice9* GetD3D9Device();
SCREAM_API HWND GetWindow();

} // namespace Renderer
} // namespace Diesel
