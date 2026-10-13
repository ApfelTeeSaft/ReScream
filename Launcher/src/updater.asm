include common.inc
include globals.inc

PUBLIC UpdaterProc, StartUpdateCheck, CheckCoreFilesReady

.data
downloadSuffix db ".download",0

.data?
updateLocalPath    db (PATH_CAP+32) DUP(?)
updateTempPath     db (PATH_CAP+48) DUP(?)
updateUrl          db (PATH_CAP+128) DUP(?)
updateActualHash   db 32 DUP(?)
updateOldPtr       dd ?
updateOldSize      dd ?
updateRemotePtr    dd ?
updateRemoteSize   dd ?

.code

SetUpdateStatus PROC STDCALL pText:DWORD
    invoke lstrcpynA, ADDR gUpdateStatusText, pText, PATH_CAP
    invoke PostMessageA, gMainWnd, WM_APP_UPDATE_STATUS, 0, 0
    ret
SetUpdateStatus ENDP

SetDownloadStatus PROC STDCALL pPath:DWORD
    LOCAL prefixLen:DWORD
    invoke lstrcpynA, ADDR gUpdateStatusText, ADDR updateStatusDownloadingPrefix, PATH_CAP
    invoke lstrlenA, ADDR gUpdateStatusText
    mov prefixLen, eax
    mov ecx, PATH_CAP
    sub ecx, eax
    lea edx, gUpdateStatusText
    add edx, eax
    invoke lstrcpynA, edx, pPath, ecx
    invoke PostMessageA, gMainWnd, WM_APP_UPDATE_STATUS, 0, 0
    ret
SetDownloadStatus ENDP

SetUpdateError PROC STDCALL pText:DWORD
    invoke lstrcpynA, ADDR gUpdateErrorText, pText, PATH_CAP
    ret
SetUpdateError ENDP

BuildDownloadUrl PROC STDCALL pManifestPath:DWORD
    invoke lstrcpyA, ADDR updateUrl, ADDR cdnBaseUrl
    invoke lstrcatA, ADDR updateUrl, pManifestPath
    mov eax, OFFSET updateUrl
    ret
BuildDownloadUrl ENDP

CheckCoreFilesReady PROC
    call BuildPaths
    test eax, eax
    jz CoreNotReady
    invoke GetFileAttributesA, ADDR gamePath
    cmp eax, INVALID_FILE_ATTRIBUTES
    je CoreNotReady
    test eax, FILE_ATTRIBUTE_DIRECTORY
    jnz CoreNotReady
    invoke GetFileAttributesA, ADDR dllPath
    cmp eax, INVALID_FILE_ATTRIBUTES
    je CoreNotReady
    test eax, FILE_ATTRIBUTE_DIRECTORY
    jnz CoreNotReady
    mov eax, TRUE
    ret
CoreNotReady:
    xor eax, eax
    ret
CheckCoreFilesReady ENDP

StartUpdateCheck PROC STDCALL hWnd:DWORD, updateMode:DWORD
    LOCAL hThread:DWORD

    cmp gUpdateBusy, 0
    jne StartUpdateDone
    cmp gBusy, 0
    jne StartUpdateDone

    mov gUpdateBusy, TRUE
    mov eax, updateMode
    mov gUpdateMode, eax

    cmp gStartButton, 0
    je StartUpdateNoStart
    invoke EnableWindow, gStartButton, FALSE
    cmp updateMode, UPDATE_MODE_VERIFY
    jne StartUpdateCheckingText
    invoke SetWindowTextA, gStartButton, ADDR buttonCheckingText
    jmp StartUpdateNoStart
StartUpdateCheckingText:
    invoke SetWindowTextA, gStartButton, ADDR buttonCheckingText
StartUpdateNoStart:

    cmp gIntegrityButton, 0
    je StartUpdateNoIntegrity
    invoke EnableWindow, gIntegrityButton, FALSE
    cmp updateMode, UPDATE_MODE_VERIFY
    jne StartUpdateIntegrityDefault
    invoke SetWindowTextA, gIntegrityButton, ADDR integrityBusyText
    jmp StartUpdateNoIntegrity
