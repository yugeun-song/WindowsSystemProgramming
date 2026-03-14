#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h> // Must be included before DbgHelp.h (Required for Win32 types)
#include <DbgHelp.h>
#include <process.h>

#include "ErrorHelper.h"

#pragma comment(lib, "Dbghelp.lib")

CRITICAL_SECTION g_cs;

void PrintStackTrace(LPCWSTR pwszThreadLabel)
{
    PVOID pStack[64] = { 0, };
    HANDLE hProcess = GetCurrentProcess();
    USHORT usFrames = CaptureStackBackTrace(0, 64, pStack, NULL);

    const SIZE_T cbSymbolInfo = sizeof(SYMBOL_INFO) + (MAX_SYM_NAME * sizeof(char));
    SYMBOL_INFO* pSymbolInfo = (SYMBOL_INFO*)calloc(1, cbSymbolInfo);
    if (pSymbolInfo == NULL)
    {
        fwprintf(stderr, L"calloc failed for SYMBOL_INFO\n");
        return;
    }

    pSymbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbolInfo->MaxNameLen = MAX_SYM_NAME;

    const DWORD dwMaxOutput = 32768;
    LPWSTR pwszOutput = (LPWSTR)calloc(dwMaxOutput, sizeof(WCHAR));
    if (pwszOutput == NULL)
    {
        fwprintf(stderr, L"calloc failed for Output Buffer\n");
        free(pSymbolInfo);
        pSymbolInfo = NULL;
        return;
    }

    DWORD dwOffset = 0;
    DWORD dwRemaining = dwMaxOutput;
    DWORD dwWritten = _snwprintf(pwszOutput + dwOffset, dwRemaining, L"\n--- Stack Trace: %s (TID: %lu) ---\n",
                                 pwszThreadLabel, GetCurrentThreadId());

    if (dwWritten > 0 && dwWritten < dwRemaining)
    {
        dwOffset += dwWritten;
        dwRemaining -= dwWritten;
    }

    EnterCriticalSection(&g_cs);

    for (USHORT i = 0; i < usFrames; ++i)
    {
        if (pStack[i] == NULL || dwRemaining == 0)
        {
            continue;
        }

        DWORD64 dw64Displacement = 0;
        if (SymFromAddr(hProcess, (DWORD64)pStack[i], &dw64Displacement, pSymbolInfo))
        {
            dwWritten = _snwprintf(pwszOutput + dwOffset, dwRemaining, L"[#%02u] %-30S (0x%llX)\n", i + 1,
                                  pSymbolInfo->Name, pSymbolInfo->Address);
        }
        else
        {
            dwWritten = _snwprintf(pwszOutput + dwOffset, dwRemaining, L"[#%02u] Unknown Symbol (0x%p) - Error: %lu\n",
                                  i + 1, pStack[i], GetLastError());
        }

        if (dwWritten > 0 && dwWritten < (int)dwRemaining)
        {
            dwOffset += dwWritten;
            dwRemaining -= dwWritten;
        }
        else
        {
            dwRemaining = 0;
        }
    }

    LeaveCriticalSection(&g_cs);

    wprintf(L"%s", pwszOutput);

    free(pwszOutput);
    pwszOutput = NULL;

    free(pSymbolInfo);
    pSymbolInfo = NULL;
}

DWORD WINAPI Win32Worker(LPVOID lpParam)
{
    DWORD dwExitCode = 13;
    PrintStackTrace(L"Win32Worker (CreateThread)");
    return dwExitCode;
}

UINT __stdcall RunThread(PVOID pParam)
{
    DWORD dwExitCode = 27;
    PrintStackTrace(L"CrtWorker (_beginthreadex)");
    return (UINT)dwExitCode;
}

int main(void)
{
    InitializeCriticalSection(&g_cs);

    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    if (!SymInitialize(GetCurrentProcess(), NULL, TRUE))
    {
        HandleErrorAndFailW(L"SymInitialize failed", GetLastError(), FALSE);
    }

    HANDLE hThreads[2] = { NULL, NULL };
    DWORD dwExitCodes[2] = { 0, 0 };
    LPCWSTR pwszThreadNames[2] = { L"Win32 Thread", L"CRT Thread" };

    hThreads[0] = CreateThread(NULL, 0, Win32Worker, NULL, 0, NULL);
    if (hThreads[0] == NULL)
    {
        HandleErrorAndFailW(L"CreateThread failed", GetLastError(), FALSE);
    }

    hThreads[1] = (HANDLE)_beginthreadex(NULL, 0, RunThread, NULL, 0, NULL);
    if (hThreads[1] == NULL)
    {
        ULONG ulDosError = 0;
        _get_doserrno(&ulDosError);
        HandleErrorAndFailW(L"_beginthreadex failed", (DWORD)ulDosError, TRUE);
    }

    DWORD dwWaitResult = WaitForMultipleObjects(2, hThreads, TRUE, INFINITE);
    if (dwWaitResult == WAIT_FAILED)
    {
        HandleErrorAndFailW(L"WaitForMultipleObjects failed", GetLastError(), FALSE);
    }

    for (DWORD i = 0; i < 2; ++i)
    {
        if (!GetExitCodeThread(hThreads[i], &dwExitCodes[i]))
        {
            fwprintf(stderr, L"Failed to get exit code for: %ls\n", pwszThreadNames[i]);
            HandleErrorAndFailW(L"GetExitCodeThread failed", GetLastError(), FALSE);
        }
    }

    wprintf(L"\n"
            L"============================================\n"
            L"            Thread Execution Results        \n"
            L"============================================\n"
            L"    Win32 API Exit Code       : %-10lu\n"
            L"    CRT Wrapper Exit Code     : %-10lu\n"
            L"============================================\n",
            dwExitCodes[0], dwExitCodes[1]);

    if (!CloseHandle(hThreads[0]))
    {
        fwprintf(stderr, L"CloseHandle failed for Win32 thread: %lu\n", GetLastError());
    }
    hThreads[0] = NULL;

    if (!CloseHandle(hThreads[1]))
    {
        fwprintf(stderr, L"CloseHandle failed for CRT thread: %lu\n", GetLastError());
    }
    hThreads[1] = NULL;

    SymCleanup(GetCurrentProcess());

    DeleteCriticalSection(&g_cs);

    return 0;
}