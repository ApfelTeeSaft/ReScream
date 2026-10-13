include common.inc
include globals.inc

PUBLIC TryRunApplyUpdateMode, CleanupUpdateStub, StageSelfUpdate

.data
cmdQuote            db '"',0
cmdApplyMiddle      db '" --apply-update=',0

.data?
selfPathBuffer      db PATH_CAP DUP(?)
selfTargetBuffer    db PATH_CAP DUP(?)
selfCmdLine         db (PATH_CAP+64) DUP(?)
pidTextBuffer       db 16 DUP(?)

.code

UIntToAscii PROC STDCALL value:DWORD, pOut:DWORD
    LOCAL temp[16]:BYTE
    LOCAL count:DWORD
    push esi
    push edi

    mov eax, value
    mov edi, pOut
    test eax, eax
    jnz UIntConvert
    mov BYTE PTR [edi], '0'
    mov BYTE PTR [edi+1], 0
    mov eax, TRUE
    jmp UIntDone

UIntConvert:
    lea esi, temp
    mov count, 0
UIntDigitLoop:
    xor edx, edx
    mov ecx, 10
    div ecx
    add dl, '0'
    mov BYTE PTR [esi], dl
    inc esi
    inc count
    test eax, eax
    jnz UIntDigitLoop

    dec esi
UIntReverseLoop:
    mov al, BYTE PTR [esi]
    mov BYTE PTR [edi], al
    inc edi
    dec esi
    dec count
    jnz UIntReverseLoop
    mov BYTE PTR [edi], 0
    mov eax, TRUE

UIntDone:
    pop edi
    pop esi
    ret
UIntToAscii ENDP

ParseApplyPid PROC
    LOCAL pCmd:DWORD
    push esi
    push edi

    invoke GetCommandLineA
    mov pCmd, eax
    mov esi, eax

FindArgLoop:
    mov al, BYTE PTR [esi]
    test al, al
    jz ArgNotFound
    cmp al, '-'
    jne ArgAdvance

    mov edi, OFFSET updaterArgPrefix
    mov edx, esi
CompareArgPrefix:
    mov al, BYTE PTR [edi]
    test al, al
    jz ArgPrefixFound
    cmp al, BYTE PTR [edx]
    jne ArgAdvance
    inc edi
    inc edx
    jmp CompareArgPrefix

ArgAdvance:
    inc esi
    jmp FindArgLoop

ArgPrefixFound:
    xor eax, eax
ParsePidDigits:
    mov cl, BYTE PTR [edx]
    cmp cl, '0'
    jb ParsePidDone
    cmp cl, '9'
    ja ParsePidDone
    imul eax, eax, 10
    movzx ecx, cl
    sub ecx, '0'
    add eax, ecx
    inc edx
    jmp ParsePidDigits
ParsePidDone:
    test eax, eax
    jz ArgNotFound
    jmp ParseArgDone

ArgNotFound:
    xor eax, eax
ParseArgDone:
    pop edi
    pop esi
    ret
ParseApplyPid ENDP

ApplySelfUpdate PROC STDCALL parentPid:DWORD
    LOCAL hParent:DWORD
    LOCAL startupInfo:STARTUPINFOA
    LOCAL procInfo:PROCESS_INFORMATION
    LOCAL pathLen:DWORD
    LOCAL retryCount:DWORD

    push edi

    invoke GetModuleFileNameA, NULL, ADDR selfPathBuffer, PATH_CAP
    test eax, eax
    jz ApplyFail
    mov pathLen, eax
    cmp eax, 12
    jb ApplyFail

    invoke lstrcpyA, ADDR selfTargetBuffer, ADDR selfPathBuffer
    mov eax, pathLen
    sub eax, 11
    lea edx, selfPathBuffer
    add edx, eax
    invoke lstrcmpA, edx, ADDR selfUpdateSuffix
    test eax, eax
    jnz ApplyFail

    mov eax, pathLen
    sub eax, 11
    mov BYTE PTR [selfTargetBuffer+eax], 0

    invoke OpenProcess, SYNCHRONIZE, FALSE, parentPid
    test eax, eax
    jz ParentWaitFallback
    mov hParent, eax
    invoke WaitForSingleObject, hParent, INFINITE
    invoke CloseHandle, hParent
    jmp ParentGone

ParentWaitFallback:
    invoke Sleep, 1000

ParentGone:
    mov retryCount, 40
CopyRetry:
    invoke CopyFileA, ADDR selfPathBuffer, ADDR selfTargetBuffer, FALSE
    test eax, eax
    jnz CopySucceeded
    invoke Sleep, 100
    dec retryCount
    jnz CopyRetry
    jmp ApplyFail

CopySucceeded:
    lea edi, startupInfo
    xor eax, eax
    mov ecx, SIZEOF STARTUPINFOA / 4
    rep stosd
    lea edi, procInfo
    xor eax, eax
    mov ecx, SIZEOF PROCESS_INFORMATION / 4
    rep stosd
    mov startupInfo.cb, SIZEOF STARTUPINFOA

    invoke CreateProcessA, ADDR selfTargetBuffer, NULL, NULL, NULL, FALSE, 0, NULL, NULL, ADDR startupInfo, ADDR procInfo
    test eax, eax
    jz ApplyFail
    invoke CloseHandle, procInfo.hThread
    invoke CloseHandle, procInfo.hProcess

    pop edi
    invoke ExitProcess, 0
    ret

ApplyFail:
    pop edi
    invoke ExitProcess, 1
    ret
ApplySelfUpdate ENDP

TryRunApplyUpdateMode PROC
    call ParseApplyPid
    test eax, eax
    jz NotApplyMode
    invoke ApplySelfUpdate, eax
NotApplyMode:
    xor eax, eax
    ret
TryRunApplyUpdateMode ENDP

CleanupUpdateStub PROC
    LOCAL retryCount:DWORD
    mov retryCount, 20
CleanupLoop:
    invoke DeleteFileA, ADDR selfUpdatePath
    test eax, eax
    jnz CleanupDone
    invoke GetFileAttributesA, ADDR selfUpdatePath
    cmp eax, INVALID_FILE_ATTRIBUTES
    je CleanupDone
    invoke Sleep, 50
    dec retryCount
    jnz CleanupLoop
CleanupDone:
    ret
CleanupUpdateStub ENDP

StageSelfUpdate PROC STDCALL pUpdateExe:DWORD
    LOCAL startupInfo:STARTUPINFOA
    LOCAL procInfo:PROCESS_INFORMATION

    push edi

    invoke GetCurrentProcessId
    invoke UIntToAscii, eax, ADDR pidTextBuffer

    invoke lstrcpyA, ADDR selfCmdLine, ADDR cmdQuote
    invoke lstrcatA, ADDR selfCmdLine, pUpdateExe
    invoke lstrcatA, ADDR selfCmdLine, ADDR cmdApplyMiddle
    invoke lstrcatA, ADDR selfCmdLine, ADDR pidTextBuffer

    lea edi, startupInfo
    xor eax, eax
    mov ecx, SIZEOF STARTUPINFOA / 4
    rep stosd
    lea edi, procInfo
    xor eax, eax
    mov ecx, SIZEOF PROCESS_INFORMATION / 4
    rep stosd
    mov startupInfo.cb, SIZEOF STARTUPINFOA

    invoke CreateProcessA, pUpdateExe, ADDR selfCmdLine, NULL, NULL, FALSE, 0, NULL, ADDR baseDir, ADDR startupInfo, ADDR procInfo
    test eax, eax
    jz StageFail
    invoke CloseHandle, procInfo.hThread
    invoke CloseHandle, procInfo.hProcess
    mov eax, TRUE
    jmp StageDone
StageFail:
    xor eax, eax
StageDone:
    pop edi
    ret
StageSelfUpdate ENDP

END
