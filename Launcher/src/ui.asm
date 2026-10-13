include common.inc
include globals.inc

PUBLIC InitializeUiResources, DestroyUiResources
PUBLIC PaintLauncher, PositionLauncherControls, DrawScopeButton

BUTTON_SCOPE_BG         EQU 00050802h
BUTTON_SCOPE_BG_DOWN    EQU 000C1A0Ah
BUTTON_SCOPE_GRID       EQU 00101E08h
BUTTON_SCOPE_BORDER     EQU 003C9B24h
BUTTON_SCOPE_BORDER_HOT EQU 005BEA38h
BUTTON_SCOPE_TEXT       EQU 008CFF72h
BUTTON_SCOPE_DISABLED   EQU 001B3D10h

.code

InitializeUiResources PROC
    invoke CreateFontA, -31, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH OR FF_DONTCARE, ADDR fontDisplayName
    mov gTitleFont, eax

    invoke CreateFontA, -15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH OR FF_DONTCARE, ADDR fontUiName
    mov gSubtitleFont, eax

    invoke CreateFontA, -19, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH OR FF_DONTCARE, ADDR fontMonoName
    mov gButtonFont, eax

    invoke CreateFontA, -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH OR FF_DONTCARE, ADDR fontMonoName
    mov gMonoFont, eax

    invoke CreateFontA, -62, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH OR FF_DONTCARE, ADDR fontVisualizerName
    mov gVisualizerFont, eax

    mov eax, TRUE
    ret
InitializeUiResources ENDP

DestroyUiResources PROC
    cmp gTitleFont, 0
    je DestroySubtitleFont
    invoke DeleteObject, gTitleFont
    mov gTitleFont, 0

DestroySubtitleFont:
    cmp gSubtitleFont, 0
    je DestroyButtonFont
    invoke DeleteObject, gSubtitleFont
    mov gSubtitleFont, 0

DestroyButtonFont:
    cmp gButtonFont, 0
    je DestroyMonoFont
    invoke DeleteObject, gButtonFont
    mov gButtonFont, 0

DestroyMonoFont:
    cmp gMonoFont, 0
    je DestroyVisualizerFont
    invoke DeleteObject, gMonoFont
    mov gMonoFont, 0

DestroyVisualizerFont:
    cmp gVisualizerFont, 0
    je DestroyDone
    invoke DeleteObject, gVisualizerFont
    mov gVisualizerFont, 0

DestroyDone:
    call DestroyVisualizerResources
    ret
DestroyUiResources ENDP

DrawFrame PROC STDCALL hdc:DWORD, pRect:DWORD, frameColor:DWORD
    LOCAL frameBrush:DWORD
    invoke CreateSolidBrush, frameColor
    mov frameBrush, eax
    invoke FrameRect, hdc, pRect, frameBrush
    invoke DeleteObject, frameBrush
    ret
DrawFrame ENDP

GetStartButtonRect PROC STDCALL hWnd:DWORD, pRect:DWORD
    LOCAL client:RECT
    invoke GetClientRect, hWnd, ADDR client
    mov edx, pRect
    mov eax, client.right
    sub eax, START_BUTTON_W
    sar eax, 1
    mov DWORD PTR [edx], eax
    mov eax, client.bottom
    sub eax, 86
    mov DWORD PTR [edx+4], eax
    mov eax, DWORD PTR [edx]
    add eax, START_BUTTON_W
    mov DWORD PTR [edx+8], eax
    mov eax, DWORD PTR [edx+4]
    add eax, START_BUTTON_H
    mov DWORD PTR [edx+12], eax
    ret
GetStartButtonRect ENDP

