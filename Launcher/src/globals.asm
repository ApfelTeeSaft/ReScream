include common.inc

PUBLIC className, appTitle, buttonClassName, staticClassName, progressClassName
PUBLIC fontDisplayName, fontUiName, fontMonoName, fontVisualizerName
PUBLIC buttonText, buttonBusyText, buttonCheckingText, buttonUpdatingText
PUBLIC integrityText, integrityBusyText
PUBLIC statusReady, statusLaunching, statusWaiting, statusSuccess
PUBLIC statusGameMissing, statusDllMissing, statusLaunchFailed
PUBLIC statusInjectFailed, statusThreadFailed, errorCaption
IFDEF DEBUG_BUILD
PUBLIC statusInjectArchMismatch, statusInjectLoadLibraryNull, statusInjectRemoteSetup
ENDIF
PUBLIC updateErrorCaption, updateErrorNetwork, updateErrorManifest
PUBLIC updateErrorHash, updateErrorFile, updateErrorSelf, updateStatusChecking
PUBLIC updateStatusScanning, updateStatusDeleting, updateStatusComplete
PUBLIC updateStatusDownloadingPrefix
PUBLIC kernel32Name, loadLibraryName, setDllDirectoryName
PUBLIC gameSuffix, gameDirSuffix, dllSuffix
PUBLIC personalizeKey, appsUseLightThemeName
PUBLIC cdnBaseUrl, manifestUrl, manifestCacheSuffix, manifestTempSuffix
PUBLIC selfManifestPath, selfUpdateSuffix, updaterArgPrefix, userAgentName
PUBLIC gInstance, gMainWnd, gStartButton, gIntegrityButton, gProgressBar, gProgressText
PUBLIC gTitleFont, gSubtitleFont, gButtonFont, gMonoFont, gVisualizerFont
PUBLIC gStatusPtr, gBusy, gHover, gThemeMode, gVisualPhase, gVisualStartTick
PUBLIC gUpdateBusy, gFilesReady, gUpdateMode
IFDEF DEBUG_BUILD
PUBLIC gInjectStage, gInjectWin32Error, gInjectRemoteExit
ENDIF
PUBLIC gUpdateStatusText, gUpdateErrorText
PUBLIC gColorBg, gColorPanel, gColorPanelAlt, gColorBorder, gColorBorderSoft
PUBLIC gColorAccent, gColorAccentHot, gColorText, gColorTextDim
PUBLIC gColorButton, gColorButtonHover, gColorButtonDisabled
PUBLIC gColorButtonTextDisabled, gColorScanline
PUBLIC baseDir, gamePath, gameDir, dllPath, launcherPath
PUBLIC manifestCachePath, manifestTempPath, selfUpdatePath

.data
className               db "ScreamLauncherWindowClass",0
appTitle                db "Scream Launcher",0
buttonClassName          db "BUTTON",0
staticClassName          db "STATIC",0
progressClassName        db "msctls_progress32",0

fontDisplayName         db "Segoe UI Semibold",0
fontUiName              db "Segoe UI",0
fontMonoName            db "Consolas",0
fontVisualizerName      db "Arial Black",0

buttonText              db "START",0
buttonBusyText          db "STARTING...",0
buttonCheckingText      db "CHECKING FOR UPDATES...",0
buttonUpdatingText      db "UPDATING...",0
integrityText           db "VERIFY / REPAIR",0
integrityBusyText       db "VERIFYING...",0

statusReady             db "Ready - waiting for launch command.",0
statusLaunching         db "Launching game process...",0
statusWaiting           db "Game running - attempting to load client.dll...",0
statusSuccess           db "client.dll loaded successfully.",0
statusGameMissing       db "The game executable was not found.",13,10,13,10,"Expected: game\\scream_win32_public_dev.exe",0
statusDllMissing        db "The launcher DLL was not found.",13,10,13,10,"Expected: client.dll next to ScreamLauncher.exe",0
statusLaunchFailed      db "Windows could not start the game process.",13,10,13,10,"Check the game files and launcher permissions.",0
statusInjectFailed      db "The game started, but client.dll could not be loaded within 60 seconds.",0
IFDEF DEBUG_BUILD
statusInjectArchMismatch db "Injection stopped: launcher and target architecture differ.",0
statusInjectLoadLibraryNull db "Remote LoadLibraryA returned NULL while loading client.dll.",0
statusInjectRemoteSetup db "Remote DLL loader setup failed before LoadLibraryA completed.",0
ENDIF
statusThreadFailed      db "The launcher could not create its background worker thread.",0
errorCaption            db "Scream Launcher - Error",0

