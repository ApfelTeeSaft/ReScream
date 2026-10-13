.386
.model flat, stdcall
option casemap:none

NULL                    EQU 0
FALSE                   EQU 0
TRUE                    EQU 1

CS_VREDRAW              EQU 0001h
CS_HREDRAW              EQU 0002h
WS_OVERLAPPEDWINDOW     EQU 00CF0000h
WS_CAPTION              EQU 00C00000h
WS_SYSMENU              EQU 00080000h
WS_MINIMIZEBOX          EQU 00020000h
WS_CHILD                EQU 40000000h
WS_VISIBLE              EQU 10000000h
WS_TABSTOP              EQU 00010000h
WS_CLIPCHILDREN         EQU 02000000h
BS_PUSHBUTTON           EQU 00000000h
BS_OWNERDRAW            EQU 0000000Bh
SW_HIDE                 EQU 0
SW_SHOWNORMAL           EQU 1
SW_SHOW                 EQU 5
SS_CENTER               EQU 00000001h
PBS_SMOOTH               EQU 00000001h

WM_CREATE               EQU 0001h
WM_CLOSE                EQU 0010h
WM_DESTROY              EQU 0002h
WM_SIZE                 EQU 0005h
WM_DRAWITEM             EQU 002Bh
WM_CTLCOLORSTATIC       EQU 0138h
WM_PAINT                EQU 000Fh
WM_ERASEBKGND           EQU 0014h
WM_SYSCOLORCHANGE       EQU 0015h
WM_SETTINGCHANGE        EQU 001Ah
WM_COMMAND              EQU 0111h
WM_TIMER                EQU 0113h
WM_SETFONT               EQU 0030h
WM_GETMINMAXINFO        EQU 0024h
WM_MOUSEMOVE            EQU 0200h
WM_LBUTTONUP            EQU 0202h
WM_MOUSELEAVE           EQU 02A3h
WM_THEMECHANGED         EQU 031Ah
WM_APP                  EQU 8000h
WM_APP_RESULT           EQU WM_APP + 1
WM_APP_PROGRESS         EQU WM_APP + 2
WM_APP_UPDATE_STATUS    EQU WM_APP + 10
WM_APP_UPDATE_PROGRESS  EQU WM_APP + 11
WM_APP_UPDATE_RESULT    EQU WM_APP + 12
WM_APP_SELF_UPDATE      EQU WM_APP + 13

TME_LEAVE               EQU 00000002h

DT_CENTER               EQU 00000001h
DT_LEFT                 EQU 00000000h
DT_RIGHT                EQU 00000002h
DT_VCENTER              EQU 00000004h
DT_SINGLELINE           EQU 00000020h
DT_END_ELLIPSIS         EQU 00008000h
DT_NOPREFIX             EQU 00000800h

TRANSPARENT             EQU 1
PS_SOLID                EQU 0
ANTIALIASED_QUALITY     EQU 4
CLEARTYPE_QUALITY       EQU 5
DEFAULT_CHARSET         EQU 1
OUT_DEFAULT_PRECIS      EQU 0
CLIP_DEFAULT_PRECIS     EQU 0
DEFAULT_PITCH           EQU 0
FF_DONTCARE             EQU 0
FW_NORMAL               EQU 400
FW_SEMIBOLD             EQU 600
FW_BOLD                 EQU 700
NULL_BRUSH              EQU 5

IDC_ARROW               EQU 32512
SM_CXSCREEN             EQU 0
SM_CYSCREEN             EQU 1

SRCCOPY                 EQU 00CC0020h
HALFTONE                EQU 4

INVALID_FILE_ATTRIBUTES EQU 0FFFFFFFFh
FILE_ATTRIBUTE_DIRECTORY EQU 00000010h
FILE_ATTRIBUTE_NORMAL    EQU 00000080h
INVALID_HANDLE_VALUE     EQU 0FFFFFFFFh
GENERIC_READ             EQU 80000000h
GENERIC_WRITE            EQU 40000000h
FILE_SHARE_READ          EQU 00000001h
CREATE_ALWAYS            EQU 2
OPEN_EXISTING            EQU 3
MOVEFILE_REPLACE_EXISTING EQU 00000001h
MOVEFILE_WRITE_THROUGH   EQU 00000008h

