include common.inc
include globals.inc

PUBLIC WorkerProc

INJECT_RETRY_INTERVAL_MS EQU 5000
INJECT_TIMEOUT_MS        EQU 60000
INJECT_MAX_ATTEMPTS      EQU (INJECT_TIMEOUT_MS / INJECT_RETRY_INTERVAL_MS) + 1

.code

WorkerProc PROC STDCALL lpParam:DWORD
    LOCAL startupInfo:STARTUPINFOA
    LOCAL procInfo:PROCESS_INFORMATION
    LOCAL attemptsRemaining:DWORD

    push edi
    mov eax, lpParam

    call BuildPaths
    test eax, eax
    jz WorkerLaunchFail

    invoke GetFileAttributesA, ADDR gamePath
    cmp eax, INVALID_FILE_ATTRIBUTES
    je WorkerGameMissing
    test eax, FILE_ATTRIBUTE_DIRECTORY
    jnz WorkerGameMissing

    invoke GetFileAttributesA, ADDR dllPath
    cmp eax, INVALID_FILE_ATTRIBUTES
    je WorkerDllMissing
    test eax, FILE_ATTRIBUTE_DIRECTORY
    jnz WorkerDllMissing

    lea edi, startupInfo
    xor eax, eax
    mov ecx, SIZEOF STARTUPINFOA / 4
    rep stosd

    lea edi, procInfo
    xor eax, eax
    mov ecx, SIZEOF PROCESS_INFORMATION / 4
    rep stosd

    mov startupInfo.cb, SIZEOF STARTUPINFOA

    invoke CreateProcessA, ADDR gamePath, NULL, NULL, NULL, FALSE, 0, NULL, ADDR gameDir, ADDR startupInfo, ADDR procInfo
    test eax, eax
    jz WorkerLaunchFail

    invoke CloseHandle, procInfo.hThread

    invoke PostMessageA, gMainWnd, WM_APP_PROGRESS, PROGRESS_WAITING, 0

    mov attemptsRemaining, INJECT_MAX_ATTEMPTS

WorkerInjectRetry:
    invoke InjectDll, procInfo.hProcess, ADDR dllPath
    test eax, eax
    jnz WorkerInjectSuccess

    dec attemptsRemaining
    jz WorkerInjectTimedOut

    invoke Sleep, INJECT_RETRY_INTERVAL_MS
    jmp WorkerInjectRetry

WorkerInjectSuccess:
    invoke CloseHandle, procInfo.hProcess
    invoke PostMessageA, gMainWnd, WM_APP_RESULT, RESULT_OK, 0
    xor eax, eax
    pop edi
    ret

WorkerInjectTimedOut:
    invoke CloseHandle, procInfo.hProcess
    invoke PostMessageA, gMainWnd, WM_APP_RESULT, RESULT_INJECT_FAILED, 0
    xor eax, eax
    pop edi
    ret

WorkerGameMissing:
    invoke PostMessageA, gMainWnd, WM_APP_RESULT, RESULT_GAME_MISSING, 0
    xor eax, eax
    pop edi
    ret

WorkerDllMissing:
    invoke PostMessageA, gMainWnd, WM_APP_RESULT, RESULT_DLL_MISSING, 0
    xor eax, eax
    pop edi
    ret

WorkerLaunchFail:
    invoke PostMessageA, gMainWnd, WM_APP_RESULT, RESULT_LAUNCH_FAILED, 0
    xor eax, eax
    pop edi
    ret
WorkerProc ENDP

END
