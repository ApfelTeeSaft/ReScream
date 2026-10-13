#include "NativeQol.hpp"

#include "Client/Addresses.hpp"
#include "Log.hpp"
#include "ScreamSDK/Build.hpp"

#include <MinHook.h>
#include <Windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <process.h>

namespace Client {
namespace NativeQol {

namespace {

typedef int (__thiscall* SteamSocketSendTo)(
    void* socket, const void* buffer, int bufferSize,
    const void* target, int sendType);
typedef int (__thiscall* SteamSocketRecvFrom)(
    void* socket, void* buffer, int bufferSize, void* source);

#pragma pack(push, 1)
struct ApplicationPrefix {
    std::uint8_t unknown000[0x1EC];
    void* renderInterface;
};

struct RenderInterfacePrefix {
    std::uint8_t unknown000[0x14];
    void* window;
};

struct Win32WindowPrefix {
    HWND hwnd;
};
#pragma pack(pop)

static_assert(offsetof(ApplicationPrefix, renderInterface) == 0x1EC,
              "Application::_render_interface offset");
static_assert(offsetof(RenderInterfacePrefix, window) == 0x14,
              "RenderInterface::_window offset");

SteamSocketSendTo g_originalSendTo = NULL;
SteamSocketRecvFrom g_originalRecvFrom = NULL;
void* g_sendTarget = NULL;
void* g_recvTarget = NULL;
std::atomic<unsigned long long> g_sentPackets(0);
std::atomic<unsigned long long> g_sentBytes(0);
std::atomic<unsigned long long> g_receivedPackets(0);
std::atomic<unsigned long long> g_receivedBytes(0);
std::atomic<unsigned long> g_networkCallbacks(0);
std::atomic<bool> g_networkStopping(false);
bool g_installed = false;

HANDLE g_borderlessWorker = NULL;
HANDLE g_borderlessWakeEvent = NULL;
std::atomic<bool> g_borderlessWorkerStop(false);
std::atomic<bool> g_borderlessAppliedPublished(false);

struct BorderlessCommand {
    HWND window;
    bool requested;
    bool rendererTransitionPending;
    unsigned long generation;
};

SRWLOCK g_borderlessCommandLock = SRWLOCK_INIT;
BorderlessCommand g_borderlessCommand = {NULL, false, false, 0};

HWND g_borderlessWindow = NULL;
LONG_PTR g_originalStyle = 0;
LONG_PTR g_originalExtendedStyle = 0;
RECT g_originalRect = {};
WINDOWPLACEMENT g_originalPlacement = {sizeof(WINDOWPLACEMENT)};
bool g_borderlessApplied = false;
bool g_borderlessRestorePending = false;
HWND g_borderlessCandidateWindow = NULL;
LONG_PTR g_borderlessCandidateStyle = 0;
LONG_PTR g_borderlessCandidateExtendedStyle = 0;
RECT g_borderlessCandidateRect = {};
DWORD g_borderlessRequestTick = 0;
DWORD g_borderlessCandidateStableTick = 0;
bool g_borderlessWasRequested = false;
DWORD g_borderlessRepairTick = 0;

const DWORD kBorderlessRequestGraceMs = 900u;
const DWORD kBorderlessCandidateStableMs = 150u;
const DWORD kNetworkQuiesceTimeoutMs = 5000u;
const DWORD kNetworkStableZeroMs = 50u;

DWORD g_rateTick = 0;
unsigned long long g_previousSentPackets = 0;
unsigned long long g_previousSentBytes = 0;
unsigned long long g_previousReceivedPackets = 0;
unsigned long long g_previousReceivedBytes = 0;
unsigned long long g_sentPacketsPerSecond = 0;
unsigned long long g_sentBytesPerSecond = 0;
unsigned long long g_receivedPacketsPerSecond = 0;
unsigned long long g_receivedBytesPerSecond = 0;

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

HWND GameWindow() {
#if defined(_MSC_VER)
    __try {
#endif
        ApplicationPrefix** applicationSlot =
            Resolve<ApplicationPrefix**>(Addresses::Application);
        ApplicationPrefix* application = *applicationSlot;
        if (application == NULL || application->renderInterface == NULL) {
            return NULL;
        }
        RenderInterfacePrefix* renderInterface =
            static_cast<RenderInterfacePrefix*>(application->renderInterface);
        if (renderInterface->window == NULL) {
            return NULL;
        }
        Win32WindowPrefix* window =
            static_cast<Win32WindowPrefix*>(renderInterface->window);
        return IsWindow(window->hwnd) ? window->hwnd : NULL;
#if defined(_MSC_VER)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return NULL;
    }
#endif
}

bool RectanglesEqual(const RECT& left, const RECT& right) {
    return left.left == right.left && left.top == right.top &&
           left.right == right.right && left.bottom == right.bottom;
}

bool PlacementsEqual(const WINDOWPLACEMENT& left,
                     const WINDOWPLACEMENT& right) {
    return (left.flags & WPF_RESTORETOMAXIMIZED) ==
               (right.flags & WPF_RESTORETOMAXIMIZED) &&
           left.showCmd == right.showCmd &&
           RectanglesEqual(left.rcNormalPosition, right.rcNormalPosition);
}

bool BorderlessCommandsEqual(const BorderlessCommand& left,
                             const BorderlessCommand& right) {
    return left.generation == right.generation &&
           left.window == right.window &&
           left.requested == right.requested &&
           left.rendererTransitionPending ==
               right.rendererTransitionPending;
}

BorderlessCommand SnapshotBorderlessCommand() {
    AcquireSRWLockShared(&g_borderlessCommandLock);
    const BorderlessCommand command = g_borderlessCommand;
    ReleaseSRWLockShared(&g_borderlessCommandLock);
    return command;
}

bool BorderlessCommandMatches(const BorderlessCommand& expected) {
    AcquireSRWLockShared(&g_borderlessCommandLock);
    const bool matches =
        BorderlessCommandsEqual(g_borderlessCommand, expected);
    ReleaseSRWLockShared(&g_borderlessCommandLock);
    return matches;
}

bool PublishBorderlessApplied(const BorderlessCommand& expected) {
    AcquireSRWLockShared(&g_borderlessCommandLock);
    const bool matches =
        BorderlessCommandsEqual(g_borderlessCommand, expected) &&
        expected.requested && !expected.rendererTransitionPending;
    if (matches) {
        g_borderlessAppliedPublished.store(true);
    }
    ReleaseSRWLockShared(&g_borderlessCommandLock);
    return matches;
}

void PublishBorderlessCommand(HWND window, bool requested,
                               bool rendererTransitionPending) {
    AcquireSRWLockExclusive(&g_borderlessCommandLock);
    if (g_borderlessCommand.window != window ||
        g_borderlessCommand.requested != requested ||
        g_borderlessCommand.rendererTransitionPending !=
            rendererTransitionPending) {
        g_borderlessCommand.window = window;
        g_borderlessCommand.requested = requested;
        g_borderlessCommand.rendererTransitionPending =
            rendererTransitionPending;
        ++g_borderlessCommand.generation;
    }
    ReleaseSRWLockExclusive(&g_borderlessCommandLock);
}

void ClearBorderlessOwnership() {
    g_borderlessWindow = NULL;
    g_borderlessApplied = false;
    g_borderlessRestorePending = false;
    g_borderlessRepairTick = 0;
    g_borderlessAppliedPublished.store(false);
}

void ResetBorderlessCandidate() {
    g_borderlessCandidateWindow = NULL;
    g_borderlessCandidateStyle = 0;
    g_borderlessCandidateExtendedStyle = 0;
    g_borderlessCandidateRect = RECT{};
    g_borderlessCandidateStableTick = 0;
}

bool RestoreWindow(const BorderlessCommand* expectedCommand = NULL) {
    if (!g_borderlessApplied || !IsWindow(g_borderlessWindow)) {
        ClearBorderlessOwnership();
        return true;
    }
    if (expectedCommand != NULL &&
        !BorderlessCommandMatches(*expectedCommand)) {
        return false;
    }

    g_borderlessRestorePending = true;
    SetWindowLongPtrA(g_borderlessWindow, GWL_STYLE, g_originalStyle);
    SetWindowLongPtrA(
        g_borderlessWindow, GWL_EXSTYLE, g_originalExtendedStyle);
    const BOOL placementRestored =
        SetWindowPlacement(g_borderlessWindow, &g_originalPlacement);
    UINT positionFlags = SWP_FRAMECHANGED | SWP_NOACTIVATE |
        SWP_NOOWNERZORDER | SWP_NOZORDER;
    int x = g_originalRect.left;
    int y = g_originalRect.top;
    int width = g_originalRect.right - g_originalRect.left;
    int height = g_originalRect.bottom - g_originalRect.top;
    if (placementRestored) {
        positionFlags |= SWP_NOMOVE | SWP_NOSIZE;
        x = y = width = height = 0;
    }
    const BOOL positionRestored = SetWindowPos(
        g_borderlessWindow, NULL, x, y, width, height, positionFlags);
    if (!IsWindow(g_borderlessWindow)) {
        ClearBorderlessOwnership();
        return true;
    }
    RECT restoredRect = {};
    WINDOWPLACEMENT restoredPlacement = {sizeof(WINDOWPLACEMENT)};
    const bool placementMatches = placementRestored &&
        GetWindowPlacement(g_borderlessWindow, &restoredPlacement) &&
        PlacementsEqual(restoredPlacement, g_originalPlacement);
    const bool rectMatches =
        GetWindowRect(g_borderlessWindow, &restoredRect) &&
        RectanglesEqual(restoredRect, g_originalRect);
    const bool restored = positionRestored &&
        GetWindowLongPtrA(g_borderlessWindow, GWL_STYLE) ==
            g_originalStyle &&
        GetWindowLongPtrA(g_borderlessWindow, GWL_EXSTYLE) ==
            g_originalExtendedStyle &&
        (placementMatches || rectMatches);
    if (!restored) {
        CLIENT_LOG_WARNING(
            "Could not fully restore the game's framed window; retrying before renderer transition");
        g_borderlessAppliedPublished.store(true);
        return false;
    }
    CLIENT_LOG_INFO("Restored the game's windowed frame after borderless mode");
    ClearBorderlessOwnership();
    return true;
}

bool CaptureAndApplyBorderless(HWND hwnd,
                               const BorderlessCommand& command) {
    if (!BorderlessCommandMatches(command) || !command.requested ||
        command.rendererTransitionPending || command.window != hwnd) {
        return false;
    }
    if (g_borderlessApplied && g_borderlessWindow != hwnd) {
        if (!RestoreWindow(&command)) {
            return false;
        }
        if (!BorderlessCommandMatches(command)) {
            return false;
        }
    }

    MONITORINFO monitor = {sizeof(MONITORINFO)};
    const HMONITOR monitorHandle =
        MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (monitorHandle == NULL || !GetMonitorInfoA(monitorHandle, &monitor)) {
        return false;
    }

    bool newlyCaptured = false;
    if (!g_borderlessApplied) {
        WINDOWPLACEMENT placement = {sizeof(WINDOWPLACEMENT)};
        RECT rect = {};
        if (!GetWindowPlacement(hwnd, &placement) ||
            !GetWindowRect(hwnd, &rect)) {
            return false;
        }
        g_borderlessWindow = hwnd;
        g_originalStyle = GetWindowLongPtrA(hwnd, GWL_STYLE);
        g_originalExtendedStyle = GetWindowLongPtrA(hwnd, GWL_EXSTYLE);
        g_originalRect = rect;
        g_originalPlacement = placement;
        g_borderlessApplied = true;
        newlyCaptured = true;
    }

    LONG_PTR desiredStyle = g_originalStyle;
    desiredStyle &= ~(static_cast<LONG_PTR>(WS_CAPTION) |
                      static_cast<LONG_PTR>(WS_THICKFRAME) |
                      static_cast<LONG_PTR>(WS_MINIMIZEBOX) |
                      static_cast<LONG_PTR>(WS_MAXIMIZEBOX) |
                      static_cast<LONG_PTR>(WS_SYSMENU));
    desiredStyle |= WS_POPUP | WS_VISIBLE;

    LONG_PTR desiredExtendedStyle = g_originalExtendedStyle;
    desiredExtendedStyle &=
        ~(static_cast<LONG_PTR>(WS_EX_DLGMODALFRAME) |
          static_cast<LONG_PTR>(WS_EX_CLIENTEDGE) |
          static_cast<LONG_PTR>(WS_EX_STATICEDGE) |
          static_cast<LONG_PTR>(WS_EX_WINDOWEDGE));

    RECT currentRect = {};
    const RECT& target = monitor.rcMonitor;
    const bool wrongRect = !GetWindowRect(hwnd, &currentRect) ||
        currentRect.left != target.left || currentRect.top != target.top ||
        currentRect.right != target.right ||
        currentRect.bottom != target.bottom;
    const bool wrongStyle =
        GetWindowLongPtrA(hwnd, GWL_STYLE) != desiredStyle;
    const bool wrongExtendedStyle =
        GetWindowLongPtrA(hwnd, GWL_EXSTYLE) != desiredExtendedStyle;

    if (wrongStyle) {
        if (!BorderlessCommandMatches(command)) {
            RestoreWindow();
            return false;
        }
        SetWindowLongPtrA(hwnd, GWL_STYLE, desiredStyle);
        if (!BorderlessCommandMatches(command)) {
            RestoreWindow();
            return false;
        }
    }
    if (wrongExtendedStyle) {
        if (!BorderlessCommandMatches(command)) {
            RestoreWindow();
            return false;
        }
        SetWindowLongPtrA(hwnd, GWL_EXSTYLE, desiredExtendedStyle);
        if (!BorderlessCommandMatches(command)) {
            RestoreWindow();
            return false;
        }
    }
    if (wrongRect || wrongStyle || wrongExtendedStyle) {
        if (!BorderlessCommandMatches(command)) {
            RestoreWindow();
            return false;
        }
        SetWindowPos(
            hwnd, NULL, target.left, target.top,
            target.right - target.left, target.bottom - target.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOOWNERZORDER |
                SWP_NOZORDER);
        if (!BorderlessCommandMatches(command)) {
            RestoreWindow();
            return false;
        }
    }

    RECT appliedRect = {};
    const bool applied =
        GetWindowLongPtrA(hwnd, GWL_STYLE) == desiredStyle &&
        GetWindowLongPtrA(hwnd, GWL_EXSTYLE) == desiredExtendedStyle &&
        GetWindowRect(hwnd, &appliedRect) &&
        RectanglesEqual(appliedRect, target);
    if (!applied) {
        CLIENT_LOG_WARNING(
            "Borderless window update was rejected; restoring the framed window");
        RestoreWindow();
        return false;
    }
    if (!PublishBorderlessApplied(command)) {
        RestoreWindow();
        return false;
    }
    if (newlyCaptured) {
        CLIENT_LOG_INFO("Applied borderless-window mode to HWND=%p", hwnd);
    }
    g_borderlessRestorePending = false;
    return true;
}

bool BorderlessCandidateIsStable(HWND hwnd, DWORD now) {
    if (hwnd == NULL || IsIconic(hwnd)) {
        ResetBorderlessCandidate();
        return false;
    }

    const LONG_PTR style = GetWindowLongPtrA(hwnd, GWL_STYLE);
    const LONG_PTR extendedStyle = GetWindowLongPtrA(hwnd, GWL_EXSTYLE);
    RECT rect = {};
    if (!GetWindowRect(hwnd, &rect)) {
        ResetBorderlessCandidate();
        return false;
    }

    if ((style & WS_CAPTION) != WS_CAPTION) {
        ResetBorderlessCandidate();
        return false;
    }

    if (g_borderlessCandidateWindow != hwnd ||
        g_borderlessCandidateStyle != style ||
        g_borderlessCandidateExtendedStyle != extendedStyle ||
        !RectanglesEqual(g_borderlessCandidateRect, rect)) {
        g_borderlessCandidateWindow = hwnd;
        g_borderlessCandidateStyle = style;
        g_borderlessCandidateExtendedStyle = extendedStyle;
        g_borderlessCandidateRect = rect;
        g_borderlessCandidateStableTick = now;
        return false;
    }
    return now - g_borderlessCandidateStableTick >=
           kBorderlessCandidateStableMs;
}

bool BorderlessConfigurationMatches(HWND hwnd) {
    if (!IsWindow(hwnd)) {
        return false;
    }
    MONITORINFO monitor = {sizeof(MONITORINFO)};
    const HMONITOR monitorHandle =
        MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    if (monitorHandle == NULL || !GetMonitorInfoA(monitorHandle, &monitor)) {
        return false;
    }
    LONG_PTR desiredStyle = g_originalStyle;
    desiredStyle &= ~(static_cast<LONG_PTR>(WS_CAPTION) |
                      static_cast<LONG_PTR>(WS_THICKFRAME) |
                      static_cast<LONG_PTR>(WS_MINIMIZEBOX) |
                      static_cast<LONG_PTR>(WS_MAXIMIZEBOX) |
                      static_cast<LONG_PTR>(WS_SYSMENU));
    desiredStyle |= WS_POPUP | WS_VISIBLE;
    LONG_PTR desiredExtendedStyle = g_originalExtendedStyle;
    desiredExtendedStyle &=
        ~(static_cast<LONG_PTR>(WS_EX_DLGMODALFRAME) |
          static_cast<LONG_PTR>(WS_EX_CLIENTEDGE) |
          static_cast<LONG_PTR>(WS_EX_STATICEDGE) |
          static_cast<LONG_PTR>(WS_EX_WINDOWEDGE));
    RECT rect = {};
    return GetWindowLongPtrA(hwnd, GWL_STYLE) == desiredStyle &&
           GetWindowLongPtrA(hwnd, GWL_EXSTYLE) == desiredExtendedStyle &&
           GetWindowRect(hwnd, &rect) &&
           RectanglesEqual(rect, monitor.rcMonitor);
}

class NetworkCallbackScope {
public:
    NetworkCallbackScope() {
        g_networkCallbacks.fetch_add(1, std::memory_order_acq_rel);
    }
    ~NetworkCallbackScope() {
        g_networkCallbacks.fetch_sub(1, std::memory_order_acq_rel);
    }
private:
    NetworkCallbackScope(const NetworkCallbackScope&);
    NetworkCallbackScope& operator=(const NetworkCallbackScope&);
};

int __fastcall SendToDetour(
    void* socket, void*, const void* buffer, int bufferSize,
    const void* target, int sendType) {
    NetworkCallbackScope callback;
    SteamSocketSendTo original = g_originalSendTo;
    const int result = original(
        socket, buffer, bufferSize, target, sendType);
    if (result > 0 && !g_networkStopping.load()) {
        g_sentPackets.fetch_add(1, std::memory_order_relaxed);
        g_sentBytes.fetch_add(
            static_cast<unsigned long long>(result),
            std::memory_order_relaxed);
    }
    return result;
}

int __fastcall RecvFromDetour(
    void* socket, void*, void* buffer, int bufferSize, void* source) {
    NetworkCallbackScope callback;
    SteamSocketRecvFrom original = g_originalRecvFrom;
    const int result = original(
        socket, buffer, bufferSize, source);
    if (result > 0 && result <= bufferSize &&
        !g_networkStopping.load()) {
        g_receivedPackets.fetch_add(1, std::memory_order_relaxed);
        g_receivedBytes.fetch_add(
            static_cast<unsigned long long>(result),
            std::memory_order_relaxed);
    }
    return result;
}

bool DisableNetworkHook(void* target, const char* name) {
    if (target == NULL) {
        return true;
    }
    const MH_STATUS status = MH_DisableHook(target);
    if (status == MH_OK || status == MH_ERROR_DISABLED ||
        status == MH_ERROR_NOT_CREATED) {
        return true;
    }
    CLIENT_LOG_ERROR("Could not disable Steam %s hook: %s", name,
                     MH_StatusToString(status));
    return false;
}

bool RemoveNetworkHook(void* target, const char* name) {
    if (target == NULL) {
        return true;
    }
    const MH_STATUS status = MH_RemoveHook(target);
    if (status == MH_OK || status == MH_ERROR_NOT_CREATED) {
        return true;
    }
    CLIENT_LOG_ERROR("Could not remove Steam %s hook: %s", name,
                     MH_StatusToString(status));
    return false;
}

bool WaitForNetworkCallbacksToQuiesce() {
    const DWORD start = GetTickCount();
    DWORD zeroSince = 0;
    bool zeroObserved = false;
    for (;;) {
        const DWORD now = GetTickCount();
        const unsigned long active =
            g_networkCallbacks.load(std::memory_order_acquire);
        if (active == 0) {
            if (!zeroObserved) {
                zeroObserved = true;
                zeroSince = now;
            } else if (now - zeroSince >= kNetworkStableZeroMs) {
                return true;
            }
        } else {
            zeroObserved = false;
        }
        if (now - start >= kNetworkQuiesceTimeoutMs) {
            CLIENT_LOG_ERROR(
                "Steam packet hooks did not quiesce (%lu callback(s) active); retaining client.dll safely",
                active);
            return false;
        }
        Sleep(1u);
    }
}

void UpdateRates() {
    const DWORD now = GetTickCount();
    if (g_rateTick == 0) {
        g_rateTick = now;
        return;
    }
    const DWORD elapsed = now - g_rateTick;
    if (elapsed < 500u) {
        return;
    }

    const unsigned long long sentPackets =
        g_sentPackets.load(std::memory_order_relaxed);
    const unsigned long long sentBytes =
        g_sentBytes.load(std::memory_order_relaxed);
    const unsigned long long receivedPackets =
        g_receivedPackets.load(std::memory_order_relaxed);
    const unsigned long long receivedBytes =
        g_receivedBytes.load(std::memory_order_relaxed);
    const unsigned long long scale = 1000ull;
    g_sentPacketsPerSecond =
        (sentPackets - g_previousSentPackets) * scale / elapsed;
    g_sentBytesPerSecond =
        (sentBytes - g_previousSentBytes) * scale / elapsed;
    g_receivedPacketsPerSecond =
        (receivedPackets - g_previousReceivedPackets) * scale / elapsed;
    g_receivedBytesPerSecond =
        (receivedBytes - g_previousReceivedBytes) * scale / elapsed;
    g_previousSentPackets = sentPackets;
    g_previousSentBytes = sentBytes;
    g_previousReceivedPackets = receivedPackets;
    g_previousReceivedBytes = receivedBytes;
    g_rateTick = now;
}

void ProcessBorderlessRequest() {
    const BorderlessCommand command = SnapshotBorderlessCommand();
    if (!command.requested) {
        g_borderlessWasRequested = false;
        g_borderlessRequestTick = 0;
        g_borderlessRepairTick = 0;
        ResetBorderlessCandidate();
        RestoreWindow(&command);
        return;
    }
    if (command.rendererTransitionPending) {
        return;
    }

    const DWORD now = GetTickCount();
    if (!g_borderlessWasRequested) {
        g_borderlessWasRequested = true;
        g_borderlessRequestTick = now;
        ResetBorderlessCandidate();
    }
    const HWND hwnd = command.window;
    if (g_borderlessApplied) {
        if (!IsWindow(g_borderlessWindow)) {
            ClearBorderlessOwnership();
            g_borderlessRequestTick = now;
            ResetBorderlessCandidate();
            return;
        }
        if (hwnd != NULL && hwnd != g_borderlessWindow) {
            if (!RestoreWindow(&command)) {
                return;
            }
            g_borderlessRequestTick = now;
            ResetBorderlessCandidate();
            return;
        }
        if (hwnd == NULL) {
            return;
        }
        if (BorderlessConfigurationMatches(hwnd)) {
            g_borderlessRepairTick = 0;
            PublishBorderlessApplied(command);
            return;
        }

        const LONG_PTR currentStyle =
            GetWindowLongPtrA(hwnd, GWL_STYLE);
        if ((currentStyle & WS_CAPTION) == WS_CAPTION) {
            if (g_borderlessRestorePending) {
                if (RestoreWindow(&command)) {
                    g_borderlessRequestTick = now;
                    ResetBorderlessCandidate();
                }
                return;
            }
            ClearBorderlessOwnership();
            g_borderlessRequestTick = now;
            ResetBorderlessCandidate();
            return;
        }
        if (g_borderlessRepairTick == 0) {
            g_borderlessRepairTick = now;
            return;
        }
        if (now - g_borderlessRepairTick >= kBorderlessRequestGraceMs &&
            BorderlessCommandMatches(command)) {
            CaptureAndApplyBorderless(hwnd, command);
        }
        return;
    }
    if (hwnd != NULL && now - g_borderlessRequestTick >=
                            kBorderlessRequestGraceMs &&
        BorderlessCandidateIsStable(hwnd, now)) {
        CaptureAndApplyBorderless(hwnd, command);
    }
}

unsigned __stdcall BorderlessWorkerMain(void*) {
    unsigned int workerResult = 0u;
    while (!g_borderlessWorkerStop.load()) {
        const DWORD waitStatus =
            WaitForSingleObject(g_borderlessWakeEvent, 50u);
        if (waitStatus == WAIT_FAILED) {
            CLIENT_LOG_ERROR(
                "Borderless window controller wake wait failed: %lu",
                GetLastError());
            workerResult = 2u;
            break;
        }
        if (waitStatus != WAIT_OBJECT_0 && waitStatus != WAIT_TIMEOUT) {
            CLIENT_LOG_ERROR(
                "Borderless window controller returned unexpected wait status 0x%08lX",
                waitStatus);
            workerResult = 2u;
            break;
        }
        if (g_borderlessWorkerStop.load()) {
            break;
        }
        ProcessBorderlessRequest();
    }

    for (unsigned int attempt = 0; attempt < 100u; ++attempt) {
        if (RestoreWindow()) {
            return workerResult;
        }
        Sleep(10u);
    }
    return 1u;
}

bool StartBorderlessWorker() {
    g_borderlessWorkerStop.store(false);
    g_borderlessAppliedPublished.store(false);
    PublishBorderlessCommand(NULL, false, false);
    g_borderlessWasRequested = false;
    g_borderlessRequestTick = 0;
    g_borderlessRepairTick = 0;
    ResetBorderlessCandidate();

    g_borderlessWakeEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (g_borderlessWakeEvent == NULL) {
        CLIENT_LOG_ERROR("Could not create the borderless controller wake event");
        return false;
    }
    const uintptr_t worker = _beginthreadex(
        NULL, 0, &BorderlessWorkerMain, NULL, 0, NULL);
    g_borderlessWorker = reinterpret_cast<HANDLE>(worker);
    if (g_borderlessWorker == NULL) {
        CLIENT_LOG_ERROR("Could not start the borderless window controller");
        CloseHandle(g_borderlessWakeEvent);
        g_borderlessWakeEvent = NULL;
        return false;
    }
    CLIENT_LOG_INFO(
        "Started asynchronous borderless window controller");
    return true;
}

bool StopBorderlessWorker() {
    if (g_borderlessWorker == NULL) {
        return true;
    }
    PublishBorderlessCommand(NULL, false, false);
    g_borderlessWorkerStop.store(true);
    if (g_borderlessWakeEvent != NULL) {
        SetEvent(g_borderlessWakeEvent);
    }
    const DWORD waitStatus = WaitForSingleObject(g_borderlessWorker, 5000u);
    if (waitStatus != WAIT_OBJECT_0) {
        CLIENT_LOG_ERROR(
            "Borderless window controller did not stop; retaining client.dll safely");
        return false;
    }
    DWORD workerExitCode = 0;
    if (!GetExitCodeThread(g_borderlessWorker, &workerExitCode)) {
        CLIENT_LOG_ERROR(
            "Could not read borderless window controller exit status; retaining client.dll safely");
        return false;
    }
    if (workerExitCode == 1u) {
        CLIENT_LOG_ERROR(
            "Borderless window controller could not restore the window; retaining client.dll safely");
        return false;
    }
    if (workerExitCode != 0u) {
        CLIENT_LOG_WARNING(
            "Borderless window controller stopped after an internal wait failure");
    }
    CloseHandle(g_borderlessWorker);
    g_borderlessWorker = NULL;
    if (g_borderlessWakeEvent != NULL) {
        CloseHandle(g_borderlessWakeEvent);
        g_borderlessWakeEvent = NULL;
    }
    g_borderlessAppliedPublished.store(false);
    return true;
}

} // namespace

bool Install() {
    if (g_installed) {
        return true;
    }
    g_sendTarget = Resolve<void*>(Addresses::SteamSocketSendTo);
    g_recvTarget = Resolve<void*>(Addresses::SteamSocketRecvFrom);
    if (!IsExecutableAddress(g_sendTarget) ||
        !IsExecutableAddress(g_recvTarget)) {
        CLIENT_LOG_ERROR("Steam packet counter targets are not executable");
        g_sendTarget = NULL;
        g_recvTarget = NULL;
        return false;
    }
    if (g_networkCallbacks.load(std::memory_order_acquire) != 0) {
        CLIENT_LOG_ERROR(
            "Cannot install Steam packet hooks while an old callback is active");
        g_sendTarget = NULL;
        g_recvTarget = NULL;
        return false;
    }
    g_networkStopping.store(false, std::memory_order_release);

    MH_STATUS status = MH_CreateHook(
        g_sendTarget, reinterpret_cast<void*>(&SendToDetour),
        reinterpret_cast<void**>(&g_originalSendTo));
    if (status != MH_OK) {
        CLIENT_LOG_ERROR("Could not create Steam send hook: %s",
                         MH_StatusToString(status));
        g_sendTarget = NULL;
        g_recvTarget = NULL;
        g_originalSendTo = NULL;
        return false;
    }
    status = MH_CreateHook(
        g_recvTarget, reinterpret_cast<void*>(&RecvFromDetour),
        reinterpret_cast<void**>(&g_originalRecvFrom));
    if (status != MH_OK) {
        CLIENT_LOG_ERROR("Could not create Steam receive hook: %s",
                         MH_StatusToString(status));
        MH_RemoveHook(g_sendTarget);
        g_sendTarget = NULL;
        g_recvTarget = NULL;
        g_originalSendTo = NULL;
        return false;
    }

    if (!StartBorderlessWorker()) {
        RemoveNetworkHook(g_recvTarget, "receive");
        RemoveNetworkHook(g_sendTarget, "send");
        g_sendTarget = NULL;
        g_recvTarget = NULL;
        g_originalSendTo = NULL;
        g_originalRecvFrom = NULL;
        return false;
    }

    status = MH_EnableHook(g_sendTarget);
    if (status == MH_OK) {
        status = MH_EnableHook(g_recvTarget);
    }
    if (status != MH_OK) {
        CLIENT_LOG_ERROR("Could not enable Steam packet hooks: %s",
                         MH_StatusToString(status));
        g_networkStopping.store(true, std::memory_order_release);
        const bool receiveDisabled =
            DisableNetworkHook(g_recvTarget, "receive");
        const bool sendDisabled = DisableNetworkHook(g_sendTarget, "send");
        const bool quiesced = receiveDisabled && sendDisabled &&
            WaitForNetworkCallbacksToQuiesce();
        if (!quiesced) {
            g_installed = true;
            CLIENT_LOG_ERROR(
                "Packet-hook rollback is still active; retaining native subsystem for safe shutdown");
            return true;
        }
        const bool receiveRemoved =
            RemoveNetworkHook(g_recvTarget, "receive");
        const bool sendRemoved = RemoveNetworkHook(g_sendTarget, "send");
        if (!receiveRemoved || !sendRemoved || !StopBorderlessWorker()) {
            g_installed = true;
            CLIENT_LOG_ERROR(
                "Native rollback was incomplete; retaining subsystem for safe shutdown");
            return true;
        }
        g_sendTarget = NULL;
        g_recvTarget = NULL;
        g_originalSendTo = NULL;
        g_originalRecvFrom = NULL;
        return false;
    }

    g_installed = true;
    CLIENT_LOG_INFO(
        "Installed payload-blind Steam P2P send/receive packet counters");
    return true;
}

bool Remove() {
    if (!StopBorderlessWorker()) {
        return false;
    }
    g_networkStopping.store(true, std::memory_order_release);
    const bool receiveDisabled =
        DisableNetworkHook(g_recvTarget, "receive");
    const bool sendDisabled = DisableNetworkHook(g_sendTarget, "send");
    if (!receiveDisabled || !sendDisabled ||
        !WaitForNetworkCallbacksToQuiesce()) {
        return false;
    }
    const bool receiveRemoved = RemoveNetworkHook(g_recvTarget, "receive");
    const bool sendRemoved = RemoveNetworkHook(g_sendTarget, "send");
    if (!receiveRemoved || !sendRemoved) {
        return false;
    }
    g_sendTarget = NULL;
    g_recvTarget = NULL;
    g_originalSendTo = NULL;
    g_originalRecvFrom = NULL;
    g_installed = false;
    return true;
}

void Tick(bool borderlessRequested, bool rendererTransitionPending) {
    UpdateRates();
    PublishBorderlessCommand(
        GameWindow(), borderlessRequested, rendererTransitionPending);
    if (g_borderlessWakeEvent != NULL) {
        SetEvent(g_borderlessWakeEvent);
    }
}

void FormatNetworkDebugText(char* destination, std::size_t capacity) {
    if (destination == NULL || capacity == 0) {
        return;
    }
    const unsigned long long sentPackets =
        g_sentPackets.load(std::memory_order_relaxed);
    const unsigned long long sentBytes =
        g_sentBytes.load(std::memory_order_relaxed);
    const unsigned long long receivedPackets =
        g_receivedPackets.load(std::memory_order_relaxed);
    const unsigned long long receivedBytes =
        g_receivedBytes.load(std::memory_order_relaxed);
    _snprintf_s(
        destination, capacity, _TRUNCATE,
        "Steamworks | TX %llu pkt, %llu B (%llu pkt/s, %llu B/s) | "
        "RX %llu pkt, %llu B (%llu pkt/s, %llu B/s)",
        sentPackets, sentBytes, g_sentPacketsPerSecond,
        g_sentBytesPerSecond, receivedPackets, receivedBytes,
        g_receivedPacketsPerSecond, g_receivedBytesPerSecond);
}

bool IsInstalled() {
    return g_installed;
}

bool IsBorderlessApplied() {
    return g_borderlessAppliedPublished.load();
}

} // namespace NativeQol
} // namespace Client
