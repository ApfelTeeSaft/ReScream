#include "Client/Runtime.hpp"

#include "LuaFixes.hpp"
#include "Log.hpp"
#include "Patches.hpp"
#include "RuntimeInternal.hpp"
#include "ScreamSDK/Build.hpp"

#include <Windows.h>
#include <process.h>

#include <atomic>
#include <string>

namespace Client {
namespace Runtime {

namespace {

HMODULE g_module = NULL;
std::atomic<bool> g_shutdownRequested(false);
std::atomic<bool> g_workerComplete(false);
std::atomic<bool> g_safeShutdownComplete(false);
std::atomic<bool> g_supportedBuild(false);
std::atomic<bool> g_fixesActive(false);
HANDLE g_workerHandle = NULL;
DWORD g_workerThreadId = 0;

unsigned int FinishWorker(HMODULE selfReference,
                          unsigned int exitCode,
                          bool safeShutdown,
                          bool releaseSelfReference = true) {
    g_fixesActive.store(false);
    g_safeShutdownComplete.store(safeShutdown);
    g_workerComplete.store(true);
    if (selfReference != NULL && releaseSelfReference) {
        FreeLibraryAndExitThread(selfReference, exitCode);
    }
    return exitCode;
}

unsigned int __stdcall Worker(void*) {
    Log::Initialize(g_module);
    HMODULE selfReference = NULL;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCSTR>(&Worker), &selfReference)) {
        CLIENT_LOG_ERROR("Could not retain client.dll for the worker lifetime");
        Log::Shutdown();
        return FinishWorker(NULL, 4, true);
    }

    CLIENT_LOG_INFO("client.dll x86 starting (interface 0x%08X)",
                    Client_InterfaceVersion());
    Scream::GameBuild build = {};
    std::string error;
    if (!Scream::Build::FingerprintRunningGame(build, error)) {
        CLIENT_LOG_ERROR("Executable fingerprint failed: %s", error.c_str());
        Log::Shutdown();
        return FinishWorker(selfReference, 1, true);
    }

    CLIENT_LOG_INFO("PE machine=0x%04X timestamp=0x%08X image=0x%08X entry=0x%08X",
                    build.machine, build.timeDateStamp, build.imageSize,
                    build.entryPointRva);
    CLIENT_LOG_INFO("PDB RSDS: %s age=%u",
                    build.pdbGuid.empty() ? "<none>" : build.pdbGuid.c_str(),
                    build.pdbAge);
    CLIENT_LOG_INFO("Raw .text SHA-256: %s", build.textSha256.c_str());

    std::string mismatch;
    if (!Scream::Build::MatchesSupportedBuild(build, mismatch)) {
        CLIENT_LOG_WARNING(
            "Unsupported executable; client.dll remains inert: %s",
            mismatch.c_str());
        Log::Shutdown();
        return FinishWorker(selfReference, 2, true);
    }
    g_supportedBuild.store(true);
    CLIENT_LOG_INFO("Game fingerprint matched the analyzed public development build");

    if (g_shutdownRequested.load()) {
        CLIENT_LOG_INFO("Shutdown requested before patch initialization; remaining inert");
        Log::Shutdown();
        return FinishWorker(selfReference, 0, true);
    }

    if (!Patches::Initialize()) {
        CLIENT_LOG_ERROR("Patch subsystem unavailable; client.dll remains inert");
    } else {
        g_fixesActive.store(true);
    }

    while (!g_shutdownRequested.load()) {
        Sleep(100);
    }

    CLIENT_LOG_INFO("Clean shutdown requested");
    if (!Patches::Shutdown()) {
        CLIENT_LOG_ERROR(
            "Patch shutdown failed; client.dll is retained so no detour can dangle");
        Log::Shutdown();
        return FinishWorker(selfReference, 5, false, false);
    }
    CLIENT_LOG_INFO(
        "client.dll native hook removed; DLL callbacks are inert "
        "(installed Lua-only fixes remain for this process lifetime)");
    Log::Shutdown();
    return FinishWorker(selfReference, 0, true);
}

} // namespace

