include common.inc
include globals.inc

PUBLIC ParseRemoteManifest, GetManifestEntryCount, GetManifestEntryPtr
PUBLIC ManifestContainsPath, ProcessDeletedManifestFiles

.data?
manifestEntries     db (MAX_MANIFEST_ENTRIES * ENTRY_SIZE) DUP(?)
manifestEntryCount  dd ?
deleteLocalPath     db PATH_CAP DUP(?)

.code

HexNibble PROC
    cmp al, '0'
    jb HexInvalid
    cmp al, '9'
    jbe HexDigit
    cmp al, 'a'
    jb HexUpperCheck
    cmp al, 'f'
    jbe HexLower
HexUpperCheck:
    cmp al, 'A'
    jb HexInvalid
    cmp al, 'F'
    ja HexInvalid
    sub al, 'A'
    add al, 10
    movzx eax, al
    ret
HexLower:
    sub al, 'a'
    add al, 10
    movzx eax, al
    ret
HexDigit:
    sub al, '0'
    movzx eax, al
    ret
HexInvalid:
    mov eax, 0FFFFFFFFh
    ret
HexNibble ENDP

ConvertHex64 PROC STDCALL pHex:DWORD, pOut:DWORD
    push esi
    push edi
    push ebx
    mov esi, pHex
    mov edi, pOut
    mov ebx, 32
ConvertHexLoop:
    mov al, BYTE PTR [esi]
    call HexNibble
    cmp eax, 0FFFFFFFFh
    je ConvertHexFail
    shl eax, 4
    mov edx, eax
    mov al, BYTE PTR [esi+1]
    call HexNibble
    cmp eax, 0FFFFFFFFh
    je ConvertHexFail
    or eax, edx
    mov BYTE PTR [edi], al
    add esi, 2
    inc edi
    dec ebx
    jnz ConvertHexLoop
    mov eax, TRUE
    jmp ConvertHexDone
ConvertHexFail:
    xor eax, eax
ConvertHexDone:
    pop ebx
    pop edi
    pop esi
    ret
ConvertHex64 ENDP

StringEquals PROC STDCALL pA:DWORD, pB:DWORD
    push esi
    push edi
    mov esi, pA
    mov edi, pB
StringEqualsLoop:
    mov al, BYTE PTR [esi]
    cmp al, BYTE PTR [edi]
    jne StringNotEqual
    test al, al
    jz StringEqual
    inc esi
    inc edi
    jmp StringEqualsLoop
StringEqual:
    mov eax, TRUE
    jmp StringEqualsDone
StringNotEqual:
    xor eax, eax
StringEqualsDone:
    pop edi
    pop esi
    ret
StringEquals ENDP

ValidateManifestPath PROC STDCALL pPath:DWORD
    push esi
    mov esi, pPath

    cmp BYTE PTR [esi+0], 'r'
    jne PathInvalid
    cmp BYTE PTR [esi+1], 'e'
    jne PathInvalid
    cmp BYTE PTR [esi+2], 's'
    jne PathInvalid
    cmp BYTE PTR [esi+3], 'c'
    jne PathInvalid
    cmp BYTE PTR [esi+4], 'r'
    jne PathInvalid
    cmp BYTE PTR [esi+5], 'e'
    jne PathInvalid
    cmp BYTE PTR [esi+6], 'a'
    jne PathInvalid
    cmp BYTE PTR [esi+7], 'm'
    jne PathInvalid
    cmp BYTE PTR [esi+8], '/'
    jne PathInvalid
    add esi, 9
    cmp BYTE PTR [esi], 0
    je PathInvalid
    cmp BYTE PTR [esi], '/'
    je PathInvalid

PathScan:
    mov al, BYTE PTR [esi]
    test al, al
    jz PathValid
    cmp al, 20h
    jb PathInvalid
    cmp al, 5Ch
    je PathInvalid
    cmp al, ':'
    je PathInvalid
    cmp al, '/'
    jne PathCheckDotDot
    cmp BYTE PTR [esi+1], '/'
    je PathInvalid
    inc esi
    jmp PathScan

