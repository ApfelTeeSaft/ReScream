include common.inc
include globals.inc

PUBLIC InjectDll

IFDEF DEBUG_BUILD
DIAG_STAGE MACRO value
    mov gInjectStage, value
ENDM
DIAG_ERROR MACRO
    invoke GetLastError
    mov gInjectWin32Error, eax
ENDM
DIAG_REMOTE MACRO value
    mov eax, value
    mov gInjectRemoteExit, eax
ENDM
ELSE
DIAG_STAGE MACRO value
ENDM
DIAG_ERROR MACRO
ENDM
DIAG_REMOTE MACRO value
ENDM
ENDIF

.code

CheckTargetArchitecture PROC STDCALL hProcess:DWORD
    LOCAL hSelf:DWORD
    LOCAL selfWow64:DWORD
    LOCAL targetWow64:DWORD

    mov selfWow64, 0
    mov targetWow64, 0

    invoke GetCurrentProcess
    mov hSelf, eax

    invoke IsWow64Process, hSelf, ADDR selfWow64
    test eax, eax
    jz ArchitectureUnknown

    invoke IsWow64Process, hProcess, ADDR targetWow64
    test eax, eax
    jz ArchitectureUnknown

    mov eax, selfWow64
    cmp eax, targetWow64
    jne ArchitectureMismatch

ArchitectureUnknown:
    mov eax, TRUE
    ret

ArchitectureMismatch:
    xor eax, eax
    ret
CheckTargetArchitecture ENDP

ResolveRemoteKernelProc PROC STDCALL hProcess:DWORD, pProcName:DWORD
    LOCAL hKernel32:DWORD
    LOCAL pLocalProc:DWORD
    LOCAL hLocalOwner:DWORD
    LOCAL pBaseName:DWORD
    LOCAL pModulePathStart:DWORD
    LOCAL processId:DWORD
    LOCAL hSnapshot:DWORD
    LOCAL modulePath[MAX_PATH]:BYTE
    LOCAL me:MODULEENTRY32A

    push esi
    push edi

    mov hSnapshot, INVALID_HANDLE_VALUE
    mov hLocalOwner, 0

    invoke GetModuleHandleA, ADDR kernel32Name
    test eax, eax
    jz ResolveFail
    mov hKernel32, eax

    invoke GetProcAddress, hKernel32, pProcName
    test eax, eax
    jz ResolveFail
    mov pLocalProc, eax

    invoke GetModuleHandleExA, GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS OR GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, pLocalProc, ADDR hLocalOwner
    test eax, eax
    jz ResolveFail

    invoke GetModuleFileNameA, hLocalOwner, ADDR modulePath, MAX_PATH
    test eax, eax
    jz ResolveFail
    cmp eax, MAX_PATH
    jae ResolveFail

    lea edx, modulePath
    mov pModulePathStart, edx
    mov pBaseName, edx
    mov esi, edx
    add esi, eax
ResolveFindBase:
    mov edx, pModulePathStart
    cmp esi, edx
    jbe ResolveBaseReady
    dec esi
    mov al, BYTE PTR [esi]
    cmp al, 5Ch
    je ResolveFoundSlash
    cmp al, '/'
    jne ResolveFindBase
ResolveFoundSlash:
    inc esi
    mov pBaseName, esi
ResolveBaseReady:

    invoke GetProcessId, hProcess
    test eax, eax
    jz ResolveFail
    mov processId, eax

    invoke CreateToolhelp32Snapshot, TH32CS_SNAPMODULE OR TH32CS_SNAPMODULE32, processId
    cmp eax, INVALID_HANDLE_VALUE
    je ResolveFail
    mov hSnapshot, eax

    lea edi, me
    mov ecx, SIZEOF MODULEENTRY32A
    xor eax, eax
    rep stosb
    mov me.dwSize, SIZEOF MODULEENTRY32A

    invoke Module32First, hSnapshot, ADDR me
    test eax, eax
    jz ResolveFailClose

ResolveModuleLoop:
    invoke lstrcmpiA, ADDR me.szModule, pBaseName
    test eax, eax
    jz ResolveModuleFound

    invoke Module32Next, hSnapshot, ADDR me
    test eax, eax
    jnz ResolveModuleLoop
    jmp ResolveFailClose

ResolveModuleFound:
    mov eax, pLocalProc
    sub eax, hLocalOwner
    add eax, me.modBaseAddr
    mov edi, eax

    invoke CloseHandle, hSnapshot
    mov eax, edi
    pop edi
    pop esi
    ret

ResolveFailClose:
    invoke CloseHandle, hSnapshot
ResolveFail:
    xor eax, eax
    pop edi
    pop esi
    ret
ResolveRemoteKernelProc ENDP

RunRemoteOneArg PROC STDCALL hProcess:DWORD, pRemoteProc:DWORD, remoteArg:DWORD, pThreadCreated:DWORD
    LOCAL hThread:DWORD
    LOCAL exitCode:DWORD

    mov hThread, 0
    mov exitCode, 0
    mov edx, pThreadCreated
    mov DWORD PTR [edx], 0

    invoke CreateRemoteThread, hProcess, NULL, 0, pRemoteProc, remoteArg, 0, NULL
    test eax, eax
    jz RemoteCallFail
    mov hThread, eax

    mov edx, pThreadCreated
    mov DWORD PTR [edx], TRUE

    invoke WaitForSingleObject, hThread, INFINITE
    cmp eax, WAIT_OBJECT_0
    jne RemoteCallCloseFail

    invoke GetExitCodeThread, hThread, ADDR exitCode
    test eax, eax
    jz RemoteCallCloseFail

    invoke CloseHandle, hThread
    mov eax, exitCode
    ret

RemoteCallCloseFail:
    invoke CloseHandle, hThread
RemoteCallFail:
    xor eax, eax
    ret
RunRemoteOneArg ENDP

InjectDll PROC STDCALL hProcess:DWORD, pDllPath:DWORD
    LOCAL remoteDllPath:DWORD
    LOCAL remoteBaseDir:DWORD
    LOCAL dllPathLength:DWORD
    LOCAL baseDirLength:DWORD
    LOCAL bytesWritten:DWORD
    LOCAL pRemoteLoadLibrary:DWORD
    LOCAL pRemoteSetDllDirectory:DWORD
    LOCAL remoteResult:DWORD
    LOCAL threadCreated:DWORD
    LOCAL dllDirWasSet:DWORD

    mov remoteDllPath, 0
    mov remoteBaseDir, 0
    mov dllDirWasSet, FALSE
IFDEF DEBUG_BUILD
    DIAG_STAGE INJECT_STAGE_NONE
    mov gInjectWin32Error, 0
    mov gInjectRemoteExit, 0
ENDIF

    invoke CheckTargetArchitecture, hProcess
    test eax, eax
    jnz ArchitectureOkay
    DIAG_STAGE INJECT_STAGE_ARCH_MISMATCH
    jmp InjectFail
ArchitectureOkay:

    invoke lstrlenA, pDllPath
    inc eax
    mov dllPathLength, eax

    invoke lstrlenA, ADDR baseDir
    inc eax
    mov baseDirLength, eax

    invoke VirtualAllocEx, hProcess, NULL, dllPathLength, MEM_COMMIT OR MEM_RESERVE, PAGE_READWRITE
    test eax, eax
    jnz AllocDllOkay
    DIAG_STAGE INJECT_STAGE_ALLOC_DLL_PATH
    DIAG_ERROR
    jmp InjectFail
AllocDllOkay:
    mov remoteDllPath, eax

    mov bytesWritten, 0
    invoke WriteProcessMemory, hProcess, remoteDllPath, pDllPath, dllPathLength, ADDR bytesWritten
    test eax, eax
    jnz WriteDllCheck
    DIAG_STAGE INJECT_STAGE_WRITE_DLL_PATH
    DIAG_ERROR
    jmp InjectCleanup
WriteDllCheck:
    mov eax, bytesWritten
    cmp eax, dllPathLength
    je WriteDllOkay
    DIAG_STAGE INJECT_STAGE_WRITE_DLL_PATH
    jmp InjectCleanup
WriteDllOkay:

    invoke VirtualAllocEx, hProcess, NULL, baseDirLength, MEM_COMMIT OR MEM_RESERVE, PAGE_READWRITE
    test eax, eax
    jnz AllocBaseOkay
    DIAG_STAGE INJECT_STAGE_ALLOC_BASE_DIR
    DIAG_ERROR
    jmp InjectCleanup
AllocBaseOkay:
    mov remoteBaseDir, eax

    mov bytesWritten, 0
    invoke WriteProcessMemory, hProcess, remoteBaseDir, ADDR baseDir, baseDirLength, ADDR bytesWritten
    test eax, eax
    jnz WriteBaseCheck
    DIAG_STAGE INJECT_STAGE_WRITE_BASE_DIR
    DIAG_ERROR
    jmp InjectCleanup
WriteBaseCheck:
    mov eax, bytesWritten
    cmp eax, baseDirLength
    je WriteBaseOkay
    DIAG_STAGE INJECT_STAGE_WRITE_BASE_DIR
    jmp InjectCleanup
WriteBaseOkay:

    invoke ResolveRemoteKernelProc, hProcess, ADDR setDllDirectoryName
    test eax, eax
    jnz ResolveDllDirOkay
    DIAG_STAGE INJECT_STAGE_RESOLVE_DLL_DIR
    jmp InjectCleanup
ResolveDllDirOkay:
    mov pRemoteSetDllDirectory, eax

    invoke ResolveRemoteKernelProc, hProcess, ADDR loadLibraryName
    test eax, eax
    jnz ResolveLoadLibraryOkay
    DIAG_STAGE INJECT_STAGE_RESOLVE_LOADLIB
    jmp InjectCleanup
ResolveLoadLibraryOkay:
    mov pRemoteLoadLibrary, eax

    mov threadCreated, 0
    invoke RunRemoteOneArg, hProcess, pRemoteSetDllDirectory, remoteBaseDir, ADDR threadCreated
    mov remoteResult, eax
    cmp threadCreated, 0
    jne SetDllDirThreadRan
    DIAG_STAGE INJECT_STAGE_SET_DLL_DIR_THREAD
    DIAG_ERROR
    jmp InjectCleanup
SetDllDirThreadRan:
    cmp remoteResult, 0
    jne SetDllDirOkay
    DIAG_STAGE INJECT_STAGE_SET_DLL_DIR_CALL
    jmp InjectCleanup
SetDllDirOkay:
    mov dllDirWasSet, TRUE

    mov threadCreated, 0
    invoke RunRemoteOneArg, hProcess, pRemoteLoadLibrary, remoteDllPath, ADDR threadCreated
    mov remoteResult, eax
    DIAG_REMOTE remoteResult
    cmp threadCreated, 0
    jne LoadLibraryThreadRan
    DIAG_STAGE INJECT_STAGE_LOADLIB_THREAD
    DIAG_ERROR
    jmp InjectCleanup
LoadLibraryThreadRan:
    cmp remoteResult, 0
    jne InjectSuccess
    DIAG_STAGE INJECT_STAGE_LOADLIB_CALL
    jmp InjectCleanup

InjectSuccess:
    cmp dllDirWasSet, FALSE
    je InjectSuccessCleanup
    mov threadCreated, 0
    invoke RunRemoteOneArg, hProcess, pRemoteSetDllDirectory, NULL, ADDR threadCreated
InjectSuccessCleanup:
    cmp remoteBaseDir, 0
    je InjectSuccessFreeDll
    invoke VirtualFreeEx, hProcess, remoteBaseDir, 0, MEM_RELEASE
InjectSuccessFreeDll:
    cmp remoteDllPath, 0
    je InjectSuccessDone
    invoke VirtualFreeEx, hProcess, remoteDllPath, 0, MEM_RELEASE
InjectSuccessDone:
    DIAG_STAGE INJECT_STAGE_NONE
    mov eax, TRUE
    ret

InjectCleanup:
    cmp dllDirWasSet, FALSE
    je InjectCleanupMemory
    cmp pRemoteSetDllDirectory, 0
    je InjectCleanupMemory
    mov threadCreated, 0
    invoke RunRemoteOneArg, hProcess, pRemoteSetDllDirectory, NULL, ADDR threadCreated

InjectCleanupMemory:
    cmp remoteBaseDir, 0
    je InjectCleanupDll
    invoke VirtualFreeEx, hProcess, remoteBaseDir, 0, MEM_RELEASE
InjectCleanupDll:
    cmp remoteDllPath, 0
    je InjectFail
    invoke VirtualFreeEx, hProcess, remoteDllPath, 0, MEM_RELEASE

InjectFail:
    xor eax, eax
    ret
InjectDll ENDP

END
