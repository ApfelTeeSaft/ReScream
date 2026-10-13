#pragma once

#if !defined(_WIN32)
#error client is a Windows DLL.
#endif

#if !defined(_M_IX86) && !defined(__i386__)
#error client must be compiled for Win32/x86.
#endif

static_assert(sizeof(void*) == 4, "client must be a 32-bit DLL");

#if defined(_MSC_VER)
#define CLIENT_CDECL __cdecl
#else
#define CLIENT_CDECL __attribute__((cdecl))
#endif

#if defined(CLIENT_EXPORTS)
#define CLIENT_API __declspec(dllexport)
#else
#define CLIENT_API __declspec(dllimport)
#endif
