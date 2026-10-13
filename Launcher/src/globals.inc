EXTERN className:BYTE
EXTERN appTitle:BYTE
EXTERN buttonClassName:BYTE
EXTERN staticClassName:BYTE
EXTERN progressClassName:BYTE
EXTERN fontDisplayName:BYTE
EXTERN fontUiName:BYTE
EXTERN fontMonoName:BYTE
EXTERN fontVisualizerName:BYTE

EXTERN buttonText:BYTE
EXTERN buttonBusyText:BYTE
EXTERN buttonCheckingText:BYTE
EXTERN buttonUpdatingText:BYTE
EXTERN integrityText:BYTE
EXTERN integrityBusyText:BYTE
EXTERN statusReady:BYTE
EXTERN statusLaunching:BYTE
EXTERN statusWaiting:BYTE
EXTERN statusSuccess:BYTE
EXTERN statusGameMissing:BYTE
EXTERN statusDllMissing:BYTE
EXTERN statusLaunchFailed:BYTE
EXTERN statusInjectFailed:BYTE
IFDEF DEBUG_BUILD
EXTERN statusInjectArchMismatch:BYTE
EXTERN statusInjectLoadLibraryNull:BYTE
EXTERN statusInjectRemoteSetup:BYTE
ENDIF
EXTERN statusThreadFailed:BYTE
EXTERN errorCaption:BYTE
EXTERN updateErrorCaption:BYTE
EXTERN updateErrorNetwork:BYTE
EXTERN updateErrorManifest:BYTE
EXTERN updateErrorHash:BYTE
EXTERN updateErrorFile:BYTE
EXTERN updateErrorSelf:BYTE
EXTERN updateStatusChecking:BYTE
EXTERN updateStatusScanning:BYTE
EXTERN updateStatusDeleting:BYTE
EXTERN updateStatusComplete:BYTE
EXTERN updateStatusDownloadingPrefix:BYTE

EXTERN kernel32Name:BYTE
EXTERN loadLibraryName:BYTE
EXTERN setDllDirectoryName:BYTE
EXTERN gameSuffix:BYTE
EXTERN gameDirSuffix:BYTE
EXTERN dllSuffix:BYTE
EXTERN personalizeKey:BYTE
EXTERN appsUseLightThemeName:BYTE
EXTERN cdnBaseUrl:BYTE
EXTERN manifestUrl:BYTE
EXTERN manifestCacheSuffix:BYTE
EXTERN manifestTempSuffix:BYTE
EXTERN selfManifestPath:BYTE
EXTERN selfUpdateSuffix:BYTE
EXTERN updaterArgPrefix:BYTE
EXTERN userAgentName:BYTE

EXTERN gInstance:DWORD
EXTERN gMainWnd:DWORD
EXTERN gStartButton:DWORD
EXTERN gIntegrityButton:DWORD
EXTERN gProgressBar:DWORD
EXTERN gProgressText:DWORD
EXTERN gTitleFont:DWORD
EXTERN gSubtitleFont:DWORD
EXTERN gButtonFont:DWORD
EXTERN gMonoFont:DWORD
EXTERN gVisualizerFont:DWORD
EXTERN gStatusPtr:DWORD
EXTERN gBusy:DWORD
EXTERN gHover:DWORD
EXTERN gThemeMode:DWORD
EXTERN gVisualPhase:DWORD
EXTERN gVisualStartTick:DWORD
EXTERN gUpdateBusy:DWORD
EXTERN gFilesReady:DWORD
EXTERN gUpdateMode:DWORD
IFDEF DEBUG_BUILD
EXTERN gInjectStage:DWORD
EXTERN gInjectWin32Error:DWORD
EXTERN gInjectRemoteExit:DWORD
ENDIF
EXTERN gUpdateStatusText:BYTE
EXTERN gUpdateErrorText:BYTE

EXTERN gColorBg:DWORD
EXTERN gColorPanel:DWORD
EXTERN gColorPanelAlt:DWORD
EXTERN gColorBorder:DWORD
EXTERN gColorBorderSoft:DWORD
EXTERN gColorAccent:DWORD
EXTERN gColorAccentHot:DWORD
EXTERN gColorText:DWORD
EXTERN gColorTextDim:DWORD
EXTERN gColorButton:DWORD
EXTERN gColorButtonHover:DWORD
EXTERN gColorButtonDisabled:DWORD
EXTERN gColorButtonTextDisabled:DWORD
EXTERN gColorScanline:DWORD

EXTERN baseDir:BYTE
EXTERN gamePath:BYTE
EXTERN gameDir:BYTE
EXTERN dllPath:BYTE
EXTERN launcherPath:BYTE
EXTERN manifestCachePath:BYTE
EXTERN manifestTempPath:BYTE
EXTERN selfUpdatePath:BYTE

BuildPaths                  PROTO
BuildLocalPath              PROTO STDCALL :DWORD, :DWORD, :DWORD
EnsureParentDirectories     PROTO STDCALL :DWORD
HashFileSha256              PROTO STDCALL :DWORD, :DWORD
CompareHash32               PROTO STDCALL :DWORD, :DWORD
LoadFileToMemory            PROTO STDCALL :DWORD, :DWORD, :DWORD
FreeLoadedFile              PROTO STDCALL :DWORD
DownloadUrlToFile           PROTO STDCALL :DWORD, :DWORD, :DWORD
ParseRemoteManifest         PROTO STDCALL :DWORD, :DWORD
GetManifestEntryCount       PROTO
GetManifestEntryPtr         PROTO STDCALL :DWORD
ManifestContainsPath        PROTO STDCALL :DWORD
ProcessDeletedManifestFiles PROTO STDCALL :DWORD, :DWORD
UpdaterProc                 PROTO STDCALL :DWORD
StartUpdateCheck            PROTO STDCALL :DWORD, :DWORD
CheckCoreFilesReady         PROTO
TryRunApplyUpdateMode       PROTO
CleanupUpdateStub           PROTO
StageSelfUpdate             PROTO STDCALL :DWORD
InjectDll                   PROTO STDCALL :DWORD, :DWORD
WorkerProc                  PROTO STDCALL :DWORD
RefreshTheme                PROTO STDCALL :DWORD
ApplyTitleBarTheme          PROTO STDCALL :DWORD
InitializeUiResources       PROTO
DestroyUiResources          PROTO
PaintLauncher               PROTO STDCALL :DWORD, :DWORD
DrawScopeButton             PROTO STDCALL :DWORD
PositionLauncherControls    PROTO STDCALL :DWORD
DrawVisualizer              PROTO STDCALL :DWORD, :DWORD
DestroyVisualizerResources  PROTO
WndProc                     PROTO STDCALL :DWORD, :DWORD, :DWORD, :DWORD
