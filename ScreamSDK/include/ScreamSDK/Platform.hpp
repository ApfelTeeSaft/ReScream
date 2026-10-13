#pragma once

#if !defined(_WIN32)
#error ScreamSDK is a Windows DLL.
#endif

#if !defined(_M_IX86) && !defined(__i386__)
#error ScreamSDK must be compiled for Win32/x86.
#endif

static_assert(sizeof(void*) == 4, "ScreamSDK must be a 32-bit DLL");

#if defined(_MSC_VER)
#define SCREAM_CDECL __cdecl
#define SCREAM_STDCALL __stdcall
#define SCREAM_THISCALL __thiscall
#define SCREAM_FASTCALL __fastcall
#else
#define SCREAM_CDECL __attribute__((cdecl))
#define SCREAM_STDCALL __attribute__((stdcall))
#define SCREAM_THISCALL __attribute__((thiscall))
#define SCREAM_FASTCALL __attribute__((fastcall))
#endif

#if defined(SCREAMSDK_EXPORTS)
#define SCREAM_API __declspec(dllexport)
#elif defined(SCREAMSDK_IMPORTS)
#define SCREAM_API __declspec(dllimport)
#else
#define SCREAM_API
#endif
