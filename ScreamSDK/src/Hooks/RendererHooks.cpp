#include "Internal.hpp"

#include "Diesel/Renderer.hpp"
#include "ScreamSDK/Addresses.hpp"
#include "ScreamSDK/Build.hpp"
#include "ScreamSDK/Log.hpp"

#include <mutex>

namespace Scream {
namespace RendererHooks {

namespace {

typedef bool (SCREAM_THISCALL* PresentFunction)(Diesel::D3DRenderDevice*);
PresentFunction g_originalPresent = NULL;
std::mutex g_callbackMutex;

bool SCREAM_FASTCALL PresentDetour(Diesel::D3DRenderDevice* renderDevice, void*) {
    std::lock_guard<std::mutex> callbackLock(g_callbackMutex);
    if (!Runtime::IsShuttingDown()) {
        try {
            Overlay::OnPresent(renderDevice);
        } catch (...) {
            SCREAM_LOG_ERROR("Unexpected C++ exception in the overlay render path; shutting down");
            Runtime::RequestShutdown();
        }
    } else {
        Overlay::RequestRenderThreadShutdown();
    }
    return g_originalPresent != NULL ? g_originalPresent(renderDevice) : false;
}

} // namespace

bool Install() {
    void* target = Build::ResolveRva<void*>(Addresses::D3DRenderDevicePresent);
    return InternalHooks::Create("D3DRenderDevice::present", target,
                                 reinterpret_cast<void*>(&PresentDetour),
                                 reinterpret_cast<void**>(&g_originalPresent));
}

void Quiesce() {
    std::lock_guard<std::mutex> callbackLock(g_callbackMutex);
}

} // namespace RendererHooks
} // namespace Scream
