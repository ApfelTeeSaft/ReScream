include common.inc
include globals.inc

PUBLIC WndProc

.code

StartLaunch PROC STDCALL hWnd:DWORD
    LOCAL hThread:DWORD

    cmp gBusy, 0
    jne StartLaunchDone
    cmp gUpdateBusy, 0
    jne StartLaunchDone
    cmp gFilesReady, TRUE
    jne StartLaunchDone

    mov gBusy, TRUE
    mov gStatusPtr, OFFSET statusLaunching

    cmp gStartButton, 0
    je DisableIntegrity
    invoke EnableWindow, gStartButton, FALSE
    invoke SetWindowTextA, gStartButton, ADDR buttonBusyText
DisableIntegrity:
    cmp gIntegrityButton, 0
    je CreateWorker
    invoke EnableWindow, gIntegrityButton, FALSE

CreateWorker:
    invoke CreateThread, NULL, 0, OFFSET WorkerProc, NULL, 0, NULL
    test eax, eax
    jz WorkerThreadCreateFailed
    mov hThread, eax
    invoke CloseHandle, hThread
    jmp StartLaunchDone

WorkerThreadCreateFailed:
    mov gBusy, FALSE
    mov gStatusPtr, OFFSET statusThreadFailed
    cmp gStartButton, 0
    je RestoreIntegrityAfterThreadFail
    invoke SetWindowTextA, gStartButton, ADDR buttonText
    invoke EnableWindow, gStartButton, TRUE
RestoreIntegrityAfterThreadFail:
    cmp gIntegrityButton, 0
    je ShowThreadError
    invoke EnableWindow, gIntegrityButton, TRUE
ShowThreadError:
    invoke MessageBoxA, hWnd, ADDR statusThreadFailed, ADDR errorCaption, MB_OK OR MB_ICONERROR OR MB_SETFOREGROUND

StartLaunchDone:
    ret
StartLaunch ENDP

WndProc PROC STDCALL hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    LOCAL ps:PAINTSTRUCT
    LOCAL pErrorText:DWORD
    LOCAL controlId:DWORD
    LOCAL notifyCode:DWORD
    LOCAL icc:INITCOMMONCONTROLSEX

    mov eax, uMsg
    cmp eax, WM_CREATE
    je WmCreate
    cmp eax, WM_SIZE
    je WmSize
    cmp eax, WM_COMMAND
    je WmCommand
    cmp eax, WM_DRAWITEM
    je WmDrawItem
    cmp eax, WM_CTLCOLORSTATIC
    je WmCtlColorStatic
    cmp eax, WM_PAINT
    je WmPaint
    cmp eax, WM_TIMER
    je WmTimer
    cmp eax, WM_ERASEBKGND
    je WmEraseBackground
    cmp eax, WM_SETTINGCHANGE
    je WmThemeRefresh
    cmp eax, WM_THEMECHANGED
    je WmThemeRefresh
    cmp eax, WM_SYSCOLORCHANGE
    je WmThemeRefresh
    cmp eax, WM_APP_PROGRESS
    je WmAppProgress
    cmp eax, WM_APP_RESULT
    je WmAppResult
    cmp eax, WM_APP_UPDATE_STATUS
    je WmUpdateStatus
    cmp eax, WM_APP_UPDATE_PROGRESS
    je WmUpdateProgress
    cmp eax, WM_APP_UPDATE_RESULT
    je WmUpdateResult
    cmp eax, WM_APP_SELF_UPDATE
    je WmSelfUpdate
    cmp eax, WM_DESTROY
    je WmDestroy

    invoke DefWindowProcA, hWnd, uMsg, wParam, lParam
    ret

WmCreate:
    mov eax, hWnd
    mov gMainWnd, eax

    call BuildPaths
    call InitializeUiResources
    invoke RefreshTheme, hWnd

    mov icc.dwSize, SIZEOF INITCOMMONCONTROLSEX
    mov icc.dwICC, ICC_PROGRESS_CLASS
    invoke InitCommonControlsEx, ADDR icc

    invoke CreateWindowExA, 0, ADDR buttonClassName, ADDR buttonCheckingText, WS_CHILD OR WS_VISIBLE OR WS_TABSTOP OR BS_OWNERDRAW, 0, 0, START_BUTTON_W, START_BUTTON_H, hWnd, START_BUTTON_ID, gInstance, NULL
    mov gStartButton, eax
    test eax, eax
    jz CreateIntegrity
    invoke SendMessageA, gStartButton, WM_SETFONT, gButtonFont, TRUE
    invoke EnableWindow, gStartButton, FALSE

CreateIntegrity:
    invoke CreateWindowExA, 0, ADDR buttonClassName, ADDR integrityText, WS_CHILD OR WS_VISIBLE OR WS_TABSTOP OR BS_OWNERDRAW, 0, 0, INTEGRITY_BUTTON_W, INTEGRITY_BUTTON_H, hWnd, INTEGRITY_BUTTON_ID, gInstance, NULL
    mov gIntegrityButton, eax
    test eax, eax
    jz CreateProgressText
    invoke SendMessageA, gIntegrityButton, WM_SETFONT, gMonoFont, TRUE
    invoke EnableWindow, gIntegrityButton, FALSE

