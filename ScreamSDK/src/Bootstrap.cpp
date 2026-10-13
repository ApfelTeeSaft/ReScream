#include "ScreamSDK/Runtime.hpp"

#include "Diesel/Core.hpp"
#include "Diesel/Lua.hpp"
#include "Diesel/Renderer.hpp"
#include "Internal.hpp"
#include "ScreamSDK/Build.hpp"
#include "ScreamSDK/Hooks.hpp"
#include "ScreamSDK/Log.hpp"

#include <Windows.h>
#include <process.h>

#include <atomic>
#include <string>

namespace Scream {
namespace Runtime {

namespace {

HMODULE g_module = NULL;
std::atomic<bool> g_shuttingDown(false);
std::atomic<bool> g_workerRunning(false);
std::atomic<bool> g_workerComplete(false);
std::atomic<bool> g_safeShutdownComplete(false);
HANDLE g_workerHandle = NULL;
DWORD g_workerThreadId = 0;

unsigned int FinishWorker(HMODULE selfReference,
                          unsigned int exitCode,
                          bool safeShutdown,
                          bool releaseSelfReference = true) {
    const bool pluginsSafe = PluginsInternal::Shutdown();
    if (!pluginsSafe) {
        safeShutdown = false;
        releaseSelfReference = false;
    }
    g_safeShutdownComplete.store(safeShutdown);
    g_workerRunning.store(false);
    g_workerComplete.store(true);
    if (selfReference != NULL && releaseSelfReference) {
        FreeLibraryAndExitThread(selfReference, exitCode);
    }
    return exitCode;
}

unsigned int __stdcall Worker(void*) {
    g_workerRunning.store(true);
    Log::Initialize(g_module);
    HMODULE selfReference = NULL;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCSTR>(&Worker), &selfReference)) {
        SCREAM_LOG_ERROR("Could not retain the ScreamSDK module for worker lifetime");
        Log::Shutdown();
        return FinishWorker(NULL, 4, true);
    }
    SCREAM_LOG_INFO("ScreamSDK x86 starting (interface 0x%08X)",
                    ScreamSDK_InterfaceVersion());
    SCREAM_LOG_INFO("Game base: 0x%08X",
                    static_cast<unsigned int>(Build::GameBase()));

    GameBuild build = {};
    std::string error;
    if (!Build::FingerprintRunningGame(build, error)) {
        SCREAM_LOG_ERROR("Executable fingerprint failed: %s", error.c_str());
        Log::Shutdown();
        return FinishWorker(selfReference, 1, true);
    }

    SCREAM_LOG_INFO("PE machine=0x%04X timestamp=0x%08X image=0x%08X entry=0x%08X",
                    build.machine, build.timeDateStamp, build.imageSize,
                    build.entryPointRva);
    SCREAM_LOG_INFO("PDB RSDS: %s age=%u",
                    build.pdbGuid.empty() ? "<none>" : build.pdbGuid.c_str(),
                    build.pdbAge);
    SCREAM_LOG_INFO("Raw .text SHA-256: %s", build.textSha256.c_str());
    std::string mismatch;
    if (!Build::MatchesSupportedBuild(build, mismatch)) {
        SCREAM_LOG_ERROR("Unsupported executable; no offsets or hooks will be used: %s",
                         mismatch.c_str());
        Log::Shutdown();
        return FinishWorker(selfReference, 2, true);
    }
    SCREAM_LOG_INFO("Game fingerprint matched the analyzed public development build");

    if (g_shuttingDown.load()) {
        SCREAM_LOG_INFO("Shutdown was requested before hook installation; module remains inert");
        Log::Shutdown();
        return FinishWorker(selfReference, 0, true);
    }

    Diesel::Lua::Initialize();
    if (!Hooks::Initialize()) {
        SCREAM_LOG_ERROR("Hook subsystem unavailable; ScreamSDK remains inert");
    } else {
        const bool gameHook = GameHooks::Install();
        const bool luaHooks = LuaHooks::Install();
        const bool rendererHook = RendererHooks::Install();
        SCREAM_LOG_INFO("Initial hooks: update=%s lua=%s renderer=%s",
                        gameHook ? "yes" : "no",
                        luaHooks ? "yes" : "no",
                        rendererHook ? "yes" : "no");
    }

