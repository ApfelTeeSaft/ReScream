include common.inc
include globals.inc

PUBLIC DownloadUrlToFile

.data?
netReadBuffer db 65536 DUP(?)

.code

DownloadUrlToFile PROC STDCALL hSession:DWORD, pUrl:DWORD, pDest:DWORD
    LOCAL hUrl:DWORD
    LOCAL hFile:DWORD
    LOCAL statusCode:DWORD
    LOCAL querySize:DWORD
    LOCAL queryIndex:DWORD
    LOCAL contentLength:DWORD
    LOCAL bytesRead:DWORD
    LOCAL bytesWritten:DWORD
    LOCAL totalDownloaded:DWORD
    LOCAL writeOffset:DWORD
    LOCAL writeRemain:DWORD
    LOCAL lastPercent:DWORD
    LOCAL ok:DWORD
    LOCAL pWriteBuffer:DWORD

    mov hUrl, 0
    mov hFile, INVALID_HANDLE_VALUE
    mov contentLength, 0
    mov totalDownloaded, 0
    mov lastPercent, 0FFFFFFFFh
    mov ok, FALSE

    invoke PostMessageA, gMainWnd, WM_APP_UPDATE_PROGRESS, 0, 0

    invoke InternetOpenUrlA, hSession, pUrl, NULL, 0, INTERNET_FLAG_RELOAD OR INTERNET_FLAG_NO_CACHE_WRITE OR INTERNET_FLAG_SECURE OR INTERNET_FLAG_NO_UI, 0
    test eax, eax
    jz DownloadCleanup
    mov hUrl, eax

    mov statusCode, 0
    mov querySize, 4
    mov queryIndex, 0
    invoke HttpQueryInfoA, hUrl, HTTP_QUERY_STATUS_CODE OR HTTP_QUERY_FLAG_NUMBER, ADDR statusCode, ADDR querySize, ADDR queryIndex
    test eax, eax
    jz DownloadCleanup
    cmp statusCode, 200
    jne DownloadCleanup

    mov contentLength, 0
    mov querySize, 4
    mov queryIndex, 0
    invoke HttpQueryInfoA, hUrl, HTTP_QUERY_CONTENT_LENGTH OR HTTP_QUERY_FLAG_NUMBER, ADDR contentLength, ADDR querySize, ADDR queryIndex

    invoke CreateFileA, pDest, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je DownloadCleanup
    mov hFile, eax

DownloadReadLoop:
    invoke InternetReadFile, hUrl, ADDR netReadBuffer, SIZEOF netReadBuffer, ADDR bytesRead
    test eax, eax
    jz DownloadCleanup
    cmp bytesRead, 0
    je DownloadComplete

    mov writeOffset, 0
    mov eax, bytesRead
    mov writeRemain, eax

DownloadWriteLoop:
    cmp writeRemain, 0
    je DownloadChunkWritten
    mov eax, OFFSET netReadBuffer
    add eax, writeOffset
    mov pWriteBuffer, eax
    invoke WriteFile, hFile, pWriteBuffer, writeRemain, ADDR bytesWritten, NULL
    test eax, eax
    jz DownloadCleanup
    cmp bytesWritten, 0
    je DownloadCleanup
    mov eax, bytesWritten
    add writeOffset, eax
    sub writeRemain, eax
    jmp DownloadWriteLoop

DownloadChunkWritten:
    mov eax, bytesRead
    add totalDownloaded, eax

    cmp contentLength, 0
    je DownloadReadLoop
    mov eax, totalDownloaded
    cmp eax, contentLength
    jbe PercentCompute
    mov eax, contentLength
PercentCompute:
    mov ecx, 100
    mul ecx
    mov ecx, contentLength
    test ecx, ecx
    jz DownloadReadLoop
    div ecx
    cmp eax, 100
    jbe PercentClamped
    mov eax, 100
PercentClamped:
    cmp eax, lastPercent
    je DownloadReadLoop
    mov lastPercent, eax
    invoke PostMessageA, gMainWnd, WM_APP_UPDATE_PROGRESS, eax, 0
    jmp DownloadReadLoop

DownloadComplete:
    invoke PostMessageA, gMainWnd, WM_APP_UPDATE_PROGRESS, 100, 0
    mov ok, TRUE

DownloadCleanup:
    cmp hFile, INVALID_HANDLE_VALUE
    je DownloadNoFile
    invoke CloseHandle, hFile
DownloadNoFile:
    cmp hUrl, 0
    je DownloadNoUrl
    invoke InternetCloseHandle, hUrl
DownloadNoUrl:
    cmp ok, TRUE
    je DownloadReturn
    invoke DeleteFileA, pDest
DownloadReturn:
    mov eax, ok
    ret
DownloadUrlToFile ENDP

END
