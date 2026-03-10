#include <Windows.h>
#include <process.h>

#include "ErrorHelper.h"

CRITICAL_SECTION g_cs;
ULONGLONG g_ullSharedData = 0;
DWORD g_dwGoalCountPerThread = 1000000;

UINT __stdcall RunThread(PVOID pParam)
{
    for (DWORD i = 0; i < g_dwGoalCountPerThread; ++i)
    {
        EnterCriticalSection(&g_cs);
        ++g_ullSharedData;
        LeaveCriticalSection(&g_cs);
    }
    return 0;
}

int main(void)
{
    SYSTEM_INFO sysInfo = {0};
    GetSystemInfo(&sysInfo);
    DWORD dwThreadCount = sysInfo.dwNumberOfProcessors;

    if (dwThreadCount > MAXIMUM_WAIT_OBJECTS)
    {
        dwThreadCount = MAXIMUM_WAIT_OBJECTS;
    }

    HANDLE* hThreads = (HANDLE*)calloc(dwThreadCount, sizeof(HANDLE));
    if (hThreads == NULL)
    {
        ULONG ulDosError = 0;
        _get_doserrno(&ulDosError);
        HandleErrorAndFailW(L"calloc failed for hThreads", (DWORD)ulDosError, TRUE);
    }

    DWORD* dwThreadExitCodes = (DWORD*)calloc(dwThreadCount, sizeof(DWORD));
    if (dwThreadExitCodes == NULL)
    {
        ULONG ulDosError = 0;
        _get_doserrno(&ulDosError);
        free(hThreads);
        hThreads = NULL;
        HandleErrorAndFailW(L"calloc failed for dwThreadExitCodes", (DWORD)ulDosError, TRUE);
    }

    InitializeCriticalSection(&g_cs);

    for (DWORD i = 0; i < dwThreadCount; ++i)
    {
        hThreads[i] = (HANDLE)_beginthreadex(NULL, 0, RunThread, NULL, 0, NULL);
        if (hThreads[i] == NULL)
        {
            ULONG ulDosError = 0;
            _get_doserrno(&ulDosError);
            HandleErrorAndFailW(L"_beginthreadex failed", (DWORD)ulDosError, TRUE);
        }
    }

    wprintf(L"Processing...\n");

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

    const ULONGLONG ullExpectedResult = (ULONGLONG)dwThreadCount * (ULONGLONG)g_dwGoalCountPerThread;
    wprintf(L"Thread Count:    %lu\n"
            L"Expected Result: %llu\n"
            L"Actual Result:   %llu\n",
            dwThreadCount, ullExpectedResult, g_ullSharedData);

    if (g_ullSharedData == ullExpectedResult)
    {
        wprintf(L"Verification:    SUCCESS\n");
    }
    else
    {
        wprintf(L"Verification:    FAILED\n");
    }

    free(hThreads);
    hThreads = NULL;

    free(dwThreadExitCodes);
    dwThreadExitCodes = NULL;

    DeleteCriticalSection(&g_cs);

    return 0;
}