MEM_COMMIT              EQU 00001000h
MEM_RESERVE             EQU 00002000h
MEM_RELEASE             EQU 00008000h
PAGE_READWRITE          EQU 04h
INFINITE                EQU 0FFFFFFFFh
WAIT_OBJECT_0           EQU 0
SYNCHRONIZE              EQU 00100000h
HEAP_ZERO_MEMORY         EQU 00000008h

TH32CS_SNAPMODULE       EQU 00000008h
TH32CS_SNAPMODULE32     EQU 00000010h
GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT EQU 00000002h
GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS       EQU 00000004h
MAX_PATH                EQU 260

INJECT_STAGE_NONE               EQU 0
INJECT_STAGE_ARCH_MISMATCH      EQU 1
INJECT_STAGE_ALLOC_DLL_PATH     EQU 2
INJECT_STAGE_WRITE_DLL_PATH     EQU 3
INJECT_STAGE_ALLOC_BASE_DIR     EQU 4
INJECT_STAGE_WRITE_BASE_DIR     EQU 5
INJECT_STAGE_RESOLVE_DLL_DIR    EQU 6
INJECT_STAGE_SET_DLL_DIR_THREAD EQU 7
INJECT_STAGE_SET_DLL_DIR_CALL   EQU 8
INJECT_STAGE_RESOLVE_LOADLIB    EQU 9
INJECT_STAGE_LOADLIB_THREAD     EQU 10
INJECT_STAGE_LOADLIB_CALL       EQU 11
INJECT_STAGE_REMOTE_WAIT        EQU 12

MB_OK                   EQU 00000000h
MB_ICONERROR            EQU 00000010h
MB_SETFOREGROUND        EQU 00010000h

HKEY_CURRENT_USER       EQU 80000001h
KEY_QUERY_VALUE         EQU 0001h
REG_DWORD               EQU 4
ERROR_SUCCESS           EQU 0

DWMWA_USE_IMMERSIVE_DARK_MODE_OLD EQU 19
DWMWA_USE_IMMERSIVE_DARK_MODE     EQU 20

IDI_APP_ICON            EQU 101

PATH_CAP                EQU 1024
WINDOW_W                EQU 680
WINDOW_H                EQU 720
MIN_WINDOW_W            EQU 560
MIN_WINDOW_H            EQU 620
START_BUTTON_W          EQU 260
START_BUTTON_H          EQU 54
VISUALIZER_TIMER_ID     EQU 1
VISUALIZER_TIMER_MS     EQU 16
VISUALIZER_SRC_W        EQU 420
VISUALIZER_SRC_H        EQU 104
START_BUTTON_ID         EQU 1001
INTEGRITY_BUTTON_ID     EQU 1002
PROGRESS_BAR_ID         EQU 1003
PROGRESS_TEXT_ID        EQU 1004
INTEGRITY_BUTTON_W      EQU 190
INTEGRITY_BUTTON_H      EQU 32
BN_CLICKED              EQU 0

WM_USER                 EQU 0400h
PBM_SETRANGE32          EQU WM_USER + 6
PBM_SETPOS              EQU WM_USER + 2
PBM_SETBARCOLOR         EQU WM_USER + 9
PBM_SETBKCOLOR          EQU 2001h
ICC_PROGRESS_CLASS      EQU 00000020h
BLACK_BRUSH             EQU 4

ODT_BUTTON              EQU 4
ODS_SELECTED            EQU 0001h
ODS_GRAYED              EQU 0002h
ODS_DISABLED            EQU 0004h
ODS_FOCUS               EQU 0010h

THEME_LIGHT             EQU 0
THEME_DARK              EQU 1