GetIntegrityButtonRect PROC STDCALL hWnd:DWORD, pRect:DWORD
    LOCAL client:RECT
    invoke GetClientRect, hWnd, ADDR client
    mov edx, pRect
    mov eax, client.right
    sub eax, INTEGRITY_BUTTON_W
    sar eax, 1
    mov DWORD PTR [edx], eax
    mov eax, client.bottom
    sub eax, 126
    mov DWORD PTR [edx+4], eax
    mov eax, DWORD PTR [edx]
    add eax, INTEGRITY_BUTTON_W
    mov DWORD PTR [edx+8], eax
    mov eax, DWORD PTR [edx+4]
    add eax, INTEGRITY_BUTTON_H
    mov DWORD PTR [edx+12], eax
    ret
GetIntegrityButtonRect ENDP

PositionLauncherControls PROC STDCALL hWnd:DWORD
    LOCAL rc:RECT
    LOCAL client:RECT
    LOCAL ctlW:DWORD
    LOCAL ctlH:DWORD
    LOCAL x:DWORD
    LOCAL y:DWORD

    cmp gStartButton, 0
    je PositionIntegrity
    invoke GetStartButtonRect, hWnd, ADDR rc
    mov eax, rc.right
    sub eax, rc.left
    mov ctlW, eax
    mov eax, rc.bottom
    sub eax, rc.top
    mov ctlH, eax
    invoke MoveWindow, gStartButton, rc.left, rc.top, ctlW, ctlH, TRUE

PositionIntegrity:
    cmp gIntegrityButton, 0
    je PositionProgress
    invoke GetIntegrityButtonRect, hWnd, ADDR rc
    mov eax, rc.right
    sub eax, rc.left
    mov ctlW, eax
    mov eax, rc.bottom
    sub eax, rc.top
    mov ctlH, eax
    invoke MoveWindow, gIntegrityButton, rc.left, rc.top, ctlW, ctlH, TRUE

PositionProgress:
    invoke GetClientRect, hWnd, ADDR client
    mov eax, client.right
    sub eax, 440
    sar eax, 1
    mov x, eax
    mov eax, client.bottom
    sub eax, 170
    mov y, eax

    cmp gProgressText, 0
    je PositionBar
    invoke MoveWindow, gProgressText, x, y, 440, 20, TRUE
PositionBar:
    cmp gProgressBar, 0
    je PositionDone
    mov eax, y
    add eax, 24
    invoke MoveWindow, gProgressBar, x, eax, 440, 14, TRUE
PositionDone:
    ret
PositionLauncherControls ENDP

DrawScopeButton PROC STDCALL pDrawItem:DWORD
    LOCAL hdcButton:DWORD
    LOCAL hwndButton:DWORD
    LOCAL buttonLabel[64]:BYTE
    LOCAL itemState:DWORD
    LOCAL rc:RECT
    LOCAL innerRc:RECT
    LOCAL textRc:RECT
    LOCAL fillBrush:DWORD
    LOCAL gridPen:DWORD
    LOCAL oldPen:DWORD
    LOCAL textColor:DWORD
    LOCAL borderColor:DWORD
    LOCAL pText:DWORD
    LOCAL gridX:DWORD
    LOCAL gridY:DWORD
    LOCAL lineX1:DWORD
    LOCAL lineY1:DWORD
    LOCAL lineX2:DWORD
    LOCAL lineY2:DWORD

    mov edx, pDrawItem
    test edx, edx
    jz ButtonDrawDone

    mov eax, DWORD PTR [edx+20]
    mov hwndButton, eax
    mov eax, DWORD PTR [edx+24]
    mov hdcButton, eax
    mov eax, DWORD PTR [edx+16]
    mov itemState, eax

    mov eax, DWORD PTR [edx+28]
    mov rc.left, eax
    mov eax, DWORD PTR [edx+32]
    mov rc.top, eax
    mov eax, DWORD PTR [edx+36]
    mov rc.right, eax
    mov eax, DWORD PTR [edx+40]
    mov rc.bottom, eax

    mov eax, BUTTON_SCOPE_BG
    test itemState, ODS_SELECTED
    jz FillReady
    mov eax, BUTTON_SCOPE_BG_DOWN
