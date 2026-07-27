#pragma once

#include <Windows.h> // Must be included before DbgHelp.h (Required for Win32 types)
#include <DbgHelp.h>
#include <stdio.h>

#pragma comment(lib, "Dbghelp.lib")

/* Prefix every diagnostic with the calling function via __FUNCTION__ (never drifts on rename).
 * %hs prints the narrow __FUNCTION__ inside a wide (f)wprintf. */
#define LOG_INFO(fmt, ...)  wprintf(L"%hs(): " fmt L"\n", __FUNCTION__, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) fwprintf(stderr, L"%hs(): " fmt L"\n", __FUNCTION__, ##__VA_ARGS__)

__declspec(noreturn) void HandleErrorAndFailImpl(LPCSTR function, LPCWSTR message, DWORD errorCode, BOOL isCrtError)
{
    if (isCrtError)
    {
        WCHAR crtMessage[256] = { 0, };
        if (_wcserror_s(crtMessage, ARRAYSIZE(crtMessage), (int)errorCode) == 0)
        {
            fwprintf(stderr, L"%hs(): %ls: %lu - %ls\n", function, message, errorCode, crtMessage);
        }
        else
        {
            fwprintf(stderr, L"%hs(): %ls: %lu\n", function, message, errorCode);
        }
    }
    else
    {
        LPWSTR sysMessage = NULL;

        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&sysMessage, 0, NULL);

        if (sysMessage == NULL)
        {
            fwprintf(stderr, L"%hs(): %ls: %lu\n", function, message, errorCode);
        }
        else
        {
            fwprintf(stderr, L"%hs(): %ls: %lu - %ls", function, message, errorCode, sysMessage);
            LocalFree(sysMessage);
        }
    }

#if defined(_DEBUG)
    __debugbreak();
#else
    HANDLE dumpFile = CreateFileW(L"CrashDump.dmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (dumpFile != INVALID_HANDLE_VALUE)
    {
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dumpFile, MiniDumpNormal, NULL, NULL, NULL);
        CloseHandle(dumpFile);
    }
#endif

    ExitProcess(errorCode);
}

/* Each call site injects its own __FUNCTION__ so the fatal message shows where it failed. */
#define HandleErrorAndFail(message, errorCode, isCrtError) \
    HandleErrorAndFailImpl(__FUNCTION__, (message), (errorCode), (isCrtError))