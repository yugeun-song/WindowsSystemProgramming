#pragma once

#include <Windows.h> // Must be included before DbgHelp.h (Required for Win32 types)
#include <DbgHelp.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "Dbghelp.lib")

[[noreturn]] inline void HandleErrorAndFailW(LPCWSTR pwszMessage, DWORD dwErrorCode, BOOL bIsCrtError)
{
    if (bIsCrtError)
    {
        wchar_t wszCrtMsg[256];
        if (_wcserror_s(wszCrtMsg, 256, (int)dwErrorCode) == 0)
        {
            fwprintf(stderr, L"%ls: %lu - %ls\n", pwszMessage, dwErrorCode, wszCrtMsg);
        }
        else
        {
            fwprintf(stderr, L"%ls: %lu\n", pwszMessage, dwErrorCode);
        }
    }
    else
    {
        LPWSTR pwszSysMsg = NULL;

        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&pwszSysMsg, 0, NULL);

        if (pwszSysMsg == NULL)
        {
            fwprintf(stderr, L"%ls: %lu\n", pwszMessage, dwErrorCode);
        }
        else
        {
            fwprintf(stderr, L"%ls: %lu - %ls", pwszMessage, dwErrorCode, pwszSysMsg);
            LocalFree(pwszSysMsg);
        }
    }

#if defined(_DEBUG)
    __debugbreak();
#else
    HANDLE hDumpFile =
        CreateFileW(L"CrashDump.dmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hDumpFile != INVALID_HANDLE_VALUE)
    {
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpWithFullMemory, NULL, NULL,
                          NULL);
        CloseHandle(hDumpFile);
    }
#endif

    ExitProcess(dwErrorCode);
}