updateErrorCaption      db "Scream Launcher - Update Error",0
updateErrorNetwork      db "Could not contact the ReScream CDN or download the update manifest.",0
updateErrorManifest     db "The downloaded ReScream manifest is invalid or contains an unsafe path.",0
updateErrorHash         db "A downloaded file failed SHA-256 verification. The temporary file was discarded.",0
updateErrorFile         db "A game file could not be written, replaced, or removed.",0
updateErrorSelf         db "The launcher update could not be staged or restarted.",0
updateStatusChecking    db "Checking update manifest...",0
updateStatusScanning    db "Checking local files...",0
updateStatusDeleting    db "Removing obsolete files...",0
updateStatusComplete    db "Files are up to date.",0
updateStatusDownloadingPrefix db "Downloading: ",0

kernel32Name            db "kernel32.dll",0
loadLibraryName         db "LoadLibraryA",0
setDllDirectoryName     db "SetDllDirectoryA",0

gameSuffix              db "game\\scream_win32_public_dev.exe",0
gameDirSuffix           db "game",0
dllSuffix               db "client.dll",0

personalizeKey          db "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",0
appsUseLightThemeName   db "AppsUseLightTheme",0

cdnBaseUrl              db "https://cdn.apfelteesaft.com/",0
manifestUrl             db "https://cdn.apfelteesaft.com/rescream/rescream_manifest.txt",0
manifestCacheSuffix     db "game\\manifest.txt",0
manifestTempSuffix      db ".rescream_manifest.remote.tmp",0
selfManifestPath        db "rescream/ScreamLauncher.exe",0
selfUpdateSuffix        db ".update.exe",0
updaterArgPrefix        db "--apply-update=",0
userAgentName           db "ReScreamLauncher/1.0",0

gInstance               dd 0
gMainWnd                dd 0
gStartButton            dd 0
gIntegrityButton        dd 0
gProgressBar            dd 0
gProgressText           dd 0
gTitleFont              dd 0
gSubtitleFont           dd 0
gButtonFont             dd 0
gMonoFont               dd 0
gVisualizerFont         dd 0
gStatusPtr              dd OFFSET statusReady
gBusy                    dd 0
gHover                   dd 0
gThemeMode               dd THEME_LIGHT
gVisualPhase             dd 0
gVisualStartTick         dd 0
gUpdateBusy              dd 0
gFilesReady              dd 0
gUpdateMode              dd UPDATE_MODE_STARTUP
IFDEF DEBUG_BUILD
gInjectStage             dd INJECT_STAGE_NONE
gInjectWin32Error        dd 0
gInjectRemoteExit        dd 0
ENDIF

gUpdateStatusText       db PATH_CAP DUP(0)
gUpdateErrorText        db PATH_CAP DUP(0)

gColorBg                dd 00F0EDE9h
gColorPanel             dd 00F9F7F4h
gColorPanelAlt          dd 00E9E4DEh
gColorBorder            dd 008A8075h
gColorBorderSoft        dd 00C9C2BAh
gColorAccent            dd 00736D0Fh
gColorAccentHot         dd 00908710h
gColorText              dd 00262117h
gColorTextDim           dd 006B6453h
gColorButton            dd 00E8E7D7h
gColorButtonHover       dd 00DEDCC1h
gColorButtonDisabled    dd 00D5D2CDh
gColorButtonTextDisabled dd 00918B84h
gColorScanline          dd 00161616h

baseDir                 db PATH_CAP DUP(0)
gamePath                db PATH_CAP DUP(0)
gameDir                 db PATH_CAP DUP(0)
dllPath                 db PATH_CAP DUP(0)
launcherPath            db PATH_CAP DUP(0)
manifestCachePath       db (PATH_CAP+64) DUP(0)
manifestTempPath        db (PATH_CAP+64) DUP(0)
selfUpdatePath          db (PATH_CAP+64) DUP(0)

END