PathCheckDotDot:
    cmp al, '.'
    jne PathAdvance
    cmp BYTE PTR [esi+1], '.'
    jne PathAdvance
    mov dl, BYTE PTR [esi-1]
    cmp dl, '/'
    jne PathAdvance
    mov dl, BYTE PTR [esi+2]
    cmp dl, 0
    je PathInvalid
    cmp dl, '/'
    je PathInvalid

PathAdvance:
    inc esi
    jmp PathScan

PathValid:
    mov eax, TRUE
    jmp PathValidateDone
PathInvalid:
    xor eax, eax
PathValidateDone:
    pop esi
    ret
ValidateManifestPath ENDP

ParseRemoteManifest PROC STDCALL pBuffer:DWORD, bufferSize:DWORD
    LOCAL pEnd:DWORD
    LOCAL pLine:DWORD
    LOCAL pLineEnd:DWORD
    LOCAL pEntry:DWORD
    LOCAL pPath:DWORD

    push esi
    push edi
    push ebx

    mov manifestEntryCount, 0
    mov esi, pBuffer
    mov eax, pBuffer
    add eax, bufferSize
    mov pEnd, eax

ParseNextLine:
    cmp esi, pEnd
    jae ParseSuccess

ParseSkipNewline:
    cmp esi, pEnd
    jae ParseSuccess
    mov al, BYTE PTR [esi]
    cmp al, 13
    je ParseSkipOne
    cmp al, 10
    je ParseSkipOne
    jmp ParseLineStart
ParseSkipOne:
    inc esi
    jmp ParseSkipNewline

ParseLineStart:
    mov pLine, esi
    cmp BYTE PTR [esi], '#'
    je ParseSkipLine
    cmp BYTE PTR [esi], 0
    je ParseSuccess

    mov edi, esi
FindLineEnd:
    cmp edi, pEnd
    jae LineEndFound
    mov al, BYTE PTR [edi]
    cmp al, 13
    je LineEndTerminate
    cmp al, 10
    je LineEndTerminate
    inc edi
    jmp FindLineEnd
LineEndTerminate:
    mov BYTE PTR [edi], 0
LineEndFound:
    mov pLineEnd, edi

    mov eax, edi
    sub eax, esi
    cmp eax, 66
    jb ParseFail
    cmp BYTE PTR [esi+64], 9
    jne ParseFail

    lea eax, [esi+65]
    mov pPath, eax
    invoke ValidateManifestPath, eax
    test eax, eax
    jz ParseFail
    invoke ManifestContainsPath, pPath
    test eax, eax
    jnz ParseFail

    mov eax, manifestEntryCount
    cmp eax, MAX_MANIFEST_ENTRIES
    jae ParseFail
    imul eax, ENTRY_SIZE
    add eax, OFFSET manifestEntries
    mov pEntry, eax

    invoke ConvertHex64, pLine, pEntry
    test eax, eax
    jz ParseFail

    mov edx, pEntry
    mov eax, pPath
    mov DWORD PTR [edx+ENTRY_PATH_OFF], eax
    mov DWORD PTR [edx+ENTRY_FLAGS_OFF], 0
    inc manifestEntryCount

    mov esi, pLineEnd
    inc esi
    jmp ParseNextLine

ParseSkipLine:
    mov edi, esi
ParseSkipLineLoop:
    cmp edi, pEnd
    jae ParseSuccess
    mov al, BYTE PTR [edi]
    cmp al, 13
    je ParseSkipLineDone
    cmp al, 10
    je ParseSkipLineDone
    inc edi
    jmp ParseSkipLineLoop
ParseSkipLineDone:
    mov esi, edi
    jmp ParseNextLine

ParseSuccess:
    cmp manifestEntryCount, 0
    je ParseFail
    mov eax, TRUE
    jmp ParseDone
ParseFail:
    mov manifestEntryCount, 0
    xor eax, eax
ParseDone:
    pop ebx
    pop edi
    pop esi
    ret
ParseRemoteManifest ENDP

GetManifestEntryCount PROC
    mov eax, manifestEntryCount
    ret
