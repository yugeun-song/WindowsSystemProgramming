#include <Windows.h> // Must be included before DbgHelp.h (Required for Win32 types)
#include <DbgHelp.h>
#include <process.h>
#include <stdio.h>

#pragma comment(lib, "Dbghelp.lib")

#define MAX_STACK_TRACE_HISTORY 512

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_RESET "\x1b[0m"

CRITICAL_SECTION g_criticalSection;

PVOID g_pAddressHistory[MAX_STACK_TRACE_HISTORY];
DWORD g_dwHistoryCount = 0;

[[noreturn]] void HandleErrorAndFailW(const wchar_t* pwszMessage)
{
    DWORD dwLastError = GetLastError();
    fwprintf(stderr, L"%ls: %lu\n", pwszMessage, dwLastError);

    if (IsDebuggerPresent())
    {
        __debugbreak();
    }
    else
    {
        __fastfail(FAST_FAIL_FATAL_APP_EXIT);
    }
}

void PrintStackTrace(const char* szThreadLabel)
{
    PVOID pStack[64] = { 0, };
    HANDLE hProcess = GetCurrentProcess();
    USHORT usFrames = CaptureStackBackTrace(0, 64, pStack, NULL);

    SYMBOL_INFO* pSymbol = (SYMBOL_INFO*)malloc(sizeof(SYMBOL_INFO) + 256);
    if (pSymbol == NULL)
    {
        return;
    }
    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = 255;

    EnterCriticalSection(&g_criticalSection);

    printf("\n--- Stack Trace: %s (TID: %lu) ---\n", szThreadLabel, GetCurrentThreadId());

    for (USHORT i = 0; i < usFrames; ++i)
    {
        int isDuplicate = 0;

        for (int j = 0; j < g_dwHistoryCount; ++j)
        {
            if (g_pAddressHistory[j] == pStack[i])
            {
                isDuplicate = 1;
                break;
            }
        }

        DWORD64 dw64Displacement = 0;
        if (SymFromAddr(hProcess, (DWORD64)pStack[i], &dw64Displacement, pSymbol))
        {
            if (isDuplicate)
                printf(ANSI_COLOR_RED);

            printf("[%02u] %-30s (0x%llX)\n", i, pSymbol->Name, pSymbol->Address);

            if (isDuplicate)
                printf(ANSI_COLOR_RESET);
        }
        else
        {
            printf("[%02u] Unknown Symbol (0x%p)\n", i, pStack[i]);
        }

        if (!isDuplicate && g_historyCount < MAX_STACK_TRACE_HISTORY)
        {
            g_pAddressHistory[g_dwHistoryCount] = pStack[i];
            ++g_dwHistoryCount;
        }
    }
    printf("--------------------------------------------------\n");

    LeaveCriticalSection(&g_criticalSection);
    free(pSymbol);
}

DWORD WINAPI Win32Worker(LPVOID lpParam)
{
    DWORD dwThreadExitCode = 13;
    PrintStackTrace("Win32Worker");
    return dwThreadExitCode;
}

unsigned int __stdcall CrtWorker(PVOID pParam)
{
    DWORD dwThreadExitCode = 27;
    PrintStackTrace("CrtWorker");
    return dwThreadExitCode;
}

int main(void)
{
    InitializeCriticalSection(&g_criticalSection);

    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    if (!SymInitialize(GetCurrentProcess(), NULL, TRUE))
    {
        HandleErrorAndFailW(L"SymInitialize failed");
    }

    HANDLE hThreads[2] = { NULL, NULL };
    DWORD dwExitCodes[2] = { 0, 0 };
    const wchar_t* pwszThreadNames[2] = { L"Win32 Thread", L"CRT Thread" };

    hThreads[0] = CreateThread(NULL, 0, Win32Worker, NULL, 0, NULL);
    if (hThreads[0] == NULL)
    {
        HandleErrorAndFailW(L"CreateThread failed");
    }

    hThreads[1] = (HANDLE)_beginthreadex(NULL, 0, CrtWorker, NULL, 0, NULL);
    if (hThreads[1] == NULL)
    {
        HandleErrorAndFailW(L"_beginthreadex failed");
    }

    for (int i = 0; i < 2; ++i)
    {
        DWORD dwWaitResult = WaitForSingleObject(hThreads[i], INFINITE);

        if (dwWaitResult == WAIT_FAILED)
        {
            fwprintf(stderr, L"Wait failed for: %ls\n", pwszThreadNames[i]);
            HandleErrorAndFailW(L"WaitForSingleObject failed");
        }

        if (!GetExitCodeThread(hThreads[i], &dwExitCodes[i]))
        {
            fwprintf(stderr, L"Failed to get exit code for: %ls\n", pwszThreadNames[i]);
            HandleErrorAndFailW(L"GetExitCodeThread failed");
        }
    }

    printf("\n"
           "============================================\n"
           "           Thread Execution Results         \n"
           "============================================\n"
           "    Win32 API Exit Code       : %-10lu\n"
           "    CRT Wrapper Exit Code     : %-10lu\n"
           "============================================\n",
           dwExitCodes[0], dwExitCodes[1]);

    if (!CloseHandle(hThreads[0]))
    {
        HandleErrorAndFailW(L"CloseHandle failed for Win32 thread");
    }
    hThreads[0] = NULL;

    if (!CloseHandle(hThreads[1]))
    {
        HandleErrorAndFailW(L"CloseHandle failed for CRT thread");
    }
    hThreads[1] = NULL;

    SymCleanup(GetCurrentProcess());

    DeleteCriticalSection(&g_criticalSection);

    return 0;
}