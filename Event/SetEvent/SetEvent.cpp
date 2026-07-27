#include <Windows.h>
#include <process.h>

#include "ErrorHelper.h"

UINT __stdcall RunThread(PVOID param)
{
    HANDLE waitEvent = (HANDLE)param;
    UINT exitCode = 0;
    DWORD waitResult = WaitForSingleObject(waitEvent, INFINITE);

    if (waitResult == WAIT_FAILED)
    {
        HandleErrorAndFail(L"WaitForSingleObject failed", GetLastError(), FALSE);
    }

    LOG_INFO(L"Worker thread signal turned on!");
    Sleep(1000);
    LOG_INFO(L"Worker thread exit!");

    return exitCode;
}

int main(void)
{
    HANDLE thread = NULL;
    DWORD threadExitCode = 0;

    HANDLE event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (event == NULL)
    {
        HandleErrorAndFail(L"CreateEventW failed", GetLastError(), FALSE);
    }

    LOG_INFO(L"Creating Thread...");
    thread = (HANDLE)_beginthreadex(NULL, 0, RunThread, event, 0, NULL);
    if (thread == NULL)
    {
        ULONG dosError = 0;
        _get_doserrno(&dosError);
        HandleErrorAndFail(L"_beginthreadex failed", (DWORD)dosError, TRUE);
    }

    Sleep(1000);

    LOG_INFO(L"Sending Event...");
    if (!SetEvent(event))
    {
        HandleErrorAndFail(L"SetEvent failed", GetLastError(), FALSE);
    }

    DWORD waitResult = WaitForSingleObject(thread, INFINITE);
    if (waitResult == WAIT_FAILED)
    {
        HandleErrorAndFail(L"WaitForSingleObject failed", GetLastError(), FALSE);
    }

    if (!GetExitCodeThread(thread, &threadExitCode))
    {
        HandleErrorAndFail(L"GetExitCodeThread failed", GetLastError(), FALSE);
    }

    if (!CloseHandle(thread))
    {
        HandleErrorAndFail(L"CloseHandle failed", GetLastError(), FALSE);
    }
    thread = NULL;

    if (!CloseHandle(event))
    {
        HandleErrorAndFail(L"CloseHandle failed", GetLastError(), FALSE);
    }
    event = NULL;

    LOG_INFO(L"Main Thread finished!");

    return 0;
}