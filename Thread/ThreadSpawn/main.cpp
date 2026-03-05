#include <Windows.h> // Must be included before DbgHelp.h (Required for Win32 types)
#include <DbgHelp.h>
#include <process.h>
#include <stdio.h>

#pragma comment(lib, "Dbghelp.lib")

CRITICAL_SECTION g_criticalSection;

[[noreturn]] void HandleErrorAndFailW(LPCWSTR pwszMessage, DWORD dwErrorCode)
{
    LPWSTR pwszSysMsg = nullptr;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                   dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&pwszSysMsg, 0, NULL);

    if (pwszSysMsg == nullptr)
    {
        fwprintf(stderr, L"%ls: %lu\n", pwszMessage, dwErrorCode);
    }
    else
    {
        fwprintf(stderr, L"%ls: %lu - %ls", pwszMessage, dwErrorCode, pwszSysMsg);
        LocalFree(pwszSysMsg);
    }

    if (IsDebuggerPresent())
    {
        __debugbreak();
    }

    ExitProcess(dwErrorCode);
}

void PrintStackTrace(LPCSTR szThreadLabel)
{
    PVOID pStack[64] = { 0, };
    HANDLE hProcess = GetCurrentProcess();
    USHORT usFrames = CaptureStackBackTrace(0, 64, pStack, NULL);

    SYMBOL_INFO* pSymbol = (SYMBOL_INFO*)calloc(1, sizeof(SYMBOL_INFO) + 256);
    if (pSymbol == nullptr)
    {
        fwprintf(stderr, L"calloc failed for SYMBOL_INFO in PrintStackTrace\n");
        return;
    }

    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    pSymbol->MaxNameLen = 255;

    SIZE_T cbMaxOutput = 32768;
    char* pszOutput = (char*)calloc(1, cbMaxOutput);
    if (pszOutput == nullptr)
    {
        fwprintf(stderr, L"calloc failed for Output Buffer in PrintStackTrace\n");
        free(pSymbol);
        return;
    }

    SIZE_T cbOffset = 0;
    SIZE_T cbRemaining = cbMaxOutput;
    SIZE_T cbWrittenCount = 0;

    cbWrittenCount = snprintf(pszOutput + cbOffset, cbRemaining, "\n--- Stack Trace: %s (TID: %lu) ---\n", szThreadLabel,
                        GetCurrentThreadId());
    if (cbWrittenCount > 0 && cbWrittenCount < cbRemaining)
    {
        cbOffset += cbWrittenCount;
        cbRemaining -= cbWrittenCount;
    }

    EnterCriticalSection(&g_criticalSection);

    for (USHORT i = 0; i < usFrames; ++i)
    {
        if (pStack[i] == nullptr || cbRemaining == 0)
        {
            continue;
        }

        DWORD64 dw64Displacement = 0;
        if (SymFromAddr(hProcess, (DWORD64)pStack[i], &dw64Displacement, pSymbol))
        {
            cbWrittenCount = snprintf(pszOutput + cbOffset, cbRemaining, "[%02u] %-30s (0x%llX)\n", i + 1, pSymbol->Name,
                                pSymbol->Address);
        }
        else
        {
            DWORD dwError = GetLastError();
            cbWrittenCount = snprintf(pszOutput + cbOffset, cbRemaining, "[%02u] Unknown Symbol (0x%p) - Error: %lu\n", i + 1,
                                pStack[i], dwError);
        }

        if (cbWrittenCount > 0 && cbWrittenCount < cbRemaining)
        {
            cbOffset += cbWrittenCount;
            cbRemaining -= cbWrittenCount;
        }
        else
        {
            cbRemaining = 0;
        }
    }

    LeaveCriticalSection(&g_criticalSection);

    if (cbRemaining > 0)
    {
        snprintf(pszOutput + cbOffset, cbRemaining, "--------------------------------------------------\n");
    }

    printf("%s", pszOutput);

    free(pszOutput);
    pszOutput = nullptr;

    free(pSymbol);
    pSymbol = nullptr;
}

DWORD WINAPI Win32Worker(LPVOID lpParam)
{
    DWORD dwThreadExitCode = 13;
    PrintStackTrace("Win32Worker (CreateThread)");
    return dwThreadExitCode;
}

UINT __stdcall CrtWorker(PVOID pParam)
{
    UINT dwThreadExitCode = 27;
    PrintStackTrace("CrtWorker (_beginthreadex)");
    return dwThreadExitCode;
}

int main(void)
{
    InitializeCriticalSection(&g_criticalSection);

    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    if (!SymInitialize(GetCurrentProcess(), NULL, TRUE))
    {
        HandleErrorAndFailW(L"SymInitialize failed", GetLastError());
    }

    HANDLE hThreads[2] = { nullptr, nullptr };
    DWORD dwExitCodes[2] = { 0, 0 };
    LPCWSTR pwszThreadNames[2] = { L"Win32 Thread", L"CRT Thread" };

    hThreads[0] = CreateThread(NULL, 0, Win32Worker, NULL, 0, NULL);
    if (hThreads[0] == nullptr)
    {
        HandleErrorAndFailW(L"CreateThread failed", GetLastError());
    }

    hThreads[1] = (HANDLE)_beginthreadex(NULL, 0, CrtWorker, NULL, 0, NULL);
    if (hThreads[1] == nullptr)
    {
        ULONG nDosErrno = 0;
        _get_doserrno(&nDosErrno);
        HandleErrorAndFailW(L"_beginthreadex failed", (DWORD)nDosErrno);
    }

    for (SIZE_T i = 0; i < 2; ++i)
    {
        DWORD dwWaitResult = WaitForSingleObject(hThreads[i], INFINITE);

        if (dwWaitResult == WAIT_FAILED)
        {
            fwprintf(stderr, L"Wait failed for: %ls\n", pwszThreadNames[i]);
            HandleErrorAndFailW(L"WaitForSingleObject failed", GetLastError());
        }

        if (!GetExitCodeThread(hThreads[i], &dwExitCodes[i]))
        {
            fwprintf(stderr, L"Failed to get exit code for: %ls\n", pwszThreadNames[i]);
            HandleErrorAndFailW(L"GetExitCodeThread failed", GetLastError());
        }
    }

    printf("\n"
           "============================================\n"
           "            Thread Execution Results        \n"
           "============================================\n"
           "    Win32 API Exit Code       : %-10lu\n"
           "    CRT Wrapper Exit Code     : %-10lu\n"
           "============================================\n",
           dwExitCodes[0], dwExitCodes[1]);

    if (!CloseHandle(hThreads[0]))
    {
        fwprintf(stderr, L"CloseHandle failed for Win32 thread: %lu\n", GetLastError());
    }
    hThreads[0] = nullptr;

    if (!CloseHandle(hThreads[1]))
    {
        fwprintf(stderr, L"CloseHandle failed for CRT thread: %lu\n", GetLastError());
    }
    hThreads[1] = nullptr;

    SymCleanup(GetCurrentProcess());

    DeleteCriticalSection(&g_criticalSection);

    return 0;
}