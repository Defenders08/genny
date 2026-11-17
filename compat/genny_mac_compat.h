#pragma once
// Совместимость Windows-кода Genny с macOS.
// Подключается принудительно (флаг -include) ко всем исходникам порта.

#if defined(__APPLE__)

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <algorithm>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <string>
#include <utility>
#include <vector>

// --- базовые типы Win32: в коде используются как непрозрачные handle/счётчики ---
typedef void* HWND;
typedef void* HINSTANCE;
typedef void* HMODULE;
typedef void* HANDLE;
typedef void* HGLOBAL;
typedef void* HRSRC;
typedef void* LPVOID;
typedef void* HDC;
typedef void* HBITMAP;
struct GennyMenu;
typedef GennyMenu* HMENU;  // меню портированы на NSMenu (см. platform/win32_menu_shim)
typedef void* HBRUSH;
typedef void* HICON;
typedef void* HCURSOR;
typedef void* HFONT;
typedef void* HPEN;
typedef void* HGDIOBJ;
typedef void* HKEY;
typedef unsigned long COLORREF;
typedef long LRESULT;
typedef unsigned long WPARAM;
typedef long LPARAM;
typedef int (*FARPROC)();
typedef unsigned long DWORD;
typedef unsigned int UINT;
#if !defined(__OBJC__)
typedef int BOOL;  // в Objective-C++ BOOL уже объявлен в objc.h как signed char
#endif
typedef unsigned char BYTE;
typedef long long __int64;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

// --- MessageBox и INI-файлы: реализации в platform/win32_compat_shim.mm ---
#define MB_OK                 0x00000000
#define MB_ICONERROR          0x00000010
#define MB_ICONEXCLAMATION    0x00000030
#define MB_ICONWARNING        0x00000030
#define MB_ICONINFORMATION    0x00000040
#define IDOK                  1
#define IDCANCEL              2

extern "C" int gennyMessageBoxA(void* window, const char* text, const char* caption, unsigned int type);
extern "C" unsigned int gennyGetPrivateProfileString(const char* section, const char* key,
													const char* defaultValue, char* outBuffer,
													unsigned int outSize, const char* fileName);
extern "C" int gennyWideMkdir(const wchar_t* path);

#define MessageBoxA gennyMessageBoxA
#define GetPrivateProfileStringA gennyGetPrivateProfileString
#define _wmkdir gennyWideMkdir

// В MSVC long — 32 бита. Формат файлов Genny это учитывает (номера версий сравниваются
// по младшим 32 битам), поэтому на 64-битном long такие сравнения нужно усекать.
static inline long gennyLong32(long value)
{
	return (long)(int)(value);
}

#if BUILD_VST
// В FL-сборке эти значения приходят из fpsdk/fp_def.h
const int FromMIDI_Max = 1 << 30;
const float FromMIDI_Div = 1.0f / FromMIDI_Max;
inline int TranslateMidi(int value, int minValue, int maxValue)
{
	return minValue + (int)(value * FromMIDI_Div * (maxValue - minValue));
}
#endif

#ifdef __cplusplus
struct IStream;  // используется только как непрозрачный указатель (FL-сборка на Windows)
#endif

// windows.h подтягивает min/max в глобальное пространство имён — код на это рассчитывает
using std::max;
using std::min;

// --- ключевые слова MSVC ---
#define __stdcall
#define __cdecl
#define __fastcall
#ifndef __forceinline
#define __forceinline inline
#endif
#ifndef __declspec
#define __declspec(x)
#endif

// --- функции, которых нет в libc ---
static inline char* genny_itoa(int value, char* buffer, int radix)
{
	if (radix == 16)
		std::snprintf(buffer, 32, "%x", value);
	else if (radix == 8)
		std::snprintf(buffer, 32, "%o", value);
	else
		std::snprintf(buffer, 32, "%d", value);
	return buffer;
}

static inline char* genny_ltoa(long value, char* buffer, int radix)
{
	if (radix == 16)
		std::snprintf(buffer, 32, "%lx", value);
	else
		std::snprintf(buffer, 32, "%ld", value);
	return buffer;
}

#ifndef itoa
#define itoa genny_itoa
#endif
#ifndef ltoa
#define ltoa genny_ltoa
#endif
#ifndef _snprintf
#define _snprintf std::snprintf
#endif
#ifndef _vsnprintf
#define _vsnprintf std::vsnprintf
#endif
#ifndef _stricmp
#define _stricmp strcasecmp
#endif
#ifndef _strnicmp
#define _strnicmp strncasecmp
#endif
#ifndef _countof
#define _countof(a) (sizeof(a) / sizeof((a)[0]))
#endif

#endif // __APPLE__