StartUpdateIntegrityDefault:
    invoke SetWindowTextA, gIntegrityButton, ADDR integrityText
StartUpdateNoIntegrity:

    cmp gProgressText, 0
    je StartUpdateNoText
    invoke SetWindowTextA, gProgressText, ADDR updateStatusChecking
    invoke ShowWindow, gProgressText, SW_SHOW
StartUpdateNoText:
    cmp gProgressBar, 0
    je StartUpdateNoProgress
    invoke SendMessageA, gProgressBar, PBM_SETPOS, 0, 0
    invoke ShowWindow, gProgressBar, SW_SHOW
StartUpdateNoProgress:

    invoke CreateThread, NULL, 0, OFFSET UpdaterProc, updateMode, 0, NULL
    test eax, eax
    jz StartUpdateThreadFail
    mov hThread, eax
    invoke CloseHandle, hThread
    jmp StartUpdateDone

StartUpdateThreadFail:
    mov gUpdateBusy, FALSE
    invoke SetUpdateError, ADDR statusThreadFailed
    invoke PostMessageA, hWnd, WM_APP_UPDATE_RESULT, UPDATE_ERR_FILE, 0

StartUpdateDone:
    ret
StartUpdateCheck ENDP

CheckAndStageSelf PROC STDCALL hInternet:DWORD
    LOCAL idx:DWORD
    LOCAL count:DWORD
    LOCAL pEntry:DWORD
    LOCAL pPath:DWORD

    call GetManifestEntryCount
    mov count, eax
    mov idx, 0
SelfFindLoop:
    mov eax, idx
    cmp eax, count
    jae SelfCurrent
    invoke GetManifestEntryPtr, eax
    mov pEntry, eax
    mov edx, eax
    mov eax, DWORD PTR [edx+ENTRY_PATH_OFF]
    mov pPath, eax
    invoke lstrcmpA, eax, ADDR selfManifestPath
    test eax, eax
    jz SelfEntryFound
    inc idx
    jmp SelfFindLoop

SelfEntryFound:
    invoke HashFileSha256, ADDR launcherPath, ADDR updateActualHash
    test eax, eax
    jz SelfNeedsUpdate
    mov edx, pEntry
    invoke CompareHash32, ADDR updateActualHash, edx
    test eax, eax
    jnz SelfCurrent

SelfNeedsUpdate:
    invoke SetDownloadStatus, pPath
    invoke BuildDownloadUrl, pPath
    invoke DeleteFileA, ADDR selfUpdatePath
    invoke DownloadUrlToFile, hInternet, ADDR updateUrl, ADDR selfUpdatePath
    test eax, eax
    jz SelfNetworkFail

    invoke HashFileSha256, ADDR selfUpdatePath, ADDR updateActualHash
    test eax, eax
    jz SelfHashFail
    mov edx, pEntry
    invoke CompareHash32, ADDR updateActualHash, edx
    test eax, eax
    jz SelfHashFail

    invoke StageSelfUpdate, ADDR selfUpdatePath
    test eax, eax
    jz SelfStageFail
    mov eax, UPDATE_SELF_RESTARTING
    ret

SelfNetworkFail:
    invoke SetUpdateError, ADDR updateErrorNetwork
    mov eax, UPDATE_ERR_NETWORK
    ret
SelfHashFail:
    invoke DeleteFileA, ADDR selfUpdatePath
    invoke SetUpdateError, ADDR updateErrorHash
    mov eax, UPDATE_ERR_HASH
    ret
SelfStageFail:
    invoke DeleteFileA, ADDR selfUpdatePath
    invoke SetUpdateError, ADDR updateErrorSelf
    mov eax, UPDATE_ERR_SELF
    ret

SelfCurrent:
    mov eax, UPDATE_OK
    ret
CheckAndStageSelf ENDP

ScanLocalFiles PROC
    LOCAL idx:DWORD
    LOCAL count:DWORD
    LOCAL pEntry:DWORD
    LOCAL pPath:DWORD

    invoke SetUpdateStatus, ADDR updateStatusScanning
    call GetManifestEntryCount
    mov count, eax
    mov idx, 0
