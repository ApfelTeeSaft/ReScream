include common.inc
include globals.inc

PUBLIC RefreshTheme, ApplyTitleBarTheme

.code

DetectSystemTheme PROC
    LOCAL hKey:DWORD
    LOCAL valueType:DWORD
    LOCAL valueData:DWORD
    LOCAL valueSize:DWORD

    mov hKey, 0
    mov valueType, 0
    mov valueData, 1
    mov valueSize, 4

    invoke RegOpenKeyExA, HKEY_CURRENT_USER, ADDR personalizeKey, 0, KEY_QUERY_VALUE, ADDR hKey
    cmp eax, ERROR_SUCCESS
    jne DetectLight

    invoke RegQueryValueExA, hKey, ADDR appsUseLightThemeName, NULL, ADDR valueType, ADDR valueData, ADDR valueSize
    push eax
    invoke RegCloseKey, hKey
    pop eax

    cmp eax, ERROR_SUCCESS
    jne DetectLight
    cmp valueType, REG_DWORD
    jne DetectLight
    cmp valueData, 0
    jne DetectLight

    mov eax, THEME_DARK
    ret

DetectLight:
    mov eax, THEME_LIGHT
    ret
DetectSystemTheme ENDP

ApplyPalette PROC
    cmp gThemeMode, THEME_DARK
    je PaletteDark

PaletteLight:
    mov gColorBg,                 00F0EDE9h ; RGB(233,237,240)
    mov gColorPanel,              00F9F7F4h ; RGB(244,247,249)
    mov gColorPanelAlt,           00E9E4DEh ; RGB(222,228,233)
    mov gColorBorder,             008A8075h ; RGB(117,128,138)
    mov gColorBorderSoft,         00C9C2BAh ; RGB(186,194,201)
    mov gColorAccent,             00736D0Fh ; RGB(15,109,115)
    mov gColorAccentHot,          00908710h ; RGB(16,135,144)
    mov gColorText,               00262117h ; RGB(23,33,38)
    mov gColorTextDim,            006B6453h ; RGB(83,100,107)
    mov gColorButton,             00E8E7D7h ; RGB(215,231,232)
    mov gColorButtonHover,        00DEDCC1h ; RGB(193,220,222)
    mov gColorButtonDisabled,     00D5D2CDh ; RGB(205,210,213)
    mov gColorButtonTextDisabled, 00918B84h ; RGB(132,139,145)
    mov gColorScanline,           00161616h ; RGB(22,22,22)
    ret

PaletteDark:
    mov gColorBg,                 00100D0Bh ; RGB(11,13,16)
    mov gColorPanel,              001E1A15h ; RGB(21,26,30)
    mov gColorPanelAlt,           0026211Bh ; RGB(27,33,38)
    mov gColorBorder,             004B4130h ; RGB(48,65,75)
    mov gColorBorderSoft,         00312C27h ; RGB(39,44,49)
    mov gColorAccent,             00D7D650h ; RGB(80,214,215)
    mov gColorAccentHot,          00F0EE75h ; RGB(117,238,240)
    mov gColorText,               00F7F6EEh ; RGB(238,246,247)
    mov gColorTextDim,            00AAA395h ; RGB(149,163,170)
    mov gColorButton,             003C3A16h ; RGB(22,58,60)
    mov gColorButtonHover,        005B5720h ; RGB(32,87,91)
    mov gColorButtonDisabled,     00221F1Bh ; RGB(27,31,34)
    mov gColorButtonTextDisabled, 006F6860h ; RGB(96,104,111)
    mov gColorScanline,           00161616h ; RGB(22,22,22)
    ret
ApplyPalette ENDP

ApplyTitleBarTheme PROC STDCALL hWnd:DWORD
    LOCAL darkEnabled:DWORD

    mov darkEnabled, FALSE
    cmp gThemeMode, THEME_DARK
    jne ApplyCaption
    mov darkEnabled, TRUE

ApplyCaption:
    invoke DwmSetWindowAttribute, hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, ADDR darkEnabled, 4
    test eax, eax
    jz CaptionDone

    invoke DwmSetWindowAttribute, hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE_OLD, ADDR darkEnabled, 4

CaptionDone:
    ret
ApplyTitleBarTheme ENDP

RefreshTheme PROC STDCALL hWnd:DWORD
    call DetectSystemTheme
    mov gThemeMode, eax
    call ApplyPalette

    cmp hWnd, NULL
    je ThemeDone
    invoke ApplyTitleBarTheme, hWnd
    invoke InvalidateRect, hWnd, NULL, FALSE

ThemeDone:
    ret
RefreshTheme ENDP

END
