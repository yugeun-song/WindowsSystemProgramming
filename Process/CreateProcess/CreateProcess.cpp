#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>

#include "ErrorHelper.h"

void RunChildRoutine(void)
{
    LOG_INFO(L"[Child] Terminating in 30 seconds... (PID: %lu)", GetCurrentProcessId());
    Sleep(30000);
}

int main(void)
{
    LPWSTR commandLine = GetCommandLineW();
    if (commandLine != NULL && wcsstr(commandLine, L"--child-mode") != NULL)
    {
        DWORD returnCode = 1234;
        RunChildRoutine();
        return returnCode;
    }

    WCHAR modulePath[MAX_PATH] = { 0, };
    DWORD pathLength = GetModuleFileNameW(NULL, modulePath, ARRAYSIZE(modulePath));
    if (pathLength == 0 || pathLength >= ARRAYSIZE(modulePath))
    {
        HandleErrorAndFail(L"GetModuleFileNameW failed", GetLastError(), FALSE);
        return 1;
    }

    STARTUPINFOW startupInfo = { sizeof(startupInfo), };
    PROCESS_INFORMATION processInfo = { 0, };
    WCHAR newCommandLine[MAX_PATH * 2] = { 0, };

    if (_snwprintf(newCommandLine, ARRAYSIZE(newCommandLine), L"\"%s\" --child-mode", modulePath) < 0)
    {
        LOG_ERROR(L"Command line buffer overflow");
        return 1;
    }

    if (!CreateProcessW(NULL, newCommandLine, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &startupInfo, &processInfo))
    {
        HandleErrorAndFail(L"CreateProcessW failed", GetLastError(), FALSE);
        return 1;
    }

    LOG_INFO(L"[Parent] Child Process created (PID: %lu)", processInfo.dwProcessId);

    if (WaitForSingleObject(processInfo.hProcess, INFINITE) == WAIT_FAILED)
    {
        HandleErrorAndFail(L"WaitForSingleObject failed", GetLastError(), FALSE);
    }

    DWORD exitCode = 0;
    if (!GetExitCodeProcess(processInfo.hProcess, &exitCode))
    {
        HandleErrorAndFail(L"GetExitCodeProcess failed", GetLastError(), FALSE);
    }

    if (!CloseHandle(processInfo.hThread))
    {
        HandleErrorAndFail(L"CloseHandle (hThread) failed", GetLastError(), FALSE);
    }
    processInfo.hThread = NULL;

    if (!CloseHandle(processInfo.hProcess))
    {
        HandleErrorAndFail(L"CloseHandle (hProcess) failed", GetLastError(), FALSE);
    }
    processInfo.hProcess = NULL;

    LOG_INFO(L"[Parent] Child (PID: %lu) exited with code: %lu", processInfo.dwProcessId, exitCode);

    return 0;
}