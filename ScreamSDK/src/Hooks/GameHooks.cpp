#include "Internal.hpp"

#include "Diesel/Lua.hpp"
#include "ScreamSDK/Addresses.hpp"
#include "ScreamSDK/Build.hpp"
#include "ScreamSDK/Log.hpp"

#include <mutex>

namespace Scream {
namespace GameHooks {

namespace {

typedef bool (SCREAM_THISCALL* ApplicationUpdate)(Diesel::Application*);
ApplicationUpdate g_originalUpdate = NULL;
std::mutex g_callbackMutex;

bool SCREAM_FASTCALL UpdateDetour(Diesel::Application* application, void*) {
    std::lock_guard<std::mutex> callbackLock(g_callbackMutex);
    if (!Runtime::IsShuttingDown()) {
        try {
            Diesel::Lua::OnMainThreadUpdate();
        } catch (...) {
            SCREAM_LOG_ERROR("Unexpected C++ exception in Lua main-thread update");
        }
    }
    return g_originalUpdate != NULL ? g_originalUpdate(application) : false;
}

} // namespace

bool Install() {
    void* target = Build::ResolveRva<void*>(Addresses::ApplicationUpdate);
    return InternalHooks::Create("Application::update", target,
                                 reinterpret_cast<void*>(&UpdateDetour),
                                 reinterpret_cast<void**>(&g_originalUpdate));
}

void Quiesce() {
    std::lock_guard<std::mutex> callbackLock(g_callbackMutex);
}

} // namespace GameHooks
} // namespace Scream
