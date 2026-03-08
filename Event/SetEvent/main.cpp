#include <Windows.h>
#include <errno.h>
#include <process.h>
#include <stdio.h>

#include "ErrorHelper.h"

UINT __stdcall RunThread(PVOID pParam)
{
    HANDLE pHEvent = (HANDLE)pParam;
    UINT dwThreadExitCode = 0;
    DWORD dwWaitResult = WaitForSingleObject(pHEvent, INFINITE);

    if (dwWaitResult == WAIT_FAILED)
    {
        HandleErrorAndFailW(L"WaitForSingleObject failed", GetLastError(), FALSE);
    }
    printf("Worker thread signal turned on!\n");
    Sleep(1000);
    printf("Worker thread exit!\n");

    return dwThreadExitCode;
}

int main(void)
{
    HANDLE hThread = NULL;
    DWORD dwThreadExitCode;

    HANDLE hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (hEvent == NULL)
    {
        HandleErrorAndFailW(L"CreateEventW failed", GetLastError(), FALSE);
    }

    printf("Creating Thread...\n");
    hThread = (HANDLE)_beginthreadex(NULL, 0, RunThread, hEvent, 0, NULL);
    if (hThread == NULL)
    {
        HandleErrorAndFailW(L"_beginthreadex failed", errno, TRUE);
    }
    Sleep(1000);

    printf("Sending Event...\n");
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
    hThread = INVALID_HANDLE_VALUE;

    if (!CloseHandle(hEvent))
    {
        HandleErrorAndFailW(L"CloseHandle failed", GetLastError(), FALSE);
    }
    hEvent = INVALID_HANDLE_VALUE;

    return 0;
}