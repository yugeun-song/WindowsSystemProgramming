#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h> // Must be included before DbgHelp.h (Required for Win32 types)
#include <DbgHelp.h>
#include <process.h>

#include "ErrorHelper.h"

#pragma comment(lib, "Dbghelp.lib")

CRITICAL_SECTION g_symbolLock;

void PrintStackTrace(LPCWSTR threadLabel)
{
    PVOID stackFrames[64] = { 0, };
    HANDLE process = GetCurrentProcess();
    USHORT frameCount = CaptureStackBackTrace(0, 64, stackFrames, NULL);

    const SIZE_T symbolInfoSize = sizeof(SYMBOL_INFO) + (MAX_SYM_NAME * sizeof(char));
    SYMBOL_INFO* symbolInfo = (SYMBOL_INFO*)calloc(1, symbolInfoSize);
    if (symbolInfo == NULL)
    {
        LOG_ERROR(L"calloc failed for SYMBOL_INFO");
        return;
    }

    symbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbolInfo->MaxNameLen = MAX_SYM_NAME;

    const DWORD maxOutputChars = 32768;
    LPWSTR output = (LPWSTR)calloc(maxOutputChars, sizeof(WCHAR));
    if (output == NULL)
    {
        LOG_ERROR(L"calloc failed for Output Buffer");
        free(symbolInfo);
        symbolInfo = NULL;
        return;
    }

    DWORD offset = 0;
    DWORD remaining = maxOutputChars;
    DWORD written = _snwprintf(output + offset, remaining, L"\n--- Stack Trace: %s (TID: %lu) ---\n",
                                 threadLabel, GetCurrentThreadId());

    if (written > 0 && written < remaining)
    {
        offset += written;
        remaining -= written;
    }

    EnterCriticalSection(&g_symbolLock);

    for (USHORT i = 0; i < frameCount; ++i)
    {
        if (stackFrames[i] == NULL || remaining == 0)
        {
            continue;
        }

        DWORD64 displacement = 0;
        if (SymFromAddr(process, (DWORD64)stackFrames[i], &displacement, symbolInfo))
        {
            written = _snwprintf(output + offset, remaining, L"[#%02u] %-30S (0x%llX)\n", i + 1,
                                  symbolInfo->Name, symbolInfo->Address);
        }
        else
        {
            written = _snwprintf(output + offset, remaining, L"[#%02u] Unknown Symbol (0x%p) - Error: %lu\n",
                                  i + 1, stackFrames[i], GetLastError());
        }

        if (written > 0 && written < (int)remaining)
        {
            offset += written;
            remaining -= written;
        }
        else
        {
            remaining = 0;
        }
    }

    LeaveCriticalSection(&g_symbolLock);

    wprintf(L"%s", output);

    free(output);
    output = NULL;

    free(symbolInfo);
    symbolInfo = NULL;
}

DWORD WINAPI Win32Worker(LPVOID param)
{
    DWORD exitCode = 13;
    PrintStackTrace(L"Win32Worker (CreateThread)");
    return exitCode;
}

UINT __stdcall RunThread(PVOID param)
{
    DWORD exitCode = 27;
    PrintStackTrace(L"CrtWorker (_beginthreadex)");
    return (UINT)exitCode;
}

int main(void)
{
    InitializeCriticalSection(&g_symbolLock);

    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    if (!SymInitialize(GetCurrentProcess(), NULL, TRUE))
    {
        HandleErrorAndFail(L"SymInitialize failed", GetLastError(), FALSE);
    }

    HANDLE threads[2] = { NULL, NULL };
    DWORD exitCodes[2] = { 0, 0 };
    LPCWSTR threadNames[2] = { L"Win32 Thread", L"CRT Thread" };

    threads[0] = CreateThread(NULL, 0, Win32Worker, NULL, 0, NULL);
    if (threads[0] == NULL)
    {
        HandleErrorAndFail(L"CreateThread failed", GetLastError(), FALSE);
    }

    threads[1] = (HANDLE)_beginthreadex(NULL, 0, RunThread, NULL, 0, NULL);
    if (threads[1] == NULL)
    {
        ULONG dosError = 0;
        _get_doserrno(&dosError);
        HandleErrorAndFail(L"_beginthreadex failed", (DWORD)dosError, TRUE);
    }

    DWORD waitResult = WaitForMultipleObjects(2, threads, TRUE, INFINITE);
    if (waitResult == WAIT_FAILED)
    {
        HandleErrorAndFail(L"WaitForMultipleObjects failed", GetLastError(), FALSE);
    }

    for (DWORD i = 0; i < 2; ++i)
    {
        if (!GetExitCodeThread(threads[i], &exitCodes[i]))
        {
            LOG_ERROR(L"Failed to get exit code for: %ls", threadNames[i]);
            HandleErrorAndFail(L"GetExitCodeThread failed", GetLastError(), FALSE);
        }
    }

    wprintf(L"\n"
            L"============================================\n"
            L"            Thread Execution Results        \n"
            L"============================================\n"
            L"    Win32 API Exit Code       : %-10lu\n"
            L"    CRT Wrapper Exit Code     : %-10lu\n"
            L"============================================\n",
            exitCodes[0], exitCodes[1]);

    if (!CloseHandle(threads[0]))
    {
        LOG_ERROR(L"CloseHandle failed for Win32 thread: %lu", GetLastError());
    }
    threads[0] = NULL;

    if (!CloseHandle(threads[1]))
    {
        LOG_ERROR(L"CloseHandle failed for CRT thread: %lu", GetLastError());
    }
    threads[1] = NULL;

    SymCleanup(GetCurrentProcess());

    DeleteCriticalSection(&g_symbolLock);

    return 0;
}