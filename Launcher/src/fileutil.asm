include common.inc
include globals.inc

PUBLIC LoadFileToMemory, FreeLoadedFile

.code

LoadFileToMemory PROC STDCALL pPath:DWORD, pOutPtr:DWORD, pOutSize:DWORD
    LOCAL hFile:DWORD
    LOCAL fileSize:DWORD
    LOCAL hHeap:DWORD
    LOCAL pBuffer:DWORD
    LOCAL totalRead:DWORD
    LOCAL bytesRead:DWORD
    LOCAL remain:DWORD

    mov edx, pOutPtr
    mov DWORD PTR [edx], 0
    mov edx, pOutSize
    mov DWORD PTR [edx], 0

    invoke CreateFileA, pPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je LoadFail
    mov hFile, eax

    invoke GetFileSize, hFile, NULL
    cmp eax, 0FFFFFFFFh
    je LoadCloseFail
    mov fileSize, eax

    invoke GetProcessHeap
    mov hHeap, eax
    mov ecx, fileSize
    inc ecx
    invoke HeapAlloc, hHeap, HEAP_ZERO_MEMORY, ecx
    test eax, eax
    jz LoadCloseFail
    mov pBuffer, eax
    mov totalRead, 0

ReadLoop:
    mov eax, totalRead
    cmp eax, fileSize
    jae ReadComplete
    mov ecx, fileSize
    sub ecx, eax
    mov remain, ecx
    mov edx, pBuffer
    add edx, eax
    invoke ReadFile, hFile, edx, remain, ADDR bytesRead, NULL
    test eax, eax
    jz LoadBufferFail
    cmp bytesRead, 0
    je LoadBufferFail
    mov eax, bytesRead
    add totalRead, eax
    jmp ReadLoop

ReadComplete:
    mov edx, pBuffer
    mov eax, fileSize
    mov BYTE PTR [edx+eax], 0
    invoke CloseHandle, hFile

    mov edx, pOutPtr
    mov eax, pBuffer
    mov DWORD PTR [edx], eax
    mov edx, pOutSize
    mov eax, fileSize
    mov DWORD PTR [edx], eax
    mov eax, TRUE
    ret

LoadBufferFail:
    invoke GetProcessHeap
    invoke HeapFree, eax, 0, pBuffer
LoadCloseFail:
    invoke CloseHandle, hFile
LoadFail:
    xor eax, eax
    ret
LoadFileToMemory ENDP

FreeLoadedFile PROC STDCALL pBuffer:DWORD
    cmp pBuffer, 0
    je FreeDone
    invoke GetProcessHeap
    invoke HeapFree, eax, 0, pBuffer
FreeDone:
    ret
FreeLoadedFile ENDP

END