ScanLoop:
    mov eax, idx
    cmp eax, count
    jae ScanSuccess
    invoke GetManifestEntryPtr, eax
    mov pEntry, eax
    mov edx, eax
    mov eax, DWORD PTR [edx+ENTRY_PATH_OFF]
    mov pPath, eax
    invoke lstrcmpA, pPath, ADDR selfManifestPath
    test eax, eax
    jz ScanNext

    invoke BuildLocalPath, pPath, ADDR updateLocalPath, SIZEOF updateLocalPath
    test eax, eax
    jz ScanFail

    invoke HashFileSha256, ADDR updateLocalPath, ADDR updateActualHash
    test eax, eax
    jz MarkDownload
    mov edx, pEntry
    invoke CompareHash32, ADDR updateActualHash, edx
    test eax, eax
    jnz ScanNext

MarkDownload:
    mov edx, pEntry
    or DWORD PTR [edx+ENTRY_FLAGS_OFF], ENTRY_FLAG_DOWNLOAD

ScanNext:
    inc idx
    jmp ScanLoop

ScanSuccess:
    mov eax, TRUE
    ret
ScanFail:
    xor eax, eax
    ret
ScanLocalFiles ENDP

DownloadMarkedFiles PROC STDCALL hInternet:DWORD
    LOCAL idx:DWORD
    LOCAL count:DWORD
    LOCAL pEntry:DWORD
    LOCAL pPath:DWORD

    call GetManifestEntryCount
    mov count, eax
    mov idx, 0
DownloadMarkedLoop:
    mov eax, idx
    cmp eax, count
    jae DownloadMarkedSuccess
    invoke GetManifestEntryPtr, eax
    mov pEntry, eax
    mov edx, eax
    test DWORD PTR [edx+ENTRY_FLAGS_OFF], ENTRY_FLAG_DOWNLOAD
    jz DownloadMarkedNext
    mov eax, DWORD PTR [edx+ENTRY_PATH_OFF]
    mov pPath, eax

    invoke BuildLocalPath, pPath, ADDR updateLocalPath, SIZEOF updateLocalPath
    test eax, eax
    jz DownloadMarkedFileFail
    invoke EnsureParentDirectories, ADDR updateLocalPath

    invoke lstrcpynA, ADDR updateTempPath, ADDR updateLocalPath, SIZEOF updateTempPath
    invoke lstrcatA, ADDR updateTempPath, ADDR downloadSuffix
    invoke DeleteFileA, ADDR updateTempPath

    invoke SetDownloadStatus, pPath
    invoke BuildDownloadUrl, pPath
    invoke DownloadUrlToFile, hInternet, ADDR updateUrl, ADDR updateTempPath
    test eax, eax
    jz DownloadMarkedNetworkFail

    invoke HashFileSha256, ADDR updateTempPath, ADDR updateActualHash
    test eax, eax
    jz DownloadMarkedHashFail
    mov edx, pEntry
    invoke CompareHash32, ADDR updateActualHash, edx
    test eax, eax
    jz DownloadMarkedHashFail

    invoke MoveFileExA, ADDR updateTempPath, ADDR updateLocalPath, MOVEFILE_REPLACE_EXISTING OR MOVEFILE_WRITE_THROUGH
    test eax, eax
    jz DownloadMarkedFileFail

DownloadMarkedNext:
    inc idx
    jmp DownloadMarkedLoop

DownloadMarkedNetworkFail:
    invoke DeleteFileA, ADDR updateTempPath
    invoke SetUpdateError, ADDR updateErrorNetwork
    mov eax, UPDATE_ERR_NETWORK
    ret
DownloadMarkedHashFail:
    invoke DeleteFileA, ADDR updateTempPath
    invoke SetUpdateError, ADDR updateErrorHash
    mov eax, UPDATE_ERR_HASH
    ret
DownloadMarkedFileFail:
    invoke DeleteFileA, ADDR updateTempPath
    invoke SetUpdateError, ADDR updateErrorFile
    mov eax, UPDATE_ERR_FILE
    ret
DownloadMarkedSuccess:
    mov eax, UPDATE_OK
    ret
DownloadMarkedFiles ENDP