    while (!g_shuttingDown.load()) {
        Overlay::PollInstallDeviceHook();
        Sleep(100);
    }

    SCREAM_LOG_INFO("Clean shutdown requested; waiting up to 10 seconds for render-thread teardown");
    const DWORD waitStarted = GetTickCount();
    while (!Overlay::IsRenderThreadShutdownComplete() &&
           GetTickCount() - waitStarted < 10000u) {
        Sleep(25);
    }
    if (!Overlay::IsRenderThreadShutdownComplete()) {
        SCREAM_LOG_ERROR(
            "Render-thread teardown did not complete; hooks and module are retained in an inert state");
        Log::Shutdown();
        return FinishWorker(selfReference, 3, false, false);
    }

    if (!Hooks::DisableAll()) {
        SCREAM_LOG_ERROR(
            "Hook disabling failed; module is retained to keep installed detours valid");
        Log::Shutdown();
        return FinishWorker(selfReference, 5, false, false);
    }
    // No new detours can enter after DisableAll. These barriers wait for calls
    // that had already crossed a trampoline before it was patched back.
    RendererHooks::Quiesce();
    Overlay::Quiesce();
    GameHooks::Quiesce();
    LuaHooks::Quiesce();
    Hooks::Shutdown();
    Diesel::Lua::Shutdown();
    SCREAM_LOG_INFO("ScreamSDK hooks removed; module is now inert");
    Log::Shutdown();
    return FinishWorker(selfReference, 0, true);
}

} // namespace

bool IsShuttingDown() {
    return g_shuttingDown.load();
}

void RequestShutdown() {
    g_shuttingDown.store(true);
}

HMODULE Module() {
    return g_module;
}

bool Start(HMODULE module) {
    g_module = module;
    g_shuttingDown.store(false);
    g_workerComplete.store(false);
    g_safeShutdownComplete.store(false);
    PluginsInternal::Initialize();
    unsigned int threadId = 0;
    uintptr_t thread = _beginthreadex(NULL, 0, &Worker, NULL, 0, &threadId);
    if (thread == 0) {
        PluginsInternal::Shutdown();
        return false;
    }
    g_workerThreadId = threadId;
    g_workerHandle = reinterpret_cast<HANDLE>(thread);
    return true;
}

bool WaitForShutdown(std::uint32_t timeoutMilliseconds) {
    HANDLE worker = g_workerHandle;
    if (worker == NULL || GetCurrentThreadId() == g_workerThreadId) {
        return g_workerComplete.load() && g_safeShutdownComplete.load();
    }
    const DWORD wait = WaitForSingleObject(worker, timeoutMilliseconds);
    return wait == WAIT_OBJECT_0 && g_safeShutdownComplete.load();
}

bool IsShutdownComplete() {
    return g_workerComplete.load() && g_safeShutdownComplete.load();
}

} // namespace Runtime
} // namespace Scream

extern "C" bool SCREAM_CDECL ScreamSDK_EnsureLoaded() {
    return !Scream::Runtime::IsShuttingDown() &&
           !Scream::Runtime::IsShutdownComplete();
}

extern "C" void SCREAM_CDECL ScreamSDK_RequestShutdown() {
    Scream::Runtime::RequestShutdown();
}

extern "C" bool SCREAM_CDECL ScreamSDK_WaitForShutdown(
    std::uint32_t timeoutMilliseconds) {
    return Scream::Runtime::WaitForShutdown(timeoutMilliseconds);
}

extern "C" bool SCREAM_CDECL ScreamSDK_IsShutdownComplete() {
    return Scream::Runtime::IsShutdownComplete();
}

extern "C" std::uint32_t SCREAM_CDECL ScreamSDK_InterfaceVersion() {
    return 0x00010002u;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        if (!Scream::Runtime::Start(instance)) {
            return FALSE;
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        Scream::Runtime::RequestShutdown();
        if (Scream::Runtime::g_workerHandle != NULL) {
            CloseHandle(Scream::Runtime::g_workerHandle);
            Scream::Runtime::g_workerHandle = NULL;
        }
    }
    (void)reserved;
    return TRUE;
}