RESULT_OK               EQU 1
RESULT_GAME_MISSING     EQU 2
RESULT_DLL_MISSING      EQU 3
RESULT_LAUNCH_FAILED    EQU 4
RESULT_INJECT_FAILED    EQU 5
RESULT_THREAD_FAILED    EQU 6

PROGRESS_WAITING        EQU 1

UPDATE_MODE_STARTUP      EQU 0
UPDATE_MODE_VERIFY       EQU 1
UPDATE_OK                EQU 1
UPDATE_ERR_NETWORK       EQU 2
UPDATE_ERR_MANIFEST      EQU 3
UPDATE_ERR_HASH          EQU 4
UPDATE_ERR_FILE          EQU 5
UPDATE_ERR_SELF          EQU 6
UPDATE_SELF_RESTARTING   EQU 7

MAX_MANIFEST_ENTRIES    EQU 1024
ENTRY_SIZE               EQU 40
ENTRY_HASH_OFF           EQU 0
ENTRY_PATH_OFF           EQU 32
ENTRY_FLAGS_OFF          EQU 36
ENTRY_FLAG_DOWNLOAD      EQU 00000001h

INTERNET_OPEN_TYPE_PRECONFIG EQU 0
INTERNET_FLAG_RELOAD     EQU 80000000h
INTERNET_FLAG_NO_CACHE_WRITE EQU 04000000h
INTERNET_FLAG_SECURE     EQU 00800000h
INTERNET_FLAG_NO_UI      EQU 00000200h
HTTP_QUERY_CONTENT_LENGTH EQU 5
HTTP_QUERY_STATUS_CODE   EQU 19
HTTP_QUERY_FLAG_NUMBER   EQU 20000000h

PROV_RSA_AES             EQU 24
CRYPT_VERIFYCONTEXT      EQU 0F0000000h
CALG_SHA_256             EQU 0000800Ch
HP_HASHVAL               EQU 2

WNDCLASSEXA STRUCT
    cbSize          DWORD ?
    style           DWORD ?
    lpfnWndProc     DWORD ?
    cbClsExtra      DWORD ?
    cbWndExtra      DWORD ?
    hInstance       DWORD ?
    hIcon           DWORD ?
    hCursor         DWORD ?
    hbrBackground   DWORD ?
    lpszMenuName    DWORD ?
    lpszClassName   DWORD ?
    hIconSm         DWORD ?
WNDCLASSEXA ENDS

POINT STRUCT
    x               DWORD ?
    y               DWORD ?
POINT ENDS

MSG STRUCT
    hwnd            DWORD ?
    message         DWORD ?
    wParam          DWORD ?
    lParam          DWORD ?
    time            DWORD ?
    pt              POINT <>
MSG ENDS

RECT STRUCT
    left            DWORD ?
    top             DWORD ?
    right           DWORD ?
    bottom          DWORD ?
RECT ENDS

PAINTSTRUCT STRUCT
    hdc             DWORD ?
    fErase          DWORD ?
    rcPaint         RECT <>
    fRestore        DWORD ?
    fIncUpdate      DWORD ?
    rgbReserved     BYTE 32 DUP(?)
PAINTSTRUCT ENDS

STARTUPINFOA STRUCT
    cb              DWORD ?
    lpReserved      DWORD ?
    lpDesktop       DWORD ?
    lpTitle         DWORD ?
    dwX             DWORD ?
    dwY             DWORD ?
    dwXSize         DWORD ?
    dwYSize         DWORD ?
    dwXCountChars   DWORD ?
    dwYCountChars   DWORD ?
    dwFillAttribute DWORD ?
    dwFlags         DWORD ?
    wShowWindow     WORD ?
    cbReserved2     WORD ?
    lpReserved2     DWORD ?
    hStdInput       DWORD ?
    hStdOutput      DWORD ?
    hStdError       DWORD ?
STARTUPINFOA ENDS

PROCESS_INFORMATION STRUCT
    hProcess        DWORD ?
    hThread         DWORD ?
    dwProcessId     DWORD ?
    dwThreadId      DWORD ?
PROCESS_INFORMATION ENDS

TRACKMOUSEEVENT STRUCT
    cbSize          DWORD ?
    dwFlags         DWORD ?
    hwndTrack       DWORD ?
    dwHoverTime     DWORD ?
TRACKMOUSEEVENT ENDS

DRAWITEMSTRUCT STRUCT
    CtlType         DWORD ?
    CtlID           DWORD ?
    itemID          DWORD ?
    itemAction      DWORD ?
    itemState       DWORD ?
    hwndItem        DWORD ?
    hDC             DWORD ?
    rcItem          RECT <>
    itemData        DWORD ?
DRAWITEMSTRUCT ENDS

INITCOMMONCONTROLSEX STRUCT
    dwSize          DWORD ?
    dwICC           DWORD ?
INITCOMMONCONTROLSEX ENDS

MODULEENTRY32A STRUCT
    dwSize          DWORD ?
    th32ModuleID    DWORD ?
    th32ProcessID   DWORD ?
    GlblcntUsage    DWORD ?
    ProccntUsage    DWORD ?
    modBaseAddr     DWORD ?
    modBaseSize     DWORD ?
    hModule         DWORD ?
    szModule        BYTE 256 DUP(?)
    szExePath       BYTE MAX_PATH DUP(?)
MODULEENTRY32A ENDS

GetModuleHandleA         PROTO STDCALL :DWORD
GetModuleHandleExA       PROTO STDCALL :DWORD, :DWORD, :DWORD
GetCurrentProcess        PROTO STDCALL
GetProcessId             PROTO STDCALL :DWORD
IsWow64Process           PROTO STDCALL :DWORD, :DWORD
CreateToolhelp32Snapshot PROTO STDCALL :DWORD, :DWORD
Module32First            PROTO STDCALL :DWORD, :DWORD
Module32Next             PROTO STDCALL :DWORD, :DWORD
GetLastError             PROTO STDCALL
GetModuleFileNameA       PROTO STDCALL :DWORD, :DWORD, :DWORD
GetCommandLineA          PROTO STDCALL
GetCurrentProcessId      PROTO STDCALL
GetProcessHeap           PROTO STDCALL
HeapAlloc                PROTO STDCALL :DWORD, :DWORD, :DWORD
HeapFree                 PROTO STDCALL :DWORD, :DWORD, :DWORD
CreateFileA              PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
ReadFile                 PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
WriteFile                PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
GetFileSize              PROTO STDCALL :DWORD, :DWORD
CreateDirectoryA         PROTO STDCALL :DWORD, :DWORD
DeleteFileA              PROTO STDCALL :DWORD
MoveFileExA              PROTO STDCALL :DWORD, :DWORD, :DWORD
CopyFileA                PROTO STDCALL :DWORD, :DWORD, :DWORD
OpenProcess              PROTO STDCALL :DWORD, :DWORD, :DWORD
GetFileAttributesA       PROTO STDCALL :DWORD
GetProcAddress           PROTO STDCALL :DWORD, :DWORD
lstrlenA                 PROTO STDCALL :DWORD
lstrcpyA                 PROTO STDCALL :DWORD, :DWORD
lstrcatA                 PROTO STDCALL :DWORD, :DWORD
lstrcpynA                PROTO STDCALL :DWORD, :DWORD, :DWORD
lstrcmpA                 PROTO STDCALL :DWORD, :DWORD
lstrcmpiA                PROTO STDCALL :DWORD, :DWORD
CreateProcessA           PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
CreateThread             PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
CreateRemoteThread       PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
VirtualAllocEx           PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
VirtualFreeEx            PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD
WriteProcessMemory       PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
WaitForSingleObject      PROTO STDCALL :DWORD, :DWORD
GetExitCodeThread        PROTO STDCALL :DWORD, :DWORD
Sleep                    PROTO STDCALL :DWORD
GetTickCount             PROTO STDCALL
timeGetTime              PROTO STDCALL
timeBeginPeriod          PROTO STDCALL :DWORD
timeEndPeriod            PROTO STDCALL :DWORD
CloseHandle              PROTO STDCALL :DWORD
ExitProcess              PROTO STDCALL :DWORD

RegisterClassExA         PROTO STDCALL :DWORD
CreateWindowExA          PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
DefWindowProcA           PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD
SendMessageA             PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD
MoveWindow               PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
EnableWindow             PROTO STDCALL :DWORD, :DWORD
SetWindowTextA           PROTO STDCALL :DWORD, :DWORD
GetWindowTextA           PROTO STDCALL :DWORD, :DWORD, :DWORD
ShowWindow               PROTO STDCALL :DWORD, :DWORD
UpdateWindow             PROTO STDCALL :DWORD
GetMessageA              PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD
TranslateMessage         PROTO STDCALL :DWORD
DispatchMessageA         PROTO STDCALL :DWORD
PostQuitMessage          PROTO STDCALL :DWORD
PostMessageA             PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD
MessageBoxA              PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD
BeginPaint               PROTO STDCALL :DWORD, :DWORD
EndPaint                 PROTO STDCALL :DWORD, :DWORD
GetClientRect            PROTO STDCALL :DWORD, :DWORD
InvalidateRect           PROTO STDCALL :DWORD, :DWORD, :DWORD
LoadIconA                PROTO STDCALL :DWORD, :DWORD
LoadCursorA              PROTO STDCALL :DWORD, :DWORD
LoadBitmapA              PROTO STDCALL :DWORD, :DWORD
TrackMouseEvent          PROTO STDCALL :DWORD
GetSystemMetrics         PROTO STDCALL :DWORD
SetTimer                 PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD
KillTimer                PROTO STDCALL :DWORD, :DWORD

CreateSolidBrush         PROTO STDCALL :DWORD
CreatePen                PROTO STDCALL :DWORD, :DWORD, :DWORD
DeleteObject             PROTO STDCALL :DWORD
CreateCompatibleDC       PROTO STDCALL :DWORD
CreateCompatibleBitmap   PROTO STDCALL :DWORD, :DWORD, :DWORD
DeleteDC                 PROTO STDCALL :DWORD
SelectObject             PROTO STDCALL :DWORD, :DWORD
GetStockObject           PROTO STDCALL :DWORD
FillRect                 PROTO STDCALL :DWORD, :DWORD, :DWORD
FrameRect                PROTO STDCALL :DWORD, :DWORD, :DWORD
Rectangle                PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
SetBkMode                PROTO STDCALL :DWORD, :DWORD
SetTextColor             PROTO STDCALL :DWORD, :DWORD
SetBkColor               PROTO STDCALL :DWORD, :DWORD
DrawTextA                PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
CreateFontA              PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
StretchBlt               PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
SetStretchBltMode        PROTO STDCALL :DWORD, :DWORD
BitBlt                   PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
TransparentBlt           PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
MoveToEx                 PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD
LineTo                   PROTO STDCALL :DWORD, :DWORD, :DWORD

RegOpenKeyExA            PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
RegQueryValueExA         PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
RegCloseKey              PROTO STDCALL :DWORD

CryptAcquireContextA     PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
CryptCreateHash          PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
CryptHashData            PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD
CryptGetHashParam        PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
CryptDestroyHash         PROTO STDCALL :DWORD
CryptReleaseContext      PROTO STDCALL :DWORD, :DWORD

InternetOpenA            PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
InternetOpenUrlA         PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
InternetReadFile         PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD
InternetCloseHandle      PROTO STDCALL :DWORD
HttpQueryInfoA           PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD

InitCommonControlsEx     PROTO STDCALL :DWORD

DwmSetWindowAttribute    PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD

includelib kernel32.lib
includelib user32.lib
includelib gdi32.lib
includelib advapi32.lib
includelib dwmapi.lib
includelib msimg32.lib
includelib winmm.lib
includelib wininet.lib
includelib comctl32.lib
