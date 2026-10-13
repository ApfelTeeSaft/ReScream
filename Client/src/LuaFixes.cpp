#include "LuaFixes.hpp"

#include "Client/Addresses.hpp"
#include "EmbeddedLuaSources.hpp"
#include "Log.hpp"
#include "NativeQol.hpp"
#include "RuntimeInternal.hpp"
#include "ScreamSDK/Build.hpp"

#include <MinHook.h>
#include <Windows.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>

struct lua_State;

namespace Client {
namespace LuaFixes {

namespace {

#pragma pack(push, 1)
struct LuaInterpreterPrefix {
    lua_State* state;
    void* next;
    void* previous;
    std::int32_t classesReference;
    std::int32_t unusedVersionStorage;
    bool active;
};
#pragma pack(pop)

static_assert(offsetof(LuaInterpreterPrefix, state) == 0x00,
              "LuaInterface::L offset");
static_assert(offsetof(LuaInterpreterPrefix, active) == 0x14,
              "LuaInterpreter::_active offset");

typedef void (__thiscall* InputUpdate)(void* input);
typedef int (__cdecl* LuaGetTop)(lua_State* state);
typedef void (__cdecl* LuaSetTop)(lua_State* state, int index);
typedef int (__cdecl* LuaType)(lua_State* state, int index);
typedef int (__cdecl* LuaToBoolean)(lua_State* state, int index);
typedef const char* (__cdecl* LuaToString)(lua_State* state, int index,
                                           unsigned int* length);
typedef void (__cdecl* LuaPushBoolean)(lua_State* state, int value);
typedef void (__cdecl* LuaPushLString)(lua_State* state, const char* value,
                                      unsigned int length);
typedef void (__cdecl* LuaGetField)(lua_State* state, int index,
                                   const char* key);
typedef void (__cdecl* LuaSetField)(lua_State* state, int index,
                                    const char* key);
typedef int (__cdecl* LuaLoadBuffer)(lua_State* state, const char* buffer,
                                     unsigned int size, const char* name);
typedef int (__cdecl* LuaPCall)(lua_State* state, int arguments,
                                int results, int errorFunction);

struct LuaApi {
    LuaGetTop getTop;
    LuaSetTop setTop;
    LuaType type;
    LuaToBoolean toBoolean;
    LuaToString toString;
    LuaPushBoolean pushBoolean;
    LuaPushLString pushLString;
    LuaGetField getField;
    LuaSetField setField;
    LuaLoadBuffer loadBuffer;
    LuaPCall protectedCall;
};

InputUpdate g_originalUpdate = NULL;
void* g_target = NULL;
LuaApi g_lua = {};
std::mutex g_callbackMutex;
std::atomic<bool> g_f4FixApplied(false);
std::atomic<bool> g_keybindApplied(false);
std::atomic<bool> g_fovFeatureApplied(false);
std::atomic<bool> g_performanceFeatureApplied(false);
std::atomic<bool> g_qualityOfLifeFeatureApplied(false);
std::atomic<bool> g_lobbyBrowserFixApplied(false);
std::atomic<bool> g_versionOverrideApplied(false);
std::atomic<bool> g_staticBundleProbeSeen(false);
bool g_staticBundleProbeEnabled = false;
bool g_versionOverrideEnabled = true;
std::string g_displayVersion("0.3.0-RELEASE");
std::string g_lastFovStatus;
std::string g_lastPerformanceStatus;
std::string g_lastQualityOfLifeStatus;
lua_State* g_observedMainState = NULL;
unsigned int g_attempts = 0;
unsigned int g_failuresLogged = 0;

template <typename T>
T Resolve(std::uintptr_t rva) {
    return Scream::Build::ResolveRva<T>(rva);
}

bool IsExecutableAddress(const void* address) {
    if (address == NULL) {
        return false;
    }
    MEMORY_BASIC_INFORMATION information = {};
    if (VirtualQuery(address, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const DWORD protection = information.Protect & 0xFFu;
    return protection == PAGE_EXECUTE ||
           protection == PAGE_EXECUTE_READ ||
           protection == PAGE_EXECUTE_READWRITE ||
           protection == PAGE_EXECUTE_WRITECOPY;
}

bool ResolveLuaApi() {
    g_lua.getTop = Resolve<LuaGetTop>(Addresses::LuaGetTop);
    g_lua.setTop = Resolve<LuaSetTop>(Addresses::LuaSetTop);
    g_lua.type = Resolve<LuaType>(Addresses::LuaType);
    g_lua.toBoolean = Resolve<LuaToBoolean>(Addresses::LuaToBoolean);
    g_lua.toString = Resolve<LuaToString>(Addresses::LuaToString);
    g_lua.pushBoolean = Resolve<LuaPushBoolean>(Addresses::LuaPushBoolean);
    g_lua.pushLString = Resolve<LuaPushLString>(Addresses::LuaPushLString);
    g_lua.getField = Resolve<LuaGetField>(Addresses::LuaGetField);
    g_lua.setField = Resolve<LuaSetField>(Addresses::LuaSetField);
    g_lua.loadBuffer = Resolve<LuaLoadBuffer>(Addresses::LuaLLoadBuffer);
    g_lua.protectedCall = Resolve<LuaPCall>(Addresses::LuaPCall);
    const void* functions[] = {
        reinterpret_cast<const void*>(g_lua.getTop),
        reinterpret_cast<const void*>(g_lua.setTop),
        reinterpret_cast<const void*>(g_lua.type),
        reinterpret_cast<const void*>(g_lua.toBoolean),
        reinterpret_cast<const void*>(g_lua.toString),
        reinterpret_cast<const void*>(g_lua.pushBoolean),
        reinterpret_cast<const void*>(g_lua.pushLString),
        reinterpret_cast<const void*>(g_lua.getField),
        reinterpret_cast<const void*>(g_lua.setField),
        reinterpret_cast<const void*>(g_lua.loadBuffer),
        reinterpret_cast<const void*>(g_lua.protectedCall)
    };
    for (std::size_t i = 0; i < sizeof(functions) / sizeof(functions[0]); ++i) {
        if (!IsExecutableAddress(functions[i])) {
            return false;
        }
    }
    return true;
}

lua_State* MainState() {
    LuaInterpreterPrefix* owner = NULL;
#if defined(_MSC_VER)
    __try {
#endif
        LuaInterpreterPrefix** slot = Resolve<LuaInterpreterPrefix**>(
            Addresses::LuaInterpreterMain);
        owner = *slot;
        if (owner == NULL || !owner->active || owner->state == NULL) {
            return NULL;
        }
        return owner->state;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return NULL;
    }
#endif
}

void LogLuaFailure(lua_State* state, const char* patchName,
                   const char* operation, int status) {
    if (g_failuresLogged >= 3) {
        return;
    }
    ++g_failuresLogged;
    unsigned int length = 0;
    const char* message = g_lua.toString(state, -1, &length);
    CLIENT_LOG_WARNING("%s Lua patch %s failed (%d): %.*s",
                       patchName, operation, status,
                       message != NULL ? static_cast<int>(length) : 0,
                       message != NULL ? message : "");
}

bool ExecuteBooleanPatch(const char* patchName, const char* source,
                         unsigned int sourceLength, const char* chunkName) {
    lua_State* state = MainState();
    if (state == NULL) {
        return false;
    }

    const int originalTop = g_lua.getTop(state);
    const int loadStatus = g_lua.loadBuffer(
        state, source, sourceLength, chunkName);
    if (loadStatus != 0) {
        LogLuaFailure(state, patchName, "compile", loadStatus);
        g_lua.setTop(state, originalTop);
        return false;
    }
    const int callStatus = g_lua.protectedCall(state, 0, 1, 0);
    if (callStatus != 0) {
        LogLuaFailure(state, patchName, "execute", callStatus);
        g_lua.setTop(state, originalTop);
        return false;
    }
    const bool ready = g_lua.toBoolean(state, -1) != 0;
    g_lua.setTop(state, originalTop);
    return ready;
}

bool CallGlobalNoArguments(lua_State* state, const char* name) {
    if (state == NULL || name == NULL) {
        return false;
    }

    static const int kLuaGlobalsIndex = -10002;
    static const int kLuaTypeFunction = 6;
    const int originalTop = g_lua.getTop(state);
    g_lua.getField(state, kLuaGlobalsIndex, name);
    if (g_lua.type(state, -1) != kLuaTypeFunction) {
        g_lua.setTop(state, originalTop);
        return false;
    }
    const int status = g_lua.protectedCall(state, 0, 0, 0);
    if (status != 0) {
        LogLuaFailure(state, "quality-of-life watchdog", "execute", status);
        g_lua.setTop(state, originalTop);
        return false;
    }
    g_lua.setTop(state, originalTop);
    return true;
}

void PublishGlobalBoolean(lua_State* state, const char* name, bool value) {
    static const int kLuaGlobalsIndex = -10002;
    g_lua.pushBoolean(state, value ? 1 : 0);
    g_lua.setField(state, kLuaGlobalsIndex, name);
}

bool ReadGlobalBoolean(lua_State* state, const char* name) {
    static const int kLuaGlobalsIndex = -10002;
    const int originalTop = g_lua.getTop(state);
    g_lua.getField(state, kLuaGlobalsIndex, name);
    const bool value = g_lua.toBoolean(state, -1) != 0;
    g_lua.setTop(state, originalTop);
    return value;
}

std::string ReadGlobalString(const char* name) {
    lua_State* state = MainState();
    if (state == NULL || name == NULL) {
        return std::string();
    }

    static const int kLuaGlobalsIndex = -10002;
    const int originalTop = g_lua.getTop(state);
    g_lua.getField(state, kLuaGlobalsIndex, name);
    unsigned int length = 0;
    const char* value = g_lua.toString(state, -1, &length);
    const std::string result = value != NULL
        ? std::string(value, static_cast<std::size_t>(length))
        : std::string();
    g_lua.setTop(state, originalTop);
    return result;
}

void LogReadinessChange(const char* feature, const char* globalName,
                        std::string& previous) {
    const std::string current = ReadGlobalString(globalName);
    if (current.empty() || current == previous) {
        return;
    }
    previous = current;
    CLIENT_LOG_INFO("%s readiness: %s", feature, current.c_str());
}

bool TryApplyF4() {
    const bool ready = ExecuteBooleanPatch(
        "F4", kF4FixSource,
        static_cast<unsigned int>(sizeof(kF4FixSource) - 1),
        "@ReScream/client/F4Fix.lua");
    if (ready && !g_f4FixApplied.exchange(true)) {
        CLIENT_LOG_INFO(
            "disabled stale switchable HUD layouts in tweak_data.version_config");
    }
    return ready;
}

bool TryApplyKeybind() {
    const bool ready = ExecuteBooleanPatch(
        "keybind", kKeybindFixSource,
        static_cast<unsigned int>(sizeof(kKeybindFixSource) - 1),
        "@ReScream/client/KeybindFix.lua");
    if (ready && !g_keybindApplied.exchange(true)) {
        CLIENT_LOG_INFO(
            "normalized absent PC binding names before menu rendering");
    }
    return ready;
}

bool TryApplyFovFeature() {
    const bool ready = ExecuteBooleanPatch(
        "FoV feature", kFovFeatureSource,
        static_cast<unsigned int>(sizeof(kFovFeatureSource) - 1),
        "@ReScream/client/ReScreamFov.lua");
    LogReadinessChange(
        "FoV feature", "__rescream_fov_status", g_lastFovStatus);
    if (ready && !g_fovFeatureApplied.exchange(true)) {
        CLIENT_LOG_INFO(
            "FoV feature applied: added the persistent ReScream settings menu "
            "and camera baseline hook");
    }
    return ready;
}

bool TryApplyPerformanceFeature() {
    const bool ready = ExecuteBooleanPatch(
        "performance feature", kPerformanceFeatureSource,
        static_cast<unsigned int>(sizeof(kPerformanceFeatureSource) - 1),
        "@ReScream/client/ReScreamPerformance.lua");
    LogReadinessChange(
        "Performance feature", "__rescream_performance_status",
        g_lastPerformanceStatus);
    if (ready && !g_performanceFeatureApplied.exchange(true)) {
        CLIENT_LOG_INFO(
            "Performance feature applied: added persistent graphics and "
            "rain-particle toggles to the ReScream menu");
    }
    return ready;
}

bool TryApplyQualityOfLifeFeature() {
    const bool ready = ExecuteBooleanPatch(
        "quality-of-life feature", kQualityOfLifeFeatureSource,
        static_cast<unsigned int>(
            sizeof(kQualityOfLifeFeatureSource) - 1),
        "@ReScream/client/ReScreamQualityOfLife.lua");
    LogReadinessChange(
        "Quality-of-life feature", "__rescream_quality_of_life_status",
        g_lastQualityOfLifeStatus);
    if (ready && !g_qualityOfLifeFeatureApplied.exchange(true)) {
        CLIENT_LOG_INFO(
            "Quality-of-life feature applied: added persistent display-mode "
            "and debug-UI pages to the ReScream menu");
    }
    return ready;
}

bool TryApplyLobbyBrowserFix() {
    const bool ready = ExecuteBooleanPatch(
        "lobby browser", kLobbyBrowserFixSource,
        static_cast<unsigned int>(sizeof(kLobbyBrowserFixSource) - 1),
        "@ReScream/client/LobbyBrowserFix.lua");
    if (ready && !g_lobbyBrowserFixApplied.exchange(true)) {
        CLIENT_LOG_INFO(
            "Lobby browser fix applied: restored Steam filters before refresh "
            "and subscribed to asynchronous lobby-data updates");
    }
    return ready;
}

bool TryApplyVersionOverride() {
    lua_State* state = MainState();
    if (state == NULL || g_displayVersion.empty()) {
        return false;
    }

    static const int kLuaGlobalsIndex = -10002;
    static const char kGlobalName[] = "__rescream_display_version_override";
    g_lua.pushLString(
        state, g_displayVersion.c_str(),
        static_cast<unsigned int>(g_displayVersion.size()));
    g_lua.setField(state, kLuaGlobalsIndex, kGlobalName);

    const bool ready = ExecuteBooleanPatch(
        "display version", kVersionOverrideSource,
        static_cast<unsigned int>(sizeof(kVersionOverrideSource) - 1),
        "@ReScream/client/VersionOverride.lua");
    if (ready && !g_versionOverrideApplied.exchange(true)) {
        CLIENT_LOG_INFO(
            "Display version override applied: v%s (network server_version unchanged)",
            g_displayVersion.c_str());
    }
    return ready;
}

bool TryObserveStaticBundleProbe() {
    const bool seen = ExecuteBooleanPatch(
        "static bundle probe", kStaticBundleProbeSource,
        static_cast<unsigned int>(sizeof(kStaticBundleProbeSource) - 1),
        "@ReScream/client/StaticBundleProbe.lua");
    if (seen && !g_staticBundleProbeSeen.exchange(true)) {
        CLIENT_LOG_INFO(
            "Static bundle probe observed: rebuilt LHD/FCL loaded float-Lua 5.1 bytecode");
    }
    return seen;
}

void ScheduleAllLuaFeaturesForRepair() {
    g_f4FixApplied.store(false);
    g_keybindApplied.store(false);
    g_fovFeatureApplied.store(false);
    g_performanceFeatureApplied.store(false);
    g_qualityOfLifeFeatureApplied.store(false);
    g_lobbyBrowserFixApplied.store(false);
    g_versionOverrideApplied.store(false);
    g_staticBundleProbeSeen.store(false);
    g_lastFovStatus.clear();
    g_lastPerformanceStatus.clear();
    g_lastQualityOfLifeStatus.clear();
    g_attempts = 0;
    g_failuresLogged = 0;
}

void ObserveMainState(lua_State* state) {
    if (state == g_observedMainState) {
        return;
    }
    if (g_observedMainState != NULL) {
        CLIENT_LOG_INFO(
            state != NULL
                ? "Main Lua state changed; scheduling all Lua features for repair"
                : "Main Lua state became unavailable; scheduling all Lua features for repair");
        ScheduleAllLuaFeaturesForRepair();
    }
    g_observedMainState = state;
}

void MaintainF4Watchdog(lua_State* state, bool attemptInstall) {
    if (state == NULL) {
        return;
    }
    if (g_f4FixApplied.load()) {
        if (CallGlobalNoArguments(state, "__rescream_f4_fix_tick")) {
            return;
        }
        g_f4FixApplied.store(false);
        TryApplyF4();
        return;
    }
    if (attemptInstall) {
        TryApplyF4();
    }
}

void UpdateNativeQolBridge(lua_State* state, bool eagerF4Repair) {
    if (state == NULL) {
        NativeQol::Tick(false, false);
        return;
    }

    static const int kLuaGlobalsIndex = -10002;
    PublishGlobalBoolean(
        state, "__rescream_native_qol_driver", true);
    MaintainF4Watchdog(state, eagerF4Repair);
    PublishGlobalBoolean(
        state, "__rescream_native_borderless_active",
        NativeQol::IsBorderlessApplied());

    char debugText[256] = {};
    NativeQol::FormatNetworkDebugText(debugText, sizeof(debugText));
    g_lua.pushLString(
        state, debugText,
        static_cast<unsigned int>(std::strlen(debugText)));
    g_lua.setField(
        state, kLuaGlobalsIndex, "__rescream_network_debug_text");

    CallGlobalNoArguments(state, "__rescream_quality_of_life_tick");

    const bool borderlessRequested = ReadGlobalBoolean(
        state, "__rescream_native_borderless_requested");
    const bool rendererTransitionPending = ReadGlobalBoolean(
        state, "__rescream_renderer_transition_pending");
    NativeQol::Tick(borderlessRequested, rendererTransitionPending);
}

void __fastcall UpdateDetour(void* input, void*) {
    std::lock_guard<std::mutex> callbackLock(g_callbackMutex);
    lua_State* stateBeforeUpdate = MainState();
    const bool stateChangedBeforeUpdate =
        stateBeforeUpdate != g_observedMainState;
    ObserveMainState(stateBeforeUpdate);
    const bool scheduledF4Repair = stateChangedBeforeUpdate ||
        g_attempts == 0 || (g_attempts % 30u) == 0u;
    MaintainF4Watchdog(stateBeforeUpdate, scheduledF4Repair);
    if (g_originalUpdate != NULL) {
        g_originalUpdate(input);
    }
    lua_State* state = MainState();
    const bool stateChangedAfterUpdate = state != g_observedMainState;
    ObserveMainState(state);
    UpdateNativeQolBridge(state, stateChangedAfterUpdate);
    if (Runtime::IsShutdownRequested() ||
        (g_f4FixApplied.load() && g_keybindApplied.load() &&
         g_fovFeatureApplied.load() && g_lobbyBrowserFixApplied.load() &&
         g_performanceFeatureApplied.load() &&
         g_qualityOfLifeFeatureApplied.load() &&
         (!g_versionOverrideEnabled || g_versionOverrideApplied.load()) &&
         (!g_staticBundleProbeEnabled || g_staticBundleProbeSeen.load() ||
          g_attempts > 1800u))) {
        return;
    }

    ++g_attempts;
    if (g_attempts == 1 || (g_attempts % 30u) == 0u) {
        if (!g_f4FixApplied.load()) {
            TryApplyF4();
        }
        if (!g_keybindApplied.load()) {
            TryApplyKeybind();
        }
        if (!g_fovFeatureApplied.load()) {
            TryApplyFovFeature();
        }
        if (!g_performanceFeatureApplied.load()) {
            TryApplyPerformanceFeature();
        }
        if (!g_qualityOfLifeFeatureApplied.load()) {
            TryApplyQualityOfLifeFeature();
        }
        if (!g_lobbyBrowserFixApplied.load()) {
            TryApplyLobbyBrowserFix();
        }
        if (g_versionOverrideEnabled &&
            !g_versionOverrideApplied.load()) {
            TryApplyVersionOverride();
        }
        if (g_staticBundleProbeEnabled &&
            !g_staticBundleProbeSeen.load() && g_attempts <= 1800u) {
            TryObserveStaticBundleProbe();
        }
    }
}

} // namespace

bool Install() {
    char probeValue[8] = {};
    const DWORD probeLength = GetEnvironmentVariableA(
        "RESCREAM_STATIC_PROBE", probeValue,
        static_cast<DWORD>(sizeof(probeValue)));
    g_staticBundleProbeEnabled =
        probeLength == 1 && probeValue[0] == '1';
    if (g_staticBundleProbeEnabled) {
        CLIENT_LOG_INFO(
            "Static bundle probe diagnostics enabled by RESCREAM_STATIC_PROBE=1");
    }
    char versionValue[49] = {};
    const DWORD versionLength = GetEnvironmentVariableA(
        "RESCREAM_DISPLAY_VERSION", versionValue,
        static_cast<DWORD>(sizeof(versionValue)));
    if (versionLength > 0 && versionLength < sizeof(versionValue)) {
        bool valid = true;
        for (DWORD i = 0; i < versionLength; ++i) {
            const unsigned char value =
                static_cast<unsigned char>(versionValue[i]);
            if (value < 0x20u || value == 0x7Fu) {
                valid = false;
                break;
            }
        }
        if (valid) {
            g_displayVersion.assign(versionValue, versionLength);
            g_versionOverrideEnabled = true;
            CLIENT_LOG_INFO(
                "Display version override configured by "
                "RESCREAM_DISPLAY_VERSION=%s",
                g_displayVersion.c_str());
        } else {
            CLIENT_LOG_WARNING(
                "Ignoring RESCREAM_DISPLAY_VERSION because it contains "
                "control characters");
        }
    } else if (versionLength >= sizeof(versionValue)) {
        CLIENT_LOG_WARNING(
            "Ignoring RESCREAM_DISPLAY_VERSION because it exceeds 48 bytes");
    }
    if (!ResolveLuaApi()) {
        CLIENT_LOG_ERROR("Lua fix scheduler could not validate the linked Lua ABI addresses");
        return false;
    }
    g_target = Resolve<void*>(Addresses::InputUpdate);
    if (!IsExecutableAddress(g_target)) {
        CLIENT_LOG_ERROR("Lua fix scheduler Input::update target is not executable");
        return false;
    }

    MH_STATUS status = MH_CreateHook(
        g_target, reinterpret_cast<void*>(&UpdateDetour),
        reinterpret_cast<void**>(&g_originalUpdate));
    if (status != MH_OK) {
        CLIENT_LOG_ERROR("Could not create Input::update hook: %s",
                         MH_StatusToString(status));
        g_target = NULL;
        return false;
    }
    status = MH_EnableHook(g_target);
    if (status != MH_OK) {
        CLIENT_LOG_ERROR("Could not enable Input::update hook: %s",
                         MH_StatusToString(status));
        MH_RemoveHook(g_target);
        g_target = NULL;
        g_originalUpdate = NULL;
        return false;
    }
    CLIENT_LOG_INFO("Installed Lua fix scheduler on Input::update target=%p",
                    g_target);
    return true;
}

void Quiesce() {
    std::lock_guard<std::mutex> callbackLock(g_callbackMutex);
}

bool Remove() {
    if (g_target == NULL) {
        return true;
    }
    const MH_STATUS status = MH_RemoveHook(g_target);
    if (status != MH_OK) {
        CLIENT_LOG_ERROR("Could not remove Input::update hook: %s",
                         MH_StatusToString(status));
        return false;
    }
    g_target = NULL;
    g_originalUpdate = NULL;
    return true;
}

bool IsF4FixApplied() {
    return g_f4FixApplied.load();
}

bool IsKeybindApplied() {
    return g_keybindApplied.load();
}

bool IsFovFeatureApplied() {
    return g_fovFeatureApplied.load();
}

bool IsPerformanceFeatureApplied() {
    return g_performanceFeatureApplied.load();
}

bool IsQualityOfLifeFeatureApplied() {
    return g_qualityOfLifeFeatureApplied.load() && NativeQol::IsInstalled();
}

bool IsLobbyBrowserFixApplied() {
    return g_lobbyBrowserFixApplied.load();
}

bool IsVersionOverrideApplied() {
    return g_versionOverrideApplied.load();
}

bool IsStaticBundleProbeSeen() {
    return g_staticBundleProbeSeen.load();
}

} // namespace LuaFixes
} // namespace Client
