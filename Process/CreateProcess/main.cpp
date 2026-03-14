#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>

#include "ErrorHelper.h"

void RunChildRoutine(void)
{
    wprintf(L"[Child] Terminating in 10 seconds... (PID: %lu)\n", GetCurrentProcessId());
    Sleep(10000);
}

int main(void)
{
    LPWSTR pwszCmdLine = GetCommandLineW();
    if (pwszCmdLine != NULL && wcsstr(pwszCmdLine, L"--child-mode") != NULL)
    {
        DWORD dwProcessReturnCode = 143;
        RunChildRoutine();
        return dwProcessReturnCode;
    }

    WCHAR wszModulePath[MAX_PATH] = { 0, };
    DWORD dwPathLen = GetModuleFileNameW(NULL, wszModulePath, ARRAYSIZE(wszModulePath));
    if (dwPathLen == 0 || dwPathLen >= ARRAYSIZE(wszModulePath))
    {
        HandleErrorAndFailW(L"GetModuleFileNameW failed", GetLastError(), FALSE);
        return 1;
    }

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0, };
    WCHAR wszNewCmdLine[MAX_PATH * 2] = { 0, };

    if (_snwprintf(wszNewCmdLine, ARRAYSIZE(wszNewCmdLine), L"\"%s\" --child-mode", wszModulePath) < 0)
    {
        fwprintf(stderr, L"Command line buffer overflow\n");
        return 1;
    }

    if (!CreateProcessW(NULL, wszNewCmdLine, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi))
    {
        HandleErrorAndFailW(L"CreateProcessW failed", GetLastError(), FALSE);
        return 1;
    }

    wprintf(L"[Parent] Child Process created\n");

    if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED)
    {
        HandleErrorAndFailW(L"WaitForSingleObject failed", GetLastError(), FALSE);
    }

    DWORD dwExitCode = 0;
    if (!GetExitCodeProcess(pi.hProcess, &dwExitCode))
    {
        HandleErrorAndFailW(L"GetExitCodeProcess failed", GetLastError(), FALSE);
    }

    if (!CloseHandle(pi.hThread))
    {
        HandleErrorAndFailW(L"CloseHandle (hThread) failed", GetLastError(), FALSE);
    }
    pi.hThread = NULL;

    if (!CloseHandle(pi.hProcess))
    {
        HandleErrorAndFailW(L"CloseHandle (hProcess) failed", GetLastError(), FALSE);
    }
    pi.hProcess = NULL;

    wprintf(L"[Parent] Child (PID: %lu) exited with code: %lu\n", pi.dwProcessId, dwExitCode);

    return 0;
}