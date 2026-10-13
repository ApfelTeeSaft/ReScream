#pragma once

#include "ScreamSDK/Platform.hpp"

#include <Windows.h>
#include <d3d9.h>

#include <atomic>
#include <cstdint>
#include <string>

struct lua_State;

namespace Diesel {
struct Application;
struct D3DRenderDevice;
}

namespace Scream {

namespace InternalHooks {
bool Create(const char* name, void* target, void* detour, void** original);
bool IsEnabled(const char* name);
}

namespace GameHooks {
bool Install();
void Quiesce();
}

namespace LuaHooks {
bool Install();
void Quiesce();
}

namespace RendererHooks {
bool Install();
void Quiesce();
}

namespace LuaCatalogInternal {
void Discover();
void Clear();
}

namespace PluginsInternal {
void Initialize();
void DrawOverlayPanels();
bool Shutdown();
bool IsShutdownComplete();
}

namespace Overlay {
void PublishDevice(IDirect3DDevice9* device);
void OnPresent(Diesel::D3DRenderDevice* renderDevice);
void PollInstallDeviceHook();
HRESULT OnReset(IDirect3DDevice9* device,
                D3DPRESENT_PARAMETERS* parameters,
                HRESULT (WINAPI* original)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*));
void RequestRenderThreadShutdown();
bool IsRenderThreadShutdownComplete();
void Quiesce();
bool IsVisible();
int ToggleKey();
void SetToggleKey(int virtualKey);
}

namespace Runtime {
bool IsShuttingDown();
void RequestShutdown();
HMODULE Module();
}

} // namespace Scream
