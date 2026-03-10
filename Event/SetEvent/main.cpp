#include <Windows.h>
#include <process.h>

#include "ErrorHelper.h"

UINT __stdcall RunThread(PVOID pParam)
{
    HANDLE hWaitEvent = (HANDLE)pParam;
    DWORD dwExitCode = 0;
    DWORD dwWaitResult = WaitForSingleObject(hWaitEvent, INFINITE);

    if (dwWaitResult == WAIT_FAILED)
    {
        HandleErrorAndFailW(L"WaitForSingleObject failed", GetLastError(), FALSE);
    }

    wprintf(L"Worker thread signal turned on!\n");
    Sleep(1000);
    wprintf(L"Worker thread exit!\n");

    return (UINT)dwExitCode;
}

int main(void)
{
    HANDLE hThread = NULL;
    DWORD dwThreadExitCode = 0;

    HANDLE hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (hEvent == NULL)
    {
        HandleErrorAndFailW(L"CreateEventW failed", GetLastError(), FALSE);
    }

    wprintf(L"Creating Thread...\n");
    hThread = (HANDLE)_beginthreadex(NULL, 0, RunThread, hEvent, 0, NULL);
    if (hThread == NULL)
    {
        ULONG ulDosError = 0;
        _get_doserrno(&ulDosError);
        HandleErrorAndFailW(L"_beginthreadex failed", (DWORD)ulDosError, TRUE);
    }

    Sleep(1000);

    wprintf(L"Sending Event...\n");
    if (!SetEvent(hEvent))
    {
        HandleErrorAndFailW(L"SetEvent failed", GetLastError(), FALSE);
    }

    DWORD dwWaitResult = WaitForSingleObject(hThread, INFINITE);
    if (dwWaitResult == WAIT_FAILED)
    {
        HandleErrorAndFailW(L"WaitForSingleObject failed", GetLastError(), FALSE);
    }

    if (!GetExitCodeThread(hThread, &dwThreadExitCode))
    {
        HandleErrorAndFailW(L"GetExitCodeThread failed", GetLastError(), FALSE);
    }

    if (!CloseHandle(hThread))
    {
        HandleErrorAndFailW(L"CloseHandle failed", GetLastError(), FALSE);
    }
    hThread = NULL;

    if (!CloseHandle(hEvent))
    {
        HandleErrorAndFailW(L"CloseHandle failed", GetLastError(), FALSE);
    }
    hEvent = NULL;

    wprintf(L"Main Thread finished!\n");

    return 0;
}