UpdaterProc PROC STDCALL updateMode:DWORD
    LOCAL hInternet:DWORD
    LOCAL resultCode:DWORD

    mov eax, updateMode
    mov gUpdateMode, eax
    mov hInternet, 0
    mov updateRemotePtr, 0
    mov updateRemoteSize, 0
    mov updateOldPtr, 0
    mov updateOldSize, 0

    call BuildPaths
    test eax, eax
    jz UpdateFileFail

    invoke SetUpdateStatus, ADDR updateStatusChecking
    invoke InternetOpenA, ADDR userAgentName, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0
    test eax, eax
    jz UpdateNetworkFail
    mov hInternet, eax

    invoke DeleteFileA, ADDR manifestTempPath
    invoke DownloadUrlToFile, hInternet, ADDR manifestUrl, ADDR manifestTempPath
    test eax, eax
    jz UpdateNetworkFail

    invoke LoadFileToMemory, ADDR manifestTempPath, ADDR updateRemotePtr, ADDR updateRemoteSize
    test eax, eax
    jz UpdateManifestFail
    invoke ParseRemoteManifest, updateRemotePtr, updateRemoteSize
    test eax, eax
    jz UpdateManifestFail

    invoke CheckAndStageSelf, hInternet
    mov resultCode, eax
    cmp eax, UPDATE_SELF_RESTARTING
    je UpdateSelfRestart
    cmp eax, UPDATE_OK
    jne UpdateFinishError

    call ScanLocalFiles
    test eax, eax
    jz UpdateManifestFail

    invoke DownloadMarkedFiles, hInternet
    mov resultCode, eax
    cmp eax, UPDATE_OK
    jne UpdateFinishError

    invoke GetFileAttributesA, ADDR manifestCachePath
    cmp eax, INVALID_FILE_ATTRIBUTES
    je NoOldManifest
    invoke SetUpdateStatus, ADDR updateStatusDeleting
    invoke LoadFileToMemory, ADDR manifestCachePath, ADDR updateOldPtr, ADDR updateOldSize
    test eax, eax
    jz UpdateFileFail
    invoke ProcessDeletedManifestFiles, updateOldPtr, updateOldSize
    test eax, eax
    jz UpdateFileFail
NoOldManifest:

    invoke EnsureParentDirectories, ADDR manifestCachePath
    invoke MoveFileExA, ADDR manifestTempPath, ADDR manifestCachePath, MOVEFILE_REPLACE_EXISTING OR MOVEFILE_WRITE_THROUGH
    test eax, eax
    jz UpdateFileFail

    invoke SetUpdateStatus, ADDR updateStatusComplete
    mov resultCode, UPDATE_OK
    jmp UpdateFinish

UpdateNetworkFail:
    invoke SetUpdateError, ADDR updateErrorNetwork
    mov resultCode, UPDATE_ERR_NETWORK
    jmp UpdateFinishError
UpdateManifestFail:
    invoke SetUpdateError, ADDR updateErrorManifest
    mov resultCode, UPDATE_ERR_MANIFEST
    jmp UpdateFinishError
UpdateFileFail:
    invoke SetUpdateError, ADDR updateErrorFile
    mov resultCode, UPDATE_ERR_FILE
    jmp UpdateFinishError

UpdateFinishError:
    invoke DeleteFileA, ADDR manifestTempPath
    jmp UpdateFinish

UpdateSelfRestart:
    invoke PostMessageA, gMainWnd, WM_APP_SELF_UPDATE, 0, 0
    jmp UpdateCleanupOnly

UpdateFinish:
    invoke PostMessageA, gMainWnd, WM_APP_UPDATE_RESULT, resultCode, 0

UpdateCleanupOnly:
    cmp updateOldPtr, 0
    je UpdateNoOldFree
    invoke FreeLoadedFile, updateOldPtr
    mov updateOldPtr, 0
UpdateNoOldFree:
    cmp updateRemotePtr, 0
    je UpdateNoRemoteFree
    invoke FreeLoadedFile, updateRemotePtr
    mov updateRemotePtr, 0
UpdateNoRemoteFree:
    cmp hInternet, 0
    je UpdateDone
    invoke InternetCloseHandle, hInternet
UpdateDone:
    xor eax, eax
    ret
UpdaterProc ENDP

END
