#include <windows.h>
#include <intrin.h>
#include <stdio.h>
#include <wchar.h>

#include "ErrorHelper.h"

void RunChildRoutine(void)
{
    wprintf(L"[Child] Process terminating in 10s. PID: %lu\n", GetCurrentProcessId());
    Sleep(10000);
}

int main(void)
{
    wchar_t* cmdLine = GetCommandLineW();
    if (cmdLine != NULL && wcsstr(cmdLine, L"--child-mode") != NULL)
    {
        RunChildRoutine();
        return 0;
    }

    wchar_t exePath[MAX_PATH];
    DWORD dwRet = GetModuleFileNameW(NULL, exePath, MAX_PATH);
    if (dwRet == 0 || dwRet == MAX_PATH)
    {
        HandleErrorAndFailW(L"Failed to get module file name", GetLastError(), FALSE);
    }

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    wchar_t newCmdLine[MAX_PATH * 2];

    if (swprintf_s(newCmdLine, ARRAYSIZE(newCmdLine), L"\"%s\" --child-mode", exePath) < 0)
    {
        printf("Failed to format command line string\n");
        return 1;
    }

    if (CreateProcessW(NULL, newCmdLine, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi))
    {
        wprintf(L"[Parent] Child spawned (PID: %lu). Detaching...\n", pi.dwProcessId);

        if (!CloseHandle(pi.hThread))
        {
            HandleErrorAndFailW(L"Failed to close child thread handle", GetLastError(), FALSE);
        }
        if (!CloseHandle(pi.hProcess))
        {
            HandleErrorAndFailW(L"Failed to close child process handle", GetLastError(), FALSE);
        }
    }
    else
    {
        HandleErrorAndFailW(L"Failed to create child process", GetLastError(), FALSE);
    }

    wprintf(L"[Parent] Terminating... Child will exit in 10s.\n");
    return 0;
}