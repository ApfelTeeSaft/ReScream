include common.inc
include globals.inc

PUBLIC start

.code

start PROC
    LOCAL wc:WNDCLASSEXA
    LOCAL msg:MSG
    LOCAL xPos:DWORD
    LOCAL yPos:DWORD

    push edi

    call TryRunApplyUpdateMode

    call BuildPaths
    test eax, eax
    jz AppExit
    call CleanupUpdateStub

    invoke GetModuleHandleA, NULL
    mov gInstance, eax

    lea edi, wc
    xor eax, eax
    mov ecx, SIZEOF WNDCLASSEXA / 4
    rep stosd

    mov wc.cbSize, SIZEOF WNDCLASSEXA
    mov wc.style, CS_HREDRAW OR CS_VREDRAW
    mov wc.lpfnWndProc, OFFSET WndProc
    mov eax, gInstance
    mov wc.hInstance, eax
    mov wc.lpszClassName, OFFSET className

    invoke LoadIconA, gInstance, IDI_APP_ICON
    mov wc.hIcon, eax
    mov wc.hIconSm, eax

    invoke LoadCursorA, NULL, IDC_ARROW
    mov wc.hCursor, eax

    invoke RegisterClassExA, ADDR wc
    test eax, eax
    jz AppExit

    invoke GetSystemMetrics, SM_CXSCREEN
    sub eax, WINDOW_W
    sar eax, 1
    mov xPos, eax

    invoke GetSystemMetrics, SM_CYSCREEN
    sub eax, WINDOW_H
    sar eax, 1
    mov yPos, eax

    invoke CreateWindowExA, 0, ADDR className, ADDR appTitle, WS_CAPTION OR WS_SYSMENU OR WS_MINIMIZEBOX OR WS_CLIPCHILDREN, xPos, yPos, WINDOW_W, WINDOW_H, NULL, NULL, gInstance, NULL
    test eax, eax
    jz AppExit
    mov gMainWnd, eax

    invoke ShowWindow, gMainWnd, SW_SHOWNORMAL
    invoke UpdateWindow, gMainWnd

MessageLoop:
    invoke GetMessageA, ADDR msg, NULL, 0, 0
    cmp eax, 0
    jle AppExit
    invoke TranslateMessage, ADDR msg
    invoke DispatchMessageA, ADDR msg
    jmp MessageLoop

AppExit:
    pop edi
    invoke ExitProcess, 0
    ret
start ENDP

END start