CreateProgressText:
    invoke CreateWindowExA, 0, ADDR staticClassName, ADDR updateStatusChecking, WS_CHILD OR SS_CENTER, 0, 0, 440, 20, hWnd, PROGRESS_TEXT_ID, gInstance, NULL
    mov gProgressText, eax
    test eax, eax
    jz CreateProgressBar
    invoke SendMessageA, gProgressText, WM_SETFONT, gMonoFont, TRUE

CreateProgressBar:
    invoke CreateWindowExA, 0, ADDR progressClassName, NULL, WS_CHILD OR PBS_SMOOTH, 0, 0, 440, 14, hWnd, PROGRESS_BAR_ID, gInstance, NULL
    mov gProgressBar, eax
    test eax, eax
    jz ControlsReady
    invoke SendMessageA, gProgressBar, PBM_SETRANGE32, 0, 100
    invoke SendMessageA, gProgressBar, PBM_SETBKCOLOR, 0, 00000000h
    invoke SendMessageA, gProgressBar, PBM_SETBARCOLOR, 0, 005BEA38h

ControlsReady:
    invoke PositionLauncherControls, hWnd

    invoke timeBeginPeriod, 1
    invoke timeGetTime
    mov gVisualStartTick, eax
    invoke SetTimer, hWnd, VISUALIZER_TIMER_ID, VISUALIZER_TIMER_MS, NULL

    invoke StartUpdateCheck, hWnd, UPDATE_MODE_STARTUP
    xor eax, eax
    ret

WmSize:
    invoke PositionLauncherControls, hWnd
    xor eax, eax
    ret

WmCommand:
    mov eax, wParam
    and eax, 0FFFFh
    mov controlId, eax

    mov eax, wParam
    shr eax, 16
    and eax, 0FFFFh
    mov notifyCode, eax
    cmp eax, BN_CLICKED
    jne CommandDone

    mov eax, controlId
    cmp eax, START_BUTTON_ID
    je CommandStart
    cmp eax, INTEGRITY_BUTTON_ID
    je CommandIntegrity
    jmp CommandDone

CommandStart:
    invoke StartLaunch, hWnd
    jmp CommandDone
CommandIntegrity:
    invoke StartUpdateCheck, hWnd, UPDATE_MODE_VERIFY

CommandDone:
    xor eax, eax
    ret

WmDrawItem:
    mov eax, wParam
    cmp eax, START_BUTTON_ID
    je DrawScopeControl
    cmp eax, INTEGRITY_BUTTON_ID
    je DrawScopeControl
    invoke DefWindowProcA, hWnd, uMsg, wParam, lParam
    ret
DrawScopeControl:
    invoke DrawScopeButton, lParam
    mov eax, TRUE
    ret

WmCtlColorStatic:
    mov eax, lParam
    cmp eax, gProgressText
    jne StaticDefault
    invoke SetTextColor, wParam, 008CFF72h
    invoke SetBkColor, wParam, 00000000h
    invoke GetStockObject, BLACK_BRUSH
    ret
StaticDefault:
    invoke DefWindowProcA, hWnd, uMsg, wParam, lParam
    ret

WmPaint:
    invoke BeginPaint, hWnd, ADDR ps
    push eax
    invoke PaintLauncher, hWnd, eax
    pop eax
    invoke EndPaint, hWnd, ADDR ps
    xor eax, eax
    ret

WmTimer:
    cmp wParam, VISUALIZER_TIMER_ID
    jne WmTimerDone
    invoke InvalidateRect, hWnd, NULL, FALSE
WmTimerDone:
    xor eax, eax
    ret

WmEraseBackground:
    mov eax, TRUE
    ret

WmThemeRefresh:
    invoke RefreshTheme, hWnd
    cmp gStartButton, 0
    je ThemeIntegrity
    invoke InvalidateRect, gStartButton, NULL, FALSE
ThemeIntegrity:
    cmp gIntegrityButton, 0
    je ThemeDone
    invoke InvalidateRect, gIntegrityButton, NULL, FALSE
ThemeDone:
    xor eax, eax
    ret

WmUpdateStatus:
    cmp gProgressText, 0
    je UpdateStatusDone
    invoke SetWindowTextA, gProgressText, ADDR gUpdateStatusText
UpdateStatusDone:
    xor eax, eax
    ret

WmUpdateProgress:
    cmp gProgressBar, 0
    je UpdateProgressDone
    invoke SendMessageA, gProgressBar, PBM_SETPOS, wParam, 0
UpdateProgressDone:
    xor eax, eax
    ret

WmUpdateResult:
    mov gUpdateBusy, FALSE

    cmp gProgressText, 0
    je HideUpdateBar
    invoke ShowWindow, gProgressText, SW_HIDE
