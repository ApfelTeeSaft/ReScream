include common.inc
include globals.inc

PUBLIC HashFileSha256, CompareHash32

.data?
hashReadBuffer db 65536 DUP(?)

.code

HashFileSha256 PROC STDCALL pPath:DWORD, pOutHash:DWORD
    LOCAL hFile:DWORD
    LOCAL hProv:DWORD
    LOCAL hHash:DWORD
    LOCAL bytesRead:DWORD
    LOCAL hashLen:DWORD
    LOCAL ok:DWORD

    mov hProv, 0
    mov hHash, 0
    mov hFile, INVALID_HANDLE_VALUE
    mov ok, FALSE

    invoke CreateFileA, pPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je HashCleanup
    mov hFile, eax

    invoke CryptAcquireContextA, ADDR hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT
    test eax, eax
    jz HashCleanup

    invoke CryptCreateHash, hProv, CALG_SHA_256, 0, 0, ADDR hHash
    test eax, eax
    jz HashCleanup

HashReadLoop:
    invoke ReadFile, hFile, ADDR hashReadBuffer, SIZEOF hashReadBuffer, ADDR bytesRead, NULL
    test eax, eax
    jz HashCleanup
    cmp bytesRead, 0
    je HashFinish

    invoke CryptHashData, hHash, ADDR hashReadBuffer, bytesRead, 0
    test eax, eax
    jz HashCleanup
    jmp HashReadLoop

HashFinish:
    mov hashLen, 32
    invoke CryptGetHashParam, hHash, HP_HASHVAL, pOutHash, ADDR hashLen, 0
    test eax, eax
    jz HashCleanup
    cmp hashLen, 32
    jne HashCleanup
    mov ok, TRUE

HashCleanup:
    cmp hHash, 0
    je HashNoHash
    invoke CryptDestroyHash, hHash
HashNoHash:
    cmp hProv, 0
    je HashNoProv
    invoke CryptReleaseContext, hProv, 0
HashNoProv:
    cmp hFile, INVALID_HANDLE_VALUE
    je HashReturn
    invoke CloseHandle, hFile
HashReturn:
    mov eax, ok
    ret
HashFileSha256 ENDP

CompareHash32 PROC STDCALL pA:DWORD, pB:DWORD
    push esi
    push edi
    push ecx
    mov esi, pA
    mov edi, pB
    mov ecx, 32
CompareLoop:
    mov al, BYTE PTR [esi]
    cmp al, BYTE PTR [edi]
    jne CompareDifferent
    inc esi
    inc edi
    dec ecx
    jnz CompareLoop
    mov eax, TRUE
    jmp CompareDone
CompareDifferent:
    xor eax, eax
CompareDone:
    pop ecx
    pop edi
    pop esi
    ret
CompareHash32 ENDP

END