bool Start(HMODULE module) {
    g_module = module;
    g_shutdownRequested.store(false);
    g_workerComplete.store(false);
    g_safeShutdownComplete.store(false);
    g_supportedBuild.store(false);
    g_fixesActive.store(false);
    unsigned int threadId = 0;
    const uintptr_t thread = _beginthreadex(NULL, 0, &Worker, NULL, 0, &threadId);
    if (thread == 0) {
        return false;
    }
    g_workerThreadId = threadId;
    g_workerHandle = reinterpret_cast<HANDLE>(thread);
    return true;
}

void RequestShutdown() {
    g_shutdownRequested.store(true);
}

bool WaitForShutdown(std::uint32_t timeoutMilliseconds) {
    const HANDLE worker = g_workerHandle;
    if (worker == NULL || GetCurrentThreadId() == g_workerThreadId) {
        return g_workerComplete.load() && g_safeShutdownComplete.load();
    }
    const DWORD wait = WaitForSingleObject(worker, timeoutMilliseconds);
    return wait == WAIT_OBJECT_0 && g_safeShutdownComplete.load();
}

bool IsShutdownComplete() {
    return g_workerComplete.load() && g_safeShutdownComplete.load();
}

bool IsSupportedBuild() {
    return g_supportedBuild.load();
}

bool AreFixesActive() {
    return g_fixesActive.load();
}

bool IsShutdownRequested() {
    return g_shutdownRequested.load();
}

void OnProcessDetach() {
    RequestShutdown();
    if (g_workerHandle != NULL) {
        CloseHandle(g_workerHandle);
        g_workerHandle = NULL;
    }
}

} // namespace Runtime
} // namespace Client

extern "C" std::uint32_t CLIENT_CDECL Client_InterfaceVersion() {
    return 0x00010003u;
}

extern "C" void CLIENT_CDECL Client_RequestShutdown() {
    Client::Runtime::RequestShutdown();
}

extern "C" bool CLIENT_CDECL Client_WaitForShutdown(
    std::uint32_t timeoutMilliseconds) {
    return Client::Runtime::WaitForShutdown(timeoutMilliseconds);
}

extern "C" bool CLIENT_CDECL Client_IsShutdownComplete() {
    return Client::Runtime::IsShutdownComplete();
}

extern "C" bool CLIENT_CDECL Client_IsSupportedBuild() {
    return Client::Runtime::IsSupportedBuild();
}

extern "C" bool CLIENT_CDECL Client_AreFixesActive() {
    return Client::Runtime::AreFixesActive();
}

extern "C" bool CLIENT_CDECL Client_IsF4FixApplied() {
    return Client::LuaFixes::IsF4FixApplied();
}

extern "C" bool CLIENT_CDECL Client_IsKeybindFixApplied() {
    return Client::LuaFixes::IsKeybindApplied();
}

extern "C" bool CLIENT_CDECL Client_IsFovFeatureApplied() {
    return Client::LuaFixes::IsFovFeatureApplied();
}

extern "C" bool CLIENT_CDECL Client_IsPerformanceFeatureApplied() {
    return Client::LuaFixes::IsPerformanceFeatureApplied();
}

extern "C" bool CLIENT_CDECL Client_IsQualityOfLifeFeatureApplied() {
    return Client::LuaFixes::IsQualityOfLifeFeatureApplied();
}

extern "C" bool CLIENT_CDECL Client_IsLobbyBrowserFixApplied() {
    return Client::LuaFixes::IsLobbyBrowserFixApplied();
}

extern "C" bool CLIENT_CDECL Client_IsVersionOverrideApplied() {
    return Client::LuaFixes::IsVersionOverrideApplied();
}

extern "C" bool CLIENT_CDECL Client_IsStaticBundleProbeSeen() {
    return Client::LuaFixes::IsStaticBundleProbeSeen();
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        if (!Client::Runtime::Start(instance)) {
            return FALSE;
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        Client::Runtime::OnProcessDetach();
    }
    (void)reserved;
    return TRUE;
}
