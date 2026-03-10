#include <Windows.h>
#include <errno.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "ErrorHelper.h"

CRITICAL_SECTION g_cs;
ULONGLONG g_dwSharedData = 0;
SIZE_T g_dwGoalCount;

UINT __stdcall RunThread(PVOID pParam)
{
    for (SIZE_T i = 0; i < g_dwGoalCount; ++i)
    {
        EnterCriticalSection(&g_cs);
        ++g_dwSharedData;
        LeaveCriticalSection(&g_cs);
    }
    return 0;
}

int main(void)
{
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    DWORD dwThreadCount = sysInfo.dwNumberOfProcessors;

    if (dwThreadCount > MAXIMUM_WAIT_OBJECTS)
    {
        dwThreadCount = MAXIMUM_WAIT_OBJECTS;
    }

    srand((unsigned int)time(NULL));
    g_dwGoalCount = (((SIZE_T)rand() << 15) | rand()) % 1000000000 + 1;

    HANDLE* hThreads = (HANDLE*)malloc(sizeof(HANDLE) * dwThreadCount);
    if (hThreads == NULL)
    {
        HandleErrorAndFailW(L"malloc failed for hThreads", errno, TRUE);
    }
    ZeroMemory(hThreads, sizeof(HANDLE) * dwThreadCount);

    DWORD* dwThreadExitCodes = (DWORD*)malloc(sizeof(DWORD) * dwThreadCount);
    if (dwThreadExitCodes == NULL)
    {
        free(hThreads);
        HandleErrorAndFailW(L"malloc failed for dwThreadExitCodes", errno, TRUE);
    }
    ZeroMemory(dwThreadExitCodes, sizeof(DWORD) * dwThreadCount);

    InitializeCriticalSection(&g_cs);

    for (DWORD i = 0; i < dwThreadCount; ++i)
    {
        hThreads[i] = (HANDLE)_beginthreadex(NULL, 0, RunThread, NULL, 0, NULL);
        if (hThreads[i] == NULL)
        {
            HandleErrorAndFailW(L"_beginthreadex failed", errno, TRUE);
        }
    }

    DWORD dwWaitResult = WaitForMultipleObjects(dwThreadCount, hThreads, TRUE, INFINITE);
    if (dwWaitResult == WAIT_FAILED)
    {
        HandleErrorAndFailW(L"WaitForMultipleObjects failed", GetLastError(), FALSE);
    }

    for (DWORD i = 0; i < dwThreadCount; ++i)
    {
        if (!GetExitCodeThread(hThreads[i], &(dwThreadExitCodes[i])))
        {
            HandleErrorAndFailW(L"GetExitCodeThread failed", GetLastError(), FALSE);
        }

        if (!CloseHandle(hThreads[i]))
        {
            HandleErrorAndFailW(L"CloseHandle failed", GetLastError(), FALSE);
        }
    }

    const ULONGLONG dwExpectedResult = (ULONGLONG)dwThreadCount * (ULONGLONG)g_dwGoalCount;
    wprintf(L"Expected Result: %llu\n"
            L"Actual Result:   %llu\n",
            dwExpectedResult, g_dwSharedData);

    if (g_dwSharedData == dwExpectedResult)
    {
        wprintf(L"Verification: SUCCESS\n");
    }
    else
    {
        wprintf(L"Verification: FAILED\n");
    }

    free(hThreads);
    hThreads = NULL;

    free(dwThreadExitCodes);
    dwThreadExitCodes = NULL;

    DeleteCriticalSection(&g_cs);

    return 0;
}