include common.inc
include globals.inc

PUBLIC BuildPaths, BuildLocalPath, EnsureParentDirectories

.code

BuildPaths PROC
    push esi
    push edi
    push ecx

    invoke GetModuleFileNameA, NULL, ADDR launcherPath, PATH_CAP
    test eax, eax
    jz BuildPathsFail
    cmp eax, PATH_CAP-1
    jae BuildPathsFail

    invoke lstrcpyA, ADDR baseDir, ADDR launcherPath

    invoke lstrlenA, ADDR baseDir
    mov ecx, eax
    lea edi, baseDir
    add edi, eax

FindLastSlash:
    test ecx, ecx
    jz BuildPathsFail
    dec edi
    dec ecx
    cmp BYTE PTR [edi], 5Ch
    jne FindLastSlash

    inc edi
    mov BYTE PTR [edi], 0

    invoke lstrcpyA, ADDR gamePath, ADDR baseDir
    invoke lstrcatA, ADDR gamePath, ADDR gameSuffix

    invoke lstrcpyA, ADDR gameDir, ADDR baseDir
    invoke lstrcatA, ADDR gameDir, ADDR gameDirSuffix

    invoke lstrcpyA, ADDR dllPath, ADDR baseDir
    invoke lstrcatA, ADDR dllPath, ADDR dllSuffix

    invoke lstrcpyA, ADDR manifestCachePath, ADDR baseDir
    invoke lstrcatA, ADDR manifestCachePath, ADDR manifestCacheSuffix

    invoke lstrcpyA, ADDR manifestTempPath, ADDR baseDir
    invoke lstrcatA, ADDR manifestTempPath, ADDR manifestTempSuffix

    invoke lstrcpyA, ADDR selfUpdatePath, ADDR launcherPath
    invoke lstrcatA, ADDR selfUpdatePath, ADDR selfUpdateSuffix

    mov eax, TRUE
    jmp BuildPathsDone

BuildPathsFail:
    xor eax, eax

BuildPathsDone:
    pop ecx
    pop edi
    pop esi
    ret
BuildPaths ENDP

BuildLocalPath PROC STDCALL pManifestPath:DWORD, pOut:DWORD, outCap:DWORD
    LOCAL baseLen:DWORD
    LOCAL remain:DWORD

    push esi
    push edi

    invoke lstrlenA, ADDR baseDir
    mov baseLen, eax
    mov ecx, outCap
    cmp eax, ecx
    jae BuildLocalFail

    invoke lstrcpyA, pOut, ADDR baseDir

    mov esi, pManifestPath
    add esi, 9
    mov edi, pOut
    add edi, baseLen

    mov eax, outCap
    sub eax, baseLen
    mov remain, eax

BuildLocalLoop:
    cmp remain, 1
    jbe BuildLocalFail
    mov al, BYTE PTR [esi]
    test al, al
    jz BuildLocalFinish
    cmp al, '/'
    jne BuildLocalStore
    mov al, 5Ch
BuildLocalStore:
    mov BYTE PTR [edi], al
    inc esi
    inc edi
    dec remain
    jmp BuildLocalLoop

BuildLocalFinish:
    mov BYTE PTR [edi], 0
    mov eax, TRUE
    jmp BuildLocalDone

BuildLocalFail:
    mov edx, pOut
    test edx, edx
    jz BuildLocalFailNoZero
    mov BYTE PTR [edx], 0
BuildLocalFailNoZero:
    xor eax, eax

BuildLocalDone:
    pop edi
    pop esi
    ret
BuildLocalPath ENDP

EnsureParentDirectories PROC STDCALL pFullPath:DWORD
    LOCAL baseLen:DWORD

    push esi

    invoke lstrlenA, ADDR baseDir
    mov baseLen, eax

    mov esi, pFullPath
    add esi, eax

EnsureDirLoop:
    mov al, BYTE PTR [esi]
    test al, al
    jz EnsureDirDone
    cmp al, 5Ch
    jne EnsureDirNext

    mov BYTE PTR [esi], 0
    invoke CreateDirectoryA, pFullPath, NULL
    mov BYTE PTR [esi], 5Ch

EnsureDirNext:
    inc esi
    jmp EnsureDirLoop

EnsureDirDone:
    mov eax, TRUE
    pop esi
    ret
EnsureParentDirectories ENDP

END
