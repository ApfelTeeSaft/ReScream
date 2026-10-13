#include "Diesel/Renderer.hpp"

#include "Diesel/Core.hpp"
#include "Diesel/Game.hpp"
#include "ScreamSDK/Memory.hpp"

namespace Diesel {
namespace Renderer {

RenderInterface* GetRenderInterface() {
    Application* application = Core::GetApplication();
    if (application == NULL ||
        !Scream::Memory::IsReadable(application, offsetof(Application, renderInterface) +
                                                  sizeof(application->renderInterface))) {
        return NULL;
    }
    RenderInterface* result = NULL;
    if (!Scream::Memory::TryRead(&application->renderInterface, result) || result == NULL ||
        !Scream::Memory::IsReadable(result, sizeof(RenderInterface))) {
        return NULL;
    }
    return result;
}

D3DRenderDevice* GetRenderDevice() {
    RenderInterface* renderInterface = GetRenderInterface();
    if (renderInterface == NULL) {
        return NULL;
    }
    D3DRenderDevice* result = NULL;
    if (!Scream::Memory::TryRead(&renderInterface->d3dDevice, result) || result == NULL ||
        !Scream::Memory::IsReadable(result, offsetof(D3DRenderDevice, device) +
                                           sizeof(result->device))) {
        return NULL;
    }
    return result;
}

IDirect3DDevice9* GetD3D9Device() {
    D3DRenderDevice* renderDevice = GetRenderDevice();
    if (renderDevice == NULL) {
        return NULL;
    }
    IDirect3DDevice9* result = NULL;
    if (!Scream::Memory::TryRead(&renderDevice->device.pointer, result) || result == NULL ||
        !Scream::Memory::IsReadable(result, sizeof(void*))) {
        return NULL;
    }
    void** vtable = NULL;
    void* present = NULL;
    if (!Scream::Memory::TryRead(result, vtable) || vtable == NULL ||
        !Scream::Memory::TryRead(vtable + 17, present) ||
        !Scream::Memory::IsExecutable(present) ||
        !Scream::Memory::IsAddressInAnyModule(present)) {
        return NULL;
    }
    return result;
}

HWND GetWindow() {
    RenderInterface* renderInterface = GetRenderInterface();
    if (renderInterface == NULL) {
        return NULL;
    }
    Win32Window* window = NULL;
    HWND hwnd = NULL;
    if (!Scream::Memory::TryRead(&renderInterface->window, window) || window == NULL ||
        !Scream::Memory::TryRead(&window->hwnd, hwnd) || hwnd == NULL ||
        !IsWindow(hwnd)) {
        return NULL;
    }
    return hwnd;
}

} // namespace Renderer
} // namespace Diesel