GetManifestEntryCount ENDP

GetManifestEntryPtr PROC STDCALL entryIndex:DWORD
    mov eax, entryIndex
    cmp eax, manifestEntryCount
    jae EntryPtrInvalid
    imul eax, ENTRY_SIZE
    add eax, OFFSET manifestEntries
    ret
EntryPtrInvalid:
    xor eax, eax
    ret
GetManifestEntryPtr ENDP

ManifestContainsPath PROC STDCALL pPath:DWORD
    LOCAL idx:DWORD
    LOCAL pEntry:DWORD
    mov idx, 0
ContainsLoop:
    mov eax, idx
    cmp eax, manifestEntryCount
    jae ContainsNo
    invoke GetManifestEntryPtr, eax
    mov pEntry, eax
    mov edx, eax
    mov eax, DWORD PTR [edx+ENTRY_PATH_OFF]
    invoke StringEquals, eax, pPath
    test eax, eax
    jnz ContainsYes
    inc idx
    jmp ContainsLoop
ContainsYes:
    mov eax, TRUE
    ret
ContainsNo:
    xor eax, eax
    ret
ManifestContainsPath ENDP

ProcessDeletedManifestFiles PROC STDCALL pOldBuffer:DWORD, oldSize:DWORD
    LOCAL pEnd:DWORD
    LOCAL pPath:DWORD
    LOCAL pLineEnd:DWORD

    push esi
    push edi

    mov esi, pOldBuffer
    mov eax, pOldBuffer
    add eax, oldSize
    mov pEnd, eax

OldNextLine:
    cmp esi, pEnd
    jae OldSuccess
OldSkipBreaks:
    cmp esi, pEnd
    jae OldSuccess
    mov al, BYTE PTR [esi]
    cmp al, 13
    je OldSkipOne
    cmp al, 10
    je OldSkipOne
    jmp OldLineStart
OldSkipOne:
    inc esi
    jmp OldSkipBreaks

OldLineStart:
    cmp BYTE PTR [esi], '#'
    je OldSkipLine
    mov edi, esi
OldFindEnd:
    cmp edi, pEnd
    jae OldEndFound
    mov al, BYTE PTR [edi]
    cmp al, 13
    je OldTerminate
    cmp al, 10
    je OldTerminate
    inc edi
    jmp OldFindEnd
OldTerminate:
    mov BYTE PTR [edi], 0
OldEndFound:
    mov pLineEnd, edi
    mov eax, edi
    sub eax, esi
    cmp eax, 66
    jb OldAdvance
    cmp BYTE PTR [esi+64], 9
    jne OldAdvance
    lea eax, [esi+65]
    mov pPath, eax
    invoke ValidateManifestPath, eax
    test eax, eax
    jz OldAdvance

    invoke StringEquals, pPath, ADDR selfManifestPath
    test eax, eax
    jnz OldAdvance
    invoke ManifestContainsPath, pPath
    test eax, eax
    jnz OldAdvance

    invoke BuildLocalPath, pPath, ADDR deleteLocalPath, PATH_CAP
    test eax, eax
    jz OldFail
    invoke DeleteFileA, ADDR deleteLocalPath
    test eax, eax
    jnz OldAdvance
    invoke GetFileAttributesA, ADDR deleteLocalPath
    cmp eax, INVALID_FILE_ATTRIBUTES
    jne OldFail

OldAdvance:
    mov esi, pLineEnd
    inc esi
    jmp OldNextLine

OldSkipLine:
    mov edi, esi
OldSkipLineLoop:
    cmp edi, pEnd
    jae OldSuccess
    mov al, BYTE PTR [edi]
    cmp al, 13
    je OldSkipLineDone
    cmp al, 10
    je OldSkipLineDone
    inc edi
    jmp OldSkipLineLoop
OldSkipLineDone:
    mov esi, edi
    jmp OldNextLine

OldSuccess:
    mov eax, TRUE
    jmp OldDone
OldFail:
    xor eax, eax
OldDone:
    pop edi
    pop esi
    ret
ProcessDeletedManifestFiles ENDP

END