HideUpdateBar:
    cmp gProgressBar, 0
    je RestoreUpdateButtons
    invoke ShowWindow, gProgressBar, SW_HIDE

RestoreUpdateButtons:
    cmp gIntegrityButton, 0
    je UpdateResultDecode
    invoke SetWindowTextA, gIntegrityButton, ADDR integrityText
    invoke EnableWindow, gIntegrityButton, TRUE

UpdateResultDecode:
    mov eax, wParam
    cmp eax, UPDATE_OK
    jne UpdateResultFailure

    mov gFilesReady, TRUE
    cmp gStartButton, 0
    je UpdateResultDone
    invoke SetWindowTextA, gStartButton, ADDR buttonText
    invoke EnableWindow, gStartButton, TRUE
    jmp UpdateResultDone

UpdateResultFailure:
    call CheckCoreFilesReady
    mov gFilesReady, eax
    cmp gStartButton, 0
    je ShowUpdateError
    invoke SetWindowTextA, gStartButton, ADDR buttonText
    invoke EnableWindow, gStartButton, gFilesReady
ShowUpdateError:
    invoke MessageBoxA, hWnd, ADDR gUpdateErrorText, ADDR updateErrorCaption, MB_OK OR MB_ICONERROR OR MB_SETFOREGROUND

UpdateResultDone:
    xor eax, eax
    ret

WmSelfUpdate:
    invoke PostMessageA, hWnd, WM_CLOSE, 0, 0
    xor eax, eax
    ret

WmAppProgress:
    cmp wParam, PROGRESS_WAITING
    jne ProgressDone
    mov gStatusPtr, OFFSET statusWaiting
ProgressDone:
    xor eax, eax
    ret

WmAppResult:
    mov gBusy, FALSE
    mov pErrorText, 0

    cmp gStartButton, 0
    je RestoreLaunchIntegrity
    invoke SetWindowTextA, gStartButton, ADDR buttonText
    invoke EnableWindow, gStartButton, TRUE
RestoreLaunchIntegrity:
    cmp gIntegrityButton, 0
    je ResultDecode
    invoke EnableWindow, gIntegrityButton, TRUE

ResultDecode:
    mov eax, wParam
    cmp eax, RESULT_OK
    je ResultSuccess
    cmp eax, RESULT_GAME_MISSING
    je ResultGameMissing
    cmp eax, RESULT_DLL_MISSING
    je ResultDllMissing
    cmp eax, RESULT_LAUNCH_FAILED
    je ResultLaunchFailed
    cmp eax, RESULT_INJECT_FAILED
    je ResultInjectFailed
    cmp eax, RESULT_THREAD_FAILED
    je ResultThreadFailed
    jmp ResultDone

ResultSuccess:
    mov gStatusPtr, OFFSET statusSuccess
    invoke PostMessageA, hWnd, WM_CLOSE, 0, 0
    jmp ResultDone
ResultGameMissing:
    mov gStatusPtr, OFFSET statusGameMissing
    mov pErrorText, OFFSET statusGameMissing
    jmp ResultShowError
ResultDllMissing:
    mov gStatusPtr, OFFSET statusDllMissing
    mov pErrorText, OFFSET statusDllMissing
    jmp ResultShowError
ResultLaunchFailed:
    mov gStatusPtr, OFFSET statusLaunchFailed
    mov pErrorText, OFFSET statusLaunchFailed
    jmp ResultShowError
ResultInjectFailed:
    mov gStatusPtr, OFFSET statusInjectFailed
IFDEF DEBUG_BUILD
    mov eax, gInjectStage
    cmp eax, INJECT_STAGE_ARCH_MISMATCH
    je ResultInjectArchMismatch
    cmp eax, INJECT_STAGE_LOADLIB_CALL
    je ResultInjectLoadLibraryNull
    mov pErrorText, OFFSET statusInjectRemoteSetup
    jmp ResultShowError

ResultInjectArchMismatch:
    mov pErrorText, OFFSET statusInjectArchMismatch
    jmp ResultShowError

ResultInjectLoadLibraryNull:
    mov pErrorText, OFFSET statusInjectLoadLibraryNull
    jmp ResultShowError
ELSE
    mov pErrorText, OFFSET statusInjectFailed
    jmp ResultShowError
ENDIF
ResultThreadFailed:
    mov gStatusPtr, OFFSET statusThreadFailed
    mov pErrorText, OFFSET statusThreadFailed
ResultShowError:
    invoke MessageBoxA, hWnd, pErrorText, ADDR errorCaption, MB_OK OR MB_ICONERROR OR MB_SETFOREGROUND
ResultDone:
    xor eax, eax
    ret

WmDestroy:
    invoke KillTimer, hWnd, VISUALIZER_TIMER_ID
    invoke timeEndPeriod, 1
    mov gStartButton, 0
    mov gIntegrityButton, 0
    mov gProgressBar, 0
    mov gProgressText, 0
    call DestroyUiResources
    invoke PostQuitMessage, 0
    xor eax, eax
    ret
WndProc ENDP

END
