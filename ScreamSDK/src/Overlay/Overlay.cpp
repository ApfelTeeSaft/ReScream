#include "Internal.hpp"

#include "Diesel/Renderer.hpp"
#include "ScreamSDK/ImGui.hpp"
#include "ScreamSDK/Log.hpp"
#include "ScreamSDK/Memory.hpp"
#include "Workbench.hpp"

#include <Windows.h>
#include <windowsx.h>

#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>

// Pure VK/scancode mapping exported by the Dear ImGui Win32 backend. Unlike
// WndProcHandler, this helper does not touch an ImGui context or Win32 thread state.
extern ImGuiKey ImGui_ImplWin32_KeyEventToImGuiKey(WPARAM wParam, LPARAM lParam);

namespace Scream {
namespace Overlay {

namespace {

enum InputEventType {
    InputMouseSource,
    InputMousePosition,
    InputMouseButton,
    InputMouseWheel,
    InputKey,
    InputCharacter,
    InputFocus
};

struct InputEvent {
    InputEventType type;
    int code;
    int nativeCode;
    int scanCode;
    float x;
    float y;
    bool down;
    unsigned int character;
};

typedef HRESULT (WINAPI* ResetFunction)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

std::atomic<IDirect3DDevice9*> g_seenDevice(NULL);
std::atomic<bool> g_resetHookReady(false);
std::atomic<bool> g_visible(false);
std::atomic<bool> g_wantMouse(false);
std::atomic<bool> g_wantKeyboard(false);
std::atomic<bool> g_initialized(false);
std::atomic<bool> g_shutdownComplete(true);
std::atomic<int> g_toggleKey(VK_INSERT);
std::atomic<int> g_desiredCursor(ImGuiMouseCursor_Arrow);
std::atomic<bool> g_restorePosted(false);
std::atomic<bool> g_refreshDpi(true);
std::atomic<float> g_dpiScale(1.0f);
std::atomic<ImFont*> g_interfaceFont(NULL);
std::atomic<ImFont*> g_headingFont(NULL);
std::atomic<ImFont*> g_codeFont(NULL);
std::mutex g_messageMutex;
std::recursive_mutex g_renderMutex;
std::deque<InputEvent> g_inputEvents;
IDirect3DDevice9* g_boundDevice = NULL;
HWND g_window = NULL;
std::atomic<WNDPROC> g_originalWindowProcedure(NULL);
ResetFunction g_originalReset = NULL;
bool g_sceneFailureLogged = false;
bool g_windowRestoreFailureLogged = false;
unsigned int g_mouseButtonsDown = 0;
int g_mouseTrackedArea = 0;
UINT g_keyboardCodePage = CP_ACP;
LARGE_INTEGER g_ticksPerSecond = {};
LARGE_INTEGER g_lastTick = {};
float g_overlayAlpha = 0.0f;
const UINT kRestoreWindowProcedureMessage = WM_APP + 0x53C;

const float kInterfaceFontSize = 16.0f;
const float kHeadingFontSize = 19.0f;
const float kCodeFontSize = 15.0f;

ImVec4 Color(unsigned int red, unsigned int green, unsigned int blue,
             unsigned int alpha = 255) {
    const float inverse = 1.0f / 255.0f;
    return ImVec4(red * inverse, green * inverse, blue * inverse, alpha * inverse);
}

void ApplyPrototypeTheme(float dpiScale) {
    // Start from a fresh style on every DPI change. ScaleAllSizes rounds values,
    // so incrementally scaling the live style would accumulate visible drift.
    ImGuiStyle style;
    style.FontSizeBase = kInterfaceFontSize;
    style.FontScaleDpi = dpiScale;
    style.DisabledAlpha = 0.62f;
    style.WindowPadding = ImVec2(14.0f, 11.0f);
    style.WindowRounding = 12.0f;
    style.WindowBorderSize = 1.0f;
    style.WindowBorderHoverPadding = 5.0f;
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.ChildRounding = 10.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupRounding = 10.0f;
    style.PopupBorderSize = 1.0f;
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.FrameRounding = 7.0f;
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.CellPadding = ImVec2(8.0f, 5.0f);
    style.IndentSpacing = 22.0f;
    style.ScrollbarSize = 13.0f;
    style.ScrollbarRounding = 7.0f;
    style.ScrollbarPadding = 2.0f;
    style.GrabMinSize = 11.0f;
    style.GrabRounding = 7.0f;
    style.TabRounding = 7.0f;
    style.TabBorderSize = 0.0f;
    style.TabBarBorderSize = 1.0f;
    style.TabBarOverlineSize = 2.0f;
    style.InputTextCursorSize = 1.5f;
    style.SeparatorTextBorderSize = 1.0f;
    style.SeparatorTextPadding = ImVec2(12.0f, 5.0f);
    style.AntiAliasedLines = true;
    style.AntiAliasedLinesUseTex = true;
    style.AntiAliasedFill = true;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = Color(245, 246, 248, 250);
    colors[ImGuiCol_TextDisabled] = Color(154, 160, 172, 230);
    colors[ImGuiCol_WindowBg] = Color(9, 10, 14, 224);
    colors[ImGuiCol_ChildBg] = Color(18, 20, 27, 72);
    colors[ImGuiCol_PopupBg] = Color(14, 15, 20, 246);
    colors[ImGuiCol_Border] = Color(255, 255, 255, 26);
    colors[ImGuiCol_BorderShadow] = Color(0, 0, 0, 96);
    colors[ImGuiCol_FrameBg] = Color(31, 33, 41, 158);
    colors[ImGuiCol_FrameBgHovered] = Color(225, 20, 39, 45);
    colors[ImGuiCol_FrameBgActive] = Color(225, 20, 39, 70);
    colors[ImGuiCol_TitleBg] = Color(9, 10, 14, 190);
    colors[ImGuiCol_TitleBgActive] = Color(16, 17, 22, 220);
    colors[ImGuiCol_TitleBgCollapsed] = Color(9, 10, 14, 170);
    colors[ImGuiCol_MenuBarBg] = Color(255, 255, 255, 6);
    colors[ImGuiCol_ScrollbarBg] = Color(0, 0, 0, 44);
    colors[ImGuiCol_ScrollbarGrab] = Color(255, 255, 255, 40);
    colors[ImGuiCol_ScrollbarGrabHovered] = Color(255, 255, 255, 62);
    colors[ImGuiCol_ScrollbarGrabActive] = Color(225, 20, 39, 135);
    colors[ImGuiCol_CheckMark] = Color(255, 49, 69);
    colors[ImGuiCol_CheckboxSelectedBg] = Color(225, 20, 39, 90);
    colors[ImGuiCol_SliderGrab] = Color(225, 20, 39);
    colors[ImGuiCol_SliderGrabActive] = Color(255, 67, 84);
    colors[ImGuiCol_Button] = Color(255, 255, 255, 15);
    colors[ImGuiCol_ButtonHovered] = Color(225, 20, 39, 54);
    colors[ImGuiCol_ButtonActive] = Color(225, 20, 39, 84);
    colors[ImGuiCol_Header] = Color(255, 255, 255, 11);
    colors[ImGuiCol_HeaderHovered] = Color(225, 20, 39, 45);
    colors[ImGuiCol_HeaderActive] = Color(225, 20, 39, 70);
    colors[ImGuiCol_Separator] = Color(255, 255, 255, 22);
    colors[ImGuiCol_SeparatorHovered] = Color(246, 42, 61, 160);
    colors[ImGuiCol_SeparatorActive] = Color(255, 49, 69);
    colors[ImGuiCol_ResizeGrip] = Color(225, 20, 39, 28);
    colors[ImGuiCol_ResizeGripHovered] = Color(246, 42, 61, 105);
    colors[ImGuiCol_ResizeGripActive] = Color(255, 67, 84, 190);
    colors[ImGuiCol_InputTextCursor] = Color(255, 67, 84);
    colors[ImGuiCol_Tab] = Color(255, 255, 255, 7);
    colors[ImGuiCol_TabHovered] = Color(225, 20, 39, 42);
    colors[ImGuiCol_TabSelected] = Color(225, 20, 39, 24);
    colors[ImGuiCol_TabSelectedOverline] = Color(255, 49, 69);
    colors[ImGuiCol_TabDimmed] = Color(0, 0, 0, 12);
    colors[ImGuiCol_TabDimmedSelected] = Color(225, 20, 39, 15);
    colors[ImGuiCol_TabDimmedSelectedOverline] = Color(159, 34, 47);
    colors[ImGuiCol_TableHeaderBg] = Color(255, 255, 255, 9);
    colors[ImGuiCol_TableBorderStrong] = Color(255, 255, 255, 24);
    colors[ImGuiCol_TableBorderLight] = Color(255, 255, 255, 14);
    colors[ImGuiCol_TableRowBg] = Color(0, 0, 0, 0);
    colors[ImGuiCol_TableRowBgAlt] = Color(255, 255, 255, 5);
    colors[ImGuiCol_TextLink] = Color(255, 73, 89);
    colors[ImGuiCol_TextSelectedBg] = Color(225, 20, 39, 72);
    colors[ImGuiCol_TreeLines] = Color(255, 255, 255, 35);
    colors[ImGuiCol_DragDropTarget] = Color(255, 185, 0);
    colors[ImGuiCol_DragDropTargetBg] = Color(255, 185, 0, 32);
    colors[ImGuiCol_UnsavedMarker] = Color(255, 185, 0);
    colors[ImGuiCol_NavCursor] = Color(255, 49, 69);
    colors[ImGuiCol_NavWindowingHighlight] = Color(245, 246, 248, 180);
    colors[ImGuiCol_NavWindowingDimBg] = Color(0, 0, 0, 90);
    colors[ImGuiCol_ModalWindowDimBg] = Color(0, 0, 0, 145);

    style.ScaleAllSizes(dpiScale);
    ImGui::GetStyle() = style;
}

std::string WindowsFontPath(const char* fileName) {
    char windowsDirectory[MAX_PATH + 1] = {};
    const UINT length = GetWindowsDirectoryA(windowsDirectory, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return std::string();
    }
    std::string path(windowsDirectory, length);
    path += "\\Fonts\\";
    path += fileName;
    const DWORD attributes = GetFileAttributesA(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return std::string();
    }
    return path;
}

ImFont* LoadWindowsFont(const char* const* candidates, std::size_t count,
                        float size, float rasterizerMultiply,
                        const char*& loadedName) {
    loadedName = NULL;
    for (std::size_t i = 0; i < count; ++i) {
        const std::string path = WindowsFontPath(candidates[i]);
        if (path.empty()) {
            continue;
        }
        ImFontConfig configuration;
        configuration.Flags |= ImFontFlags_NoLoadError;
        configuration.OversampleH = 0;
        configuration.OversampleV = 0;
        configuration.PixelSnapH = false;
        configuration.RasterizerMultiply = rasterizerMultiply;
        std::strncpy(configuration.Name, candidates[i], sizeof(configuration.Name) - 1);
        configuration.Name[sizeof(configuration.Name) - 1] = 0;
        ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF(
            path.c_str(), size, &configuration);
        if (font != NULL) {
            loadedName = candidates[i];
            return font;
        }
    }
    return NULL;
}

ImFont* AddVectorFallback(float size, const char* name) {
    ImFontConfig configuration;
    configuration.SizePixels = size;
    std::strncpy(configuration.Name, name, sizeof(configuration.Name) - 1);
    configuration.Name[sizeof(configuration.Name) - 1] = 0;
    return ImGui::GetIO().Fonts->AddFontDefaultVector(&configuration);
}

void InitializeFonts() {
    static const char* const interfaceCandidates[] = {
        "segoeui.ttf", "segoeuisl.ttf"
    };
    static const char* const headingCandidates[] = {
        "seguisb.ttf", "segoeuib.ttf"
    };
    static const char* const codeCandidates[] = {
        "CascadiaMono.ttf", "consola.ttf", "cour.ttf"
    };

    const char* interfaceName = NULL;
    const char* headingName = NULL;
    const char* codeName = NULL;
    ImFont* interfaceFont = LoadWindowsFont(
        interfaceCandidates, sizeof(interfaceCandidates) / sizeof(interfaceCandidates[0]),
        kInterfaceFontSize, 1.04f, interfaceName);
    if (interfaceFont == NULL) {
        interfaceFont = AddVectorFallback(kInterfaceFontSize, "Embedded UI fallback");
        interfaceName = "embedded ProggyForever";
    }

    ImFont* headingFont = LoadWindowsFont(
        headingCandidates, sizeof(headingCandidates) / sizeof(headingCandidates[0]),
        kHeadingFontSize, 1.02f, headingName);
    if (headingFont == NULL) {
        headingFont = interfaceFont;
        headingName = interfaceName;
    }

    ImFont* codeFont = LoadWindowsFont(
        codeCandidates, sizeof(codeCandidates) / sizeof(codeCandidates[0]),
        kCodeFontSize, 1.03f, codeName);
    if (codeFont == NULL) {
        codeFont = AddVectorFallback(kCodeFontSize, "Embedded code fallback");
        codeName = "embedded ProggyForever";
    }
    if (codeFont == NULL) {
        codeFont = interfaceFont;
        codeName = interfaceName;
    }

    ImGui::GetIO().FontDefault = interfaceFont;
    g_interfaceFont.store(interfaceFont);
    g_headingFont.store(headingFont);
    g_codeFont.store(codeFont);
    SCREAM_LOG_INFO("ImGui fonts: interface=%s heading=%s code=%s",
                    interfaceName != NULL ? interfaceName : "unavailable",
                    headingName != NULL ? headingName : "unavailable",
                    codeName != NULL ? codeName : "unavailable");
}

void ClearFontHandles() {
    g_interfaceFont.store(NULL);
    g_headingFont.store(NULL);
    g_codeFont.store(NULL);
}

float QueryDpiScale(HWND window) {
    float scale = window != NULL
        ? ImGui_ImplWin32_GetDpiScaleForHwnd(window) : 1.0f;
    if (!std::isfinite(scale) || scale <= 0.0f) {
        scale = 1.0f;
    }
    return std::max(1.0f, std::min(scale, 2.5f));
}

bool IsMouseMessage(UINT message) {
    return (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST) ||
           message == WM_NCMOUSEMOVE || message == WM_NCLBUTTONDOWN ||
           message == WM_NCLBUTTONUP || message == WM_NCRBUTTONDOWN ||
           message == WM_NCRBUTTONUP;
}

bool IsKeyboardMessage(UINT message) {
    return (message >= WM_KEYFIRST && message <= WM_KEYLAST) ||
           message == WM_CHAR || message == WM_SYSCHAR || message == WM_UNICHAR;
}

void ClearInputEvents() {
    std::lock_guard<std::mutex> lock(g_messageMutex);
    g_inputEvents.clear();
}

void QueueInput(InputEventType type, int code = 0, bool down = false,
                float x = 0.0f, float y = 0.0f,
                unsigned int character = 0,
                int nativeCode = 0, int scanCode = 0) {
    InputEvent event = {};
    event.type = type;
    event.code = code;
    event.nativeCode = nativeCode;
    event.scanCode = scanCode;
    event.x = x;
    event.y = y;
    event.down = down;
    event.character = character;
    std::lock_guard<std::mutex> lock(g_messageMutex);
    if (g_inputEvents.size() < 4096) {
        try {
            g_inputEvents.push_back(event);
        } catch (...) {
            // A window procedure must never propagate a C++ exception.
        }
    }
}

bool IsVirtualKeyDown(int virtualKey) {
    return (::GetKeyState(virtualKey) & 0x8000) != 0;
}

void QueueModifiers() {
    QueueInput(InputKey, ImGuiMod_Ctrl, IsVirtualKeyDown(VK_CONTROL));
    QueueInput(InputKey, ImGuiMod_Shift, IsVirtualKeyDown(VK_SHIFT));
    QueueInput(InputKey, ImGuiMod_Alt, IsVirtualKeyDown(VK_MENU));
    QueueInput(InputKey, ImGuiMod_Super,
               IsVirtualKeyDown(VK_LWIN) || IsVirtualKeyDown(VK_RWIN));
}

int MouseSourceFromMessage() {
    const LPARAM extra = GetMessageExtraInfo();
    if ((extra & 0xFFFFFF80) == 0xFF515700) {
        return ImGuiMouseSource_Pen;
    }
    if ((extra & 0xFFFFFF80) == 0xFF515780) {
        return ImGuiMouseSource_TouchScreen;
    }
    return ImGuiMouseSource_Mouse;
}

void UpdateKeyboardCodePage(HWND window) {
    const DWORD thread = GetWindowThreadProcessId(window, NULL);
    const HKL layout = GetKeyboardLayout(thread);
    const LCID locale = MAKELCID(HIWORD(layout), SORT_DEFAULT);
    UINT codePage = CP_ACP;
    if (GetLocaleInfoA(locale, LOCALE_RETURN_NUMBER | LOCALE_IDEFAULTANSICODEPAGE,
                       reinterpret_cast<LPSTR>(&codePage), sizeof(codePage)) == 0) {
        codePage = CP_ACP;
    }
    g_keyboardCodePage = codePage;
}

HCURSOR CursorForImGui(ImGuiMouseCursor cursor) {
    switch (cursor) {
    case ImGuiMouseCursor_Arrow: return LoadCursor(NULL, IDC_ARROW);
    case ImGuiMouseCursor_TextInput: return LoadCursor(NULL, IDC_IBEAM);
    case ImGuiMouseCursor_ResizeAll: return LoadCursor(NULL, IDC_SIZEALL);
    case ImGuiMouseCursor_ResizeEW: return LoadCursor(NULL, IDC_SIZEWE);
    case ImGuiMouseCursor_ResizeNS: return LoadCursor(NULL, IDC_SIZENS);
    case ImGuiMouseCursor_ResizeNESW: return LoadCursor(NULL, IDC_SIZENESW);
    case ImGuiMouseCursor_ResizeNWSE: return LoadCursor(NULL, IDC_SIZENWSE);
    case ImGuiMouseCursor_Hand: return LoadCursor(NULL, IDC_HAND);
    case ImGuiMouseCursor_NotAllowed: return LoadCursor(NULL, IDC_NO);
    default: return LoadCursor(NULL, IDC_ARROW);
    }
}

LRESULT CALLBACK WindowProcedure(HWND window, UINT message,
                                 WPARAM wParam, LPARAM lParam) {
    if (message == kRestoreWindowProcedureMessage) {
        WNDPROC original = g_originalWindowProcedure.load();
        const WNDPROC current = reinterpret_cast<WNDPROC>(
            GetWindowLongPtrA(window, GWLP_WNDPROC));
        if (current != &WindowProcedure) {
            g_originalWindowProcedure.store(NULL);
            g_restorePosted.store(false);
            return TRUE;
        }
        if (original == NULL) {
            return FALSE;
        }
        SetLastError(0);
        const LONG_PTR previous = SetWindowLongPtrA(
            window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
        if (previous == 0 && GetLastError() != 0) {
            return FALSE;
        }
        g_originalWindowProcedure.store(NULL);
        g_restorePosted.store(false);
        return TRUE;
    }
    if (Runtime::IsShuttingDown()) {
        WNDPROC original = g_originalWindowProcedure.load();
        return original != NULL ? CallWindowProcA(original, window, message, wParam, lParam)
                                : DefWindowProcA(window, message, wParam, lParam);
    }
    // The window thread may notify us while the renderer owns the ImGui
    // context. Defer all querying and style mutation to the next Present.
    if (message == WM_DPICHANGED || message == WM_DISPLAYCHANGE ||
        message == WM_WINDOWPOSCHANGED) {
        g_refreshDpi.store(true);
    }
    if (message == WM_KEYUP && static_cast<int>(wParam) == g_toggleKey.load()) {
        const bool nowVisible = !g_visible.load();
        g_visible.store(nowVisible);
        if (!nowVisible) {
            g_wantMouse.store(false);
            g_wantKeyboard.store(false);
            ClearInputEvents();
            QueueInput(InputFocus, 0, false);
            g_mouseButtonsDown = 0;
            if (GetCapture() == window) {
                ReleaseCapture();
            }
        } else {
            QueueInput(InputFocus, 0, GetFocus() == window);
        }
        return 0;
    }

    if (g_initialized.load() && g_visible.load()) {
        switch (message) {
        case WM_MOUSEMOVE:
        case WM_NCMOUSEMOVE: {
            const int area = message == WM_MOUSEMOVE ? 1 : 2;
            if (g_mouseTrackedArea != area) {
                TRACKMOUSEEVENT cancel = { sizeof(cancel), TME_CANCEL, window, 0 };
                TRACKMOUSEEVENT track = {
                    sizeof(track),
                    static_cast<DWORD>(area == 2 ? TME_LEAVE | TME_NONCLIENT : TME_LEAVE),
                    window, 0
                };
                if (g_mouseTrackedArea != 0) {
                    TrackMouseEvent(&cancel);
                }
                TrackMouseEvent(&track);
                g_mouseTrackedArea = area;
            }
            POINT position = {
                static_cast<LONG>(GET_X_LPARAM(lParam)),
                static_cast<LONG>(GET_Y_LPARAM(lParam))
            };
            if (message == WM_NCMOUSEMOVE) {
                ScreenToClient(window, &position);
            }
            QueueInput(InputMouseSource, MouseSourceFromMessage());
            QueueInput(InputMousePosition, 0, false,
                       static_cast<float>(position.x), static_cast<float>(position.y));
            break;
        }
        case WM_MOUSELEAVE:
        case WM_NCMOUSELEAVE: {
            const int area = message == WM_MOUSELEAVE ? 1 : 2;
            if (g_mouseTrackedArea == area) {
                g_mouseTrackedArea = 0;
                QueueInput(InputMousePosition, 0, false, -FLT_MAX, -FLT_MAX);
            }
            break;
        }
        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK: {
            int button = 0;
            if (message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK) button = 1;
            if (message == WM_MBUTTONDOWN || message == WM_MBUTTONDBLCLK) button = 2;
            if (message == WM_XBUTTONDOWN || message == WM_XBUTTONDBLCLK) {
                button = GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? 3 : 4;
            }
            if (g_mouseButtonsDown == 0 && GetCapture() == NULL) {
                SetCapture(window);
            }
            g_mouseButtonsDown |= 1u << button;
            QueueInput(InputMouseSource, MouseSourceFromMessage());
            QueueInput(InputMouseButton, button, true);
            break;
        }
        case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP: case WM_XBUTTONUP: {
            int button = 0;
            if (message == WM_RBUTTONUP) button = 1;
            if (message == WM_MBUTTONUP) button = 2;
            if (message == WM_XBUTTONUP) {
                button = GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? 3 : 4;
            }
            g_mouseButtonsDown &= ~(1u << button);
            if (g_mouseButtonsDown == 0 && GetCapture() == window) {
                ReleaseCapture();
            }
            QueueInput(InputMouseSource, MouseSourceFromMessage());
            QueueInput(InputMouseButton, button, false);
            break;
        }
        case WM_MOUSEWHEEL:
            QueueInput(InputMouseWheel, 0, false, 0.0f,
                       static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA);
            break;
        case WM_MOUSEHWHEEL:
            QueueInput(InputMouseWheel, 0, false,
                       -static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA,
                       0.0f);
            break;
        case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP: {
            const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
            if (wParam < 256) {
                QueueModifiers();
                const ImGuiKey key = ImGui_ImplWin32_KeyEventToImGuiKey(wParam, lParam);
                const int virtualKey = static_cast<int>(wParam);
                const int scanCode = static_cast<int>(LOBYTE(HIWORD(lParam)));
                if (key == ImGuiKey_PrintScreen && !down) {
                    QueueInput(InputKey, key, true, 0.0f, 0.0f, 0,
                               virtualKey, scanCode);
                }
                if (key != ImGuiKey_None) {
                    QueueInput(InputKey, key, down, 0.0f, 0.0f, 0,
                               virtualKey, scanCode);
                }
                if (virtualKey == VK_SHIFT) {
                    if (IsVirtualKeyDown(VK_LSHIFT) == down)
                        QueueInput(InputKey, ImGuiKey_LeftShift, down, 0, 0, 0,
                                   VK_LSHIFT, scanCode);
                    if (IsVirtualKeyDown(VK_RSHIFT) == down)
                        QueueInput(InputKey, ImGuiKey_RightShift, down, 0, 0, 0,
                                   VK_RSHIFT, scanCode);
                } else if (virtualKey == VK_CONTROL) {
                    if (IsVirtualKeyDown(VK_LCONTROL) == down)
                        QueueInput(InputKey, ImGuiKey_LeftCtrl, down, 0, 0, 0,
                                   VK_LCONTROL, scanCode);
                    if (IsVirtualKeyDown(VK_RCONTROL) == down)
                        QueueInput(InputKey, ImGuiKey_RightCtrl, down, 0, 0, 0,
                                   VK_RCONTROL, scanCode);
                } else if (virtualKey == VK_MENU) {
                    if (IsVirtualKeyDown(VK_LMENU) == down)
                        QueueInput(InputKey, ImGuiKey_LeftAlt, down, 0, 0, 0,
                                   VK_LMENU, scanCode);
                    if (IsVirtualKeyDown(VK_RMENU) == down)
                        QueueInput(InputKey, ImGuiKey_RightAlt, down, 0, 0, 0,
                                   VK_RMENU, scanCode);
                }
            }
            break;
        }
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            QueueInput(InputFocus, 0, message == WM_SETFOCUS);
            break;
        case WM_INPUTLANGCHANGE:
            UpdateKeyboardCodePage(window);
            break;
        case WM_CHAR:
            if (wParam > 0 && wParam < 0x10000) {
                if (IsWindowUnicode(window)) {
                    QueueInput(InputCharacter, 0, false, 0, 0,
                               static_cast<unsigned int>(wParam));
                } else {
                    wchar_t character = 0;
                    WPARAM bytes = wParam;
                    if (MultiByteToWideChar(g_keyboardCodePage, MB_PRECOMPOSED,
                                            reinterpret_cast<const char*>(&bytes), 2,
                                            &character, 1) != 0) {
                        QueueInput(InputCharacter, 0, false, 0, 0,
                                   static_cast<unsigned int>(character));
                    }
                }
            }
            break;
        case WM_DESTROY:
            g_mouseTrackedArea = 0;
            g_mouseButtonsDown = 0;
            QueueInput(InputMousePosition, 0, false, -FLT_MAX, -FLT_MAX);
            QueueInput(InputFocus, 0, false);
            break;
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                const int cursor = g_desiredCursor.load();
                SetCursor(cursor == ImGuiMouseCursor_None
                              ? NULL
                              : CursorForImGui(static_cast<ImGuiMouseCursor>(cursor)));
                return TRUE;
            }
            break;
        default:
            break;
        }
        if ((IsMouseMessage(message) && g_wantMouse.load()) ||
            (IsKeyboardMessage(message) && g_wantKeyboard.load())) {
            return 0;
        }
    }

    WNDPROC original = g_originalWindowProcedure.load();
    return original != NULL ? CallWindowProcA(original, window, message, wParam, lParam)
                            : DefWindowProcA(window, message, wParam, lParam);
}

bool RestoreWindowProcedure() {
    WNDPROC original = g_originalWindowProcedure.load();
    HWND window = g_window;
    if (window == NULL || original == NULL || !IsWindow(window)) {
        g_originalWindowProcedure.store(NULL);
        g_window = NULL;
        return true;
    }
    const WNDPROC current = reinterpret_cast<WNDPROC>(
        GetWindowLongPtrA(window, GWLP_WNDPROC));
    if (current != &WindowProcedure) {
        g_originalWindowProcedure.store(NULL);
        g_window = NULL;
        return true;
    }

    DWORD windowThread = GetWindowThreadProcessId(window, NULL);
    bool restored = false;
    if (windowThread == GetCurrentThreadId()) {
        restored = WindowProcedure(window, kRestoreWindowProcedureMessage, 0, 0) != FALSE;
    } else {
        // A threaded renderer can run while the window thread is synchronously
        // waiting for Present. Posting avoids a cross-thread SendMessage deadlock;
        // a later Present observes the completed restoration and tears ImGui down.
        if (!g_restorePosted.exchange(true) &&
            !PostMessageA(window, kRestoreWindowProcedureMessage, 0, 0)) {
            g_restorePosted.store(false);
        }
        return false;
    }
    if (restored && g_originalWindowProcedure.load() == NULL) {
        g_window = NULL;
        return true;
    }
    return false;
}

bool ShutdownOnRenderThread() {
    if (!g_initialized.load()) {
        g_shutdownComplete.store(true);
        return true;
    }
    g_visible.store(false);
    g_wantMouse.store(false);
    g_wantKeyboard.store(false);
    if (!RestoreWindowProcedure()) {
        g_shutdownComplete.store(false);
        if (!g_windowRestoreFailureLogged) {
            SCREAM_LOG_INFO("Window procedure restoration queued on its owning thread");
            g_windowRestoreFailureLogged = true;
        }
        return false;
    }
    g_initialized.store(false);
    ClearInputEvents();
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ClearFontHandles();
    ImGui::DestroyContext();
    g_overlayAlpha = 0.0f;
    g_dpiScale.store(1.0f);
    g_refreshDpi.store(true);
    if (g_boundDevice != NULL) {
        g_boundDevice->Release();
        g_boundDevice = NULL;
    }
    g_shutdownComplete.store(true);
    SCREAM_LOG_INFO("ImGui overlay shut down on the render thread");
    return true;
}

bool InstallWindowProcedure(HWND window) {
    SetLastError(0);
    const LONG_PTR previous = SetWindowLongPtrA(
        window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowProcedure));
    if (previous == 0 && GetLastError() != 0) {
        return false;
    }
    g_originalWindowProcedure.store(reinterpret_cast<WNDPROC>(previous));
    g_window = window;
    return g_originalWindowProcedure.load() != NULL;
}

bool InitializeOnRenderThread(IDirect3DDevice9* device) {
    HWND window = Diesel::Renderer::GetWindow();
    if (device == NULL || window == NULL || !IsWindow(window)) {
        return false;
    }

    g_shutdownComplete.store(false);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigInputTextCursorBlink = true;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.IniFilename = "ScreamSDK.imgui.ini";
    const float dpiScale = QueryDpiScale(window);
    g_dpiScale.store(dpiScale);
    ApplyPrototypeTheme(dpiScale);
    InitializeFonts();
    g_overlayAlpha = 0.0f;
    g_refreshDpi.store(false);

    if (!ImGui_ImplWin32_Init(window)) {
        ClearFontHandles();
        ImGui::DestroyContext();
        SCREAM_LOG_ERROR("ImGui Win32 backend initialization failed");
        g_shutdownComplete.store(true);
        return false;
    }
    // Mouse positioning and all input are supplied by the window-thread event
    // queue below. The render thread never polls capture or keyboard state.
    io.BackendFlags &= ~ImGuiBackendFlags_HasSetMousePos;
    if (!ImGui_ImplDX9_Init(device)) {
        ImGui_ImplWin32_Shutdown();
        ClearFontHandles();
        ImGui::DestroyContext();
        SCREAM_LOG_ERROR("ImGui DX9 backend initialization failed");
        g_shutdownComplete.store(true);
        return false;
    }
    UpdateKeyboardCodePage(window);
    g_mouseButtonsDown = 0;
    g_mouseTrackedArea = 0;
    if (!InstallWindowProcedure(window)) {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ClearFontHandles();
        ImGui::DestroyContext();
        SCREAM_LOG_ERROR("Could not subclass the game window procedure");
        g_shutdownComplete.store(true);
        return false;
    }

    device->AddRef();
    g_boundDevice = device;
    QueryPerformanceFrequency(&g_ticksPerSecond);
    QueryPerformanceCounter(&g_lastTick);
    g_desiredCursor.store(ImGuiMouseCursor_Arrow);
    g_restorePosted.store(false);
    g_windowRestoreFailureLogged = false;
    g_shutdownComplete.store(false);
    g_initialized.store(true);
    SCREAM_LOG_INFO("ImGui initialized: hwnd=%p d3d9_device=%p (INSERT toggles)",
                    window, device);
    return true;
}

void DrainInputEvents() {
    std::deque<InputEvent> events;
    {
        std::lock_guard<std::mutex> lock(g_messageMutex);
        events.swap(g_inputEvents);
    }
    ImGuiIO& io = ImGui::GetIO();
    for (std::size_t i = 0; i < events.size(); ++i) {
        const InputEvent& event = events[i];
        switch (event.type) {
        case InputMouseSource:
            io.AddMouseSourceEvent(static_cast<ImGuiMouseSource>(event.code));
            break;
        case InputMousePosition:
            io.AddMousePosEvent(event.x, event.y);
            break;
        case InputMouseButton:
            io.AddMouseButtonEvent(event.code, event.down);
            break;
        case InputMouseWheel:
            io.AddMouseWheelEvent(event.x, event.y);
            break;
        case InputKey: {
            const ImGuiKey key = static_cast<ImGuiKey>(event.code);
            io.AddKeyEvent(key, event.down);
            if (event.nativeCode != 0) {
                io.SetKeyEventNativeData(key, event.nativeCode, event.scanCode);
            }
            break;
        }
        case InputCharacter:
            io.AddInputCharacterUTF16(static_cast<ImWchar16>(event.character));
            break;
        case InputFocus:
            io.AddFocusEvent(event.down);
            break;
        }
    }
}

void PlatformNewFrame() {
    ImGuiIO& io = ImGui::GetIO();
    RECT rectangle = {};
    if (g_window != NULL && GetClientRect(g_window, &rectangle)) {
        io.DisplaySize = ImVec2(static_cast<float>(rectangle.right - rectangle.left),
                              static_cast<float>(rectangle.bottom - rectangle.top));
    }
    if (g_refreshDpi.exchange(false)) {
        const float previousScale = g_dpiScale.load();
        const float currentScale = QueryDpiScale(g_window);
        if (std::fabs(currentScale - previousScale) > 0.01f) {
            g_dpiScale.store(currentScale);
            ApplyPrototypeTheme(currentScale);
            SCREAM_LOG_INFO("ImGui content scale changed from %.2f to %.2f",
                            previousScale, currentScale);
        }
    }
    LARGE_INTEGER now = {};
    QueryPerformanceCounter(&now);
    if (g_ticksPerSecond.QuadPart > 0 && now.QuadPart > g_lastTick.QuadPart) {
        io.DeltaTime = static_cast<float>(
            static_cast<double>(now.QuadPart - g_lastTick.QuadPart) /
            static_cast<double>(g_ticksPerSecond.QuadPart));
    } else {
        io.DeltaTime = 1.0f / 60.0f;
    }
    g_lastTick = now;
}

void DrawPrototypeWindowChrome(float scale) {
    const ImVec2 windowMin = ImGui::GetWindowPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();
    const ImVec2 windowMax(windowMin.x + windowSize.x,
                           windowMin.y + windowSize.y);
    const float rounding = ImGui::GetStyle().WindowRounding;
    ImDrawList* background = ImGui::GetBackgroundDrawList();
    background->AddRectFilled(
        ImVec2(windowMin.x - 7.0f * scale, windowMin.y - 4.0f * scale),
        ImVec2(windowMax.x + 9.0f * scale, windowMax.y + 12.0f * scale),
        ImGui::GetColorU32(Color(0, 0, 0, 18)), rounding + 8.0f * scale);
    background->AddRectFilled(
        ImVec2(windowMin.x - 3.0f * scale, windowMin.y - 2.0f * scale),
        ImVec2(windowMax.x + 5.0f * scale, windowMax.y + 7.0f * scale),
        ImGui::GetColorU32(Color(0, 0, 0, 32)), rounding + 4.0f * scale);
    background->AddRectFilled(
        ImVec2(windowMin.x + 1.0f * scale, windowMin.y + 3.0f * scale),
        ImVec2(windowMax.x + 3.0f * scale, windowMax.y + 6.0f * scale),
        ImGui::GetColorU32(Color(0, 0, 0, 55)), rounding + 2.0f * scale);

    ImDrawList* windowDrawList = ImGui::GetWindowDrawList();
    windowDrawList->AddLine(
        ImVec2(windowMin.x + rounding, windowMin.y + 1.0f * scale),
        ImVec2(windowMax.x - rounding, windowMin.y + 1.0f * scale),
        ImGui::GetColorU32(Color(255, 49, 69, 155)),
        std::max(1.0f, 1.25f * scale));
}

void DrawRootWindow() {
    const float scale = g_dpiScale.load();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float maximumWidth = display.x > 0.0f
        ? std::max(320.0f, display.x - 24.0f * scale) : FLT_MAX;
    const float maximumHeight = display.y > 0.0f
        ? std::max(280.0f, display.y - 24.0f * scale) : FLT_MAX;
    const ImVec2 minimumSize(std::min(700.0f * scale, maximumWidth),
                             std::min(460.0f * scale, maximumHeight));
    const ImVec2 maximumSize(maximumWidth, maximumHeight);
    const ImVec2 initialSize(std::min(1050.0f * scale, maximumWidth),
                             std::min(720.0f * scale, maximumHeight));
    ImGui::SetNextWindowSizeConstraints(minimumSize, maximumSize);
    ImGui::SetNextWindowSize(initialSize, ImGuiCond_FirstUseEver);
    const bool drawContents = ImGui::Begin(
        "ScreamSDK — Diesel Modding Tools", NULL, ImGuiWindowFlags_NoCollapse);
    DrawPrototypeWindowChrome(scale);
    if (drawContents) {
        ImFont* headingFont = g_headingFont.load();
        if (headingFont != NULL) {
            ImGui::PushFont(headingFont, kHeadingFontSize);
        }
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark),
                           "Diesel Modding Workbench");
        if (headingFont != NULL) {
            ImGui::PopFont();
        }
        ImGui::TextDisabled("Live runtime tools  |  INSERT hides the overlay");
        ImGui::Separator();
        ImGui::Spacing();
        if (ImGui::BeginTabBar("sdk_tabs")) {
            PluginsInternal::DrawOverlayPanels();
            if (ImGui::BeginTabItem("Engine")) {
                Workbench::DrawEngineTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Hooks")) {
                Workbench::DrawHooksTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Memory")) {
                Workbench::DrawMemoryTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Log")) {
                Workbench::DrawLogTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("About")) {
                Workbench::DrawAboutTab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

HRESULT WINAPI ResetDetour(IDirect3DDevice9* device,
                           D3DPRESENT_PARAMETERS* parameters) {
    return OnReset(device, parameters, g_originalReset);
}

} // namespace

ImFont* CurrentInterfaceFont() {
    return g_interfaceFont.load();
}

ImFont* CurrentHeadingFont() {
    return g_headingFont.load();
}

ImFont* CurrentCodeFont() {
    return g_codeFont.load();
}

float CurrentDpiScale() {
    return g_dpiScale.load();
}

void PublishDevice(IDirect3DDevice9* device) {
    if (device != NULL) {
        g_seenDevice.store(device);
    }
}

void PollInstallDeviceHook() {
    if (g_resetHookReady.load() || Runtime::IsShuttingDown()) {
        return;
    }
    IDirect3DDevice9* device = g_seenDevice.load();
    if (device == NULL || !Memory::IsReadable(device, sizeof(void*))) {
        return;
    }
    void** vtable = NULL;
    void* reset = NULL;
    if (!Memory::TryRead(device, vtable) || vtable == NULL ||
        !Memory::TryRead(vtable + 16, reset)) {
        return;
    }
    if (!Memory::IsExecutable(reset) || !Memory::IsAddressInAnyModule(reset)) {
        return;
    }
    if (InternalHooks::Create("IDirect3DDevice9::Reset", reset,
                              reinterpret_cast<void*>(&ResetDetour),
                              reinterpret_cast<void**>(&g_originalReset))) {
        g_resetHookReady.store(true);
    }
}

void OnPresent(Diesel::D3DRenderDevice* renderDevice) {
    std::lock_guard<std::recursive_mutex> renderLock(g_renderMutex);
    IDirect3DDevice9* device = NULL;
    if (renderDevice == NULL ||
        !Memory::TryRead(&renderDevice->device.pointer, device) || device == NULL ||
        !Memory::IsReadable(device, sizeof(void*))) {
        return;
    }
    PublishDevice(device);

    if (Runtime::IsShuttingDown()) {
        ShutdownOnRenderThread();
        return;
    }
    if (!g_resetHookReady.load()) {
        return; // Never create DX9 objects until Reset interception is active.
    }
    if (g_initialized.load() && device != g_boundDevice) {
        SCREAM_LOG_WARN("D3D9 device changed from %p to %p; rebuilding ImGui", g_boundDevice,
                        device);
        if (!ShutdownOnRenderThread()) {
            return;
        }
        g_shutdownComplete.store(false);
    }
    if (!g_initialized.load() && !InitializeOnRenderThread(device)) {
        return;
    }

    DrainInputEvents();
    ImGui_ImplDX9_NewFrame();
    PlatformNewFrame();
    ImGui::NewFrame();

    const bool wantsVisible = g_visible.load();
    const float targetAlpha = wantsVisible ? 1.0f : 0.0f;
    const float deltaTime = std::max(0.0f, std::min(ImGui::GetIO().DeltaTime, 0.1f));
    const float blend = 1.0f - std::exp(-14.0f * deltaTime);
    g_overlayAlpha += (targetAlpha - g_overlayAlpha) * blend;
    if (wantsVisible && g_overlayAlpha > 0.995f) {
        g_overlayAlpha = 1.0f;
    } else if (!wantsVisible && g_overlayAlpha < 0.01f) {
        g_overlayAlpha = 0.0f;
    }

    if (wantsVisible || g_overlayAlpha > 0.0f) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_overlayAlpha);
        DrawRootWindow();
        ImGui::PopStyleVar();
        ImGuiIO& io = ImGui::GetIO();
        // Closing releases the game immediately while the last visual frames
        // fade out; a fading window never captures input invisibly.
        g_wantMouse.store(wantsVisible && io.WantCaptureMouse);
        g_wantKeyboard.store(wantsVisible && io.WantCaptureKeyboard);
        ImGui::Render();
        const HRESULT beginScene = device->BeginScene();
        if (SUCCEEDED(beginScene)) {
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            const HRESULT endScene = device->EndScene();
            if (FAILED(endScene) && !g_sceneFailureLogged) {
                SCREAM_LOG_WARN("IDirect3DDevice9::EndScene failed after ImGui rendering: 0x%08X",
                                static_cast<unsigned int>(endScene));
                g_sceneFailureLogged = true;
            }
        } else if (!g_sceneFailureLogged) {
            SCREAM_LOG_WARN("IDirect3DDevice9::BeginScene rejected ImGui rendering: 0x%08X",
                            static_cast<unsigned int>(beginScene));
            g_sceneFailureLogged = true;
        }
    } else {
        g_wantMouse.store(false);
        g_wantKeyboard.store(false);
        ImGui::EndFrame();
    }
    g_desiredCursor.store(ImGui::GetIO().MouseDrawCursor
                              ? ImGuiMouseCursor_None
                              : ImGui::GetMouseCursor());
    // A shutdown requested by the About tab is completed in the same Present,
    // while the D3D9/ImGui context is still owned by this render thread.
    if (Runtime::IsShuttingDown()) {
        ShutdownOnRenderThread();
    }
}

HRESULT OnReset(IDirect3DDevice9* device,
                D3DPRESENT_PARAMETERS* parameters,
                HRESULT (WINAPI* original)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*)) {
    std::lock_guard<std::recursive_mutex> renderLock(g_renderMutex);
    if (original == NULL) {
        return D3DERR_INVALIDCALL;
    }
    const bool ownsDevice = g_initialized.load() && device == g_boundDevice;
    if (ownsDevice) {
        ImGui_ImplDX9_InvalidateDeviceObjects();
    }
    const HRESULT result = original(device, parameters);
    if (ownsDevice && SUCCEEDED(result) && !Runtime::IsShuttingDown()) {
        if (!ImGui_ImplDX9_CreateDeviceObjects()) {
            SCREAM_LOG_WARN("ImGui DX9 object recreation failed after Reset");
        }
    }
    return result;
}

void RequestRenderThreadShutdown() {
    std::lock_guard<std::recursive_mutex> renderLock(g_renderMutex);
    ShutdownOnRenderThread();
}

bool IsRenderThreadShutdownComplete() {
    return g_shutdownComplete.load();
}

void Quiesce() {
    std::lock_guard<std::recursive_mutex> renderLock(g_renderMutex);
}

bool IsVisible() {
    return g_visible.load();
}

int ToggleKey() {
    return g_toggleKey.load();
}

void SetToggleKey(int virtualKey) {
    if (virtualKey > 0 && virtualKey < 256) {
        g_toggleKey.store(virtualKey);
    }
}

} // namespace Overlay
} // namespace Scream

namespace Scream {
namespace UI {

ImFont* InterfaceFont() {
    return Overlay::CurrentInterfaceFont();
}

ImFont* HeadingFont() {
    return Overlay::CurrentHeadingFont();
}

ImFont* CodeFont() {
    return Overlay::CurrentCodeFont();
}

float DpiScale() {
    return Overlay::CurrentDpiScale();
}

} // namespace UI
} // namespace Scream