FillReady:
    invoke CreateSolidBrush, eax
    mov fillBrush, eax
    invoke FillRect, hdcButton, ADDR rc, fillBrush
    invoke DeleteObject, fillBrush

    invoke CreatePen, PS_SOLID, 1, BUTTON_SCOPE_GRID
    mov gridPen, eax
    invoke SelectObject, hdcButton, gridPen
    mov oldPen, eax

    mov eax, rc.left
    add eax, 32
    mov gridX, eax
ButtonGridVertical:
    mov eax, gridX
    mov ecx, rc.right
    sub ecx, 8
    cmp eax, ecx
    jge ButtonGridHorizontalSetup
    mov ecx, rc.top
    add ecx, 4
    invoke MoveToEx, hdcButton, gridX, ecx, NULL
    mov ecx, rc.bottom
    sub ecx, 4
    invoke LineTo, hdcButton, gridX, ecx
    add gridX, 32
    jmp ButtonGridVertical

ButtonGridHorizontalSetup:
    mov eax, rc.top
    add eax, 18
    mov gridY, eax
ButtonGridHorizontal:
    mov eax, gridY
    mov ecx, rc.bottom
    sub ecx, 4
    cmp eax, ecx
    jge ButtonGridDone
    mov ecx, rc.left
    add ecx, 4
    invoke MoveToEx, hdcButton, ecx, gridY, NULL
    mov ecx, rc.right
    sub ecx, 4
    invoke LineTo, hdcButton, ecx, gridY
    add gridY, 18
    jmp ButtonGridHorizontal

ButtonGridDone:
    invoke SelectObject, hdcButton, oldPen
    invoke DeleteObject, gridPen

    mov eax, BUTTON_SCOPE_BORDER
    mov borderColor, eax
    mov eax, BUTTON_SCOPE_TEXT
    mov textColor, eax

    test itemState, ODS_DISABLED
    jz ButtonEnabledStyle
    mov eax, BUTTON_SCOPE_DISABLED
    mov borderColor, eax
    mov textColor, eax
    jmp ButtonStyleReady

ButtonEnabledStyle:
    test itemState, ODS_FOCUS
    jz ButtonStyleReady
    mov eax, BUTTON_SCOPE_BORDER_HOT
    mov borderColor, eax

ButtonStyleReady:
    invoke DrawFrame, hdcButton, ADDR rc, borderColor

    mov eax, rc.left
    add eax, 3
    mov innerRc.left, eax
    mov eax, rc.top
    add eax, 3
    mov innerRc.top, eax
    mov eax, rc.right
    sub eax, 3
    mov innerRc.right, eax
    mov eax, rc.bottom
    sub eax, 3
    mov innerRc.bottom, eax
    invoke DrawFrame, hdcButton, ADDR innerRc, BUTTON_SCOPE_GRID

    invoke CreatePen, PS_SOLID, 2, borderColor
    mov gridPen, eax
    invoke SelectObject, hdcButton, gridPen
    mov oldPen, eax

    ; top-left horizontal
    mov eax, rc.left
    add eax, 7
    mov lineX1, eax
    mov eax, rc.top
    add eax, 7
    mov lineY1, eax
    mov eax, lineX1
    add eax, 14
    mov lineX2, eax
    mov eax, lineY1
    mov lineY2, eax
    invoke MoveToEx, hdcButton, lineX1, lineY1, NULL
    invoke LineTo, hdcButton, lineX2, lineY2

    ; top-left vertical
    mov eax, rc.left
    add eax, 7
    mov lineX1, eax
    mov lineX2, eax
    mov eax, rc.top
    add eax, 7
    mov lineY1, eax
    add eax, 9
    mov lineY2, eax
    invoke MoveToEx, hdcButton, lineX1, lineY1, NULL
    invoke LineTo, hdcButton, lineX2, lineY2

    ; top-right horizontal
    mov eax, rc.right
    sub eax, 7
    mov lineX1, eax
    mov eax, lineX1
    sub eax, 14
    mov lineX2, eax
    mov eax, rc.top
    add eax, 7
    mov lineY1, eax
    mov lineY2, eax
    invoke MoveToEx, hdcButton, lineX1, lineY1, NULL
    invoke LineTo, hdcButton, lineX2, lineY2

    ; top-right vertical
    mov eax, rc.right
    sub eax, 7
    mov lineX1, eax
    mov lineX2, eax
    mov eax, rc.top
    add eax, 7
    mov lineY1, eax
    add eax, 9
    mov lineY2, eax
    invoke MoveToEx, hdcButton, lineX1, lineY1, NULL
    invoke LineTo, hdcButton, lineX2, lineY2

    ; bottom-left horizontal
    mov eax, rc.left
    add eax, 7
    mov lineX1, eax
    mov eax, lineX1
    add eax, 14
    mov lineX2, eax
    mov eax, rc.bottom
    sub eax, 7
    mov lineY1, eax
    mov lineY2, eax
    invoke MoveToEx, hdcButton, lineX1, lineY1, NULL
    invoke LineTo, hdcButton, lineX2, lineY2

    ; bottom-left vertical
    mov eax, rc.left
    add eax, 7
    mov lineX1, eax
    mov lineX2, eax
    mov eax, rc.bottom
    sub eax, 7
    mov lineY1, eax
    sub eax, 9
    mov lineY2, eax
    invoke MoveToEx, hdcButton, lineX1, lineY1, NULL
    invoke LineTo, hdcButton, lineX2, lineY2

    ; bottom-right horizontal
    mov eax, rc.right
    sub eax, 7
    mov lineX1, eax
    mov eax, lineX1
    sub eax, 14
    mov lineX2, eax
    mov eax, rc.bottom
    sub eax, 7
    mov lineY1, eax
    mov lineY2, eax
    invoke MoveToEx, hdcButton, lineX1, lineY1, NULL
    invoke LineTo, hdcButton, lineX2, lineY2

    ; bottom-right vertical
    mov eax, rc.right
    sub eax, 7
    mov lineX1, eax
    mov lineX2, eax
    mov eax, rc.bottom
    sub eax, 7
    mov lineY1, eax
    sub eax, 9
    mov lineY2, eax
    invoke MoveToEx, hdcButton, lineX1, lineY1, NULL
    invoke LineTo, hdcButton, lineX2, lineY2

    invoke SelectObject, hdcButton, oldPen
    invoke DeleteObject, gridPen

    mov eax, rc.left
    mov textRc.left, eax
    mov eax, rc.top
    mov textRc.top, eax
    mov eax, rc.right
    mov textRc.right, eax
    mov eax, rc.bottom
    mov textRc.bottom, eax

    test itemState, ODS_SELECTED
    jz TextPositionReady
    inc textRc.left
    inc textRc.top
    inc textRc.right
    inc textRc.bottom
TextPositionReady:

    invoke GetWindowTextA, hwndButton, ADDR buttonLabel, SIZEOF buttonLabel
    lea eax, buttonLabel
    mov pText, eax
    invoke SelectObject, hdcButton, gButtonFont
    invoke SetBkMode, hdcButton, TRANSPARENT
    invoke SetTextColor, hdcButton, textColor
    invoke DrawTextA, hdcButton, pText, -1, ADDR textRc, DT_CENTER OR DT_VCENTER OR DT_SINGLELINE OR DT_NOPREFIX

ButtonDrawDone:
    mov eax, TRUE
    ret
DrawScopeButton ENDP

PaintLauncher PROC STDCALL hWnd:DWORD, hdc:DWORD
    LOCAL client:RECT

    invoke GetClientRect, hWnd, ADDR client

    invoke DrawVisualizer, hdc, ADDR client
    ret
PaintLauncher ENDP

END
