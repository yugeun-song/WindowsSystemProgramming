#include <Windows.h>
#include <process.h>

#include "ErrorHelper.h"

#define GOAL_COUNT_PER_THREAD 3000000

CRITICAL_SECTION g_counterLock;
ULONGLONG g_sharedData = 0;

UINT __stdcall RunThread(PVOID param)
{
    for (DWORD i = 0; i < GOAL_COUNT_PER_THREAD; ++i)
    {
        EnterCriticalSection(&g_counterLock);
        ++g_sharedData;
        LeaveCriticalSection(&g_counterLock);
    }
    return 0;
}

int main(void)
{
    SYSTEM_INFO systemInfo = { 0, };
    GetSystemInfo(&systemInfo);
    DWORD threadCount = systemInfo.dwNumberOfProcessors;

    if (threadCount > MAXIMUM_WAIT_OBJECTS)
    {
        threadCount = MAXIMUM_WAIT_OBJECTS;
    }

    HANDLE* threads = (HANDLE*)calloc(threadCount, sizeof(HANDLE));
    if (threads == NULL)
    {
        ULONG dosError = 0;
        _get_doserrno(&dosError);
        HandleErrorAndFail(L"calloc failed for threads", (DWORD)dosError, TRUE);
    }

    DWORD* threadExitCodes = (DWORD*)calloc(threadCount, sizeof(DWORD));
    if (threadExitCodes == NULL)
    {
        ULONG dosError = 0;
        _get_doserrno(&dosError);
        free(threads);
        threads = NULL;
        HandleErrorAndFail(L"calloc failed for threadExitCodes", (DWORD)dosError, TRUE);
    }

    InitializeCriticalSection(&g_counterLock);

    for (DWORD i = 0; i < threadCount; ++i)
    {
        threads[i] = (HANDLE)_beginthreadex(NULL, 0, RunThread, NULL, 0, NULL);
        if (threads[i] == NULL)
        {
            ULONG dosError = 0;
            _get_doserrno(&dosError);
            HandleErrorAndFail(L"_beginthreadex failed", (DWORD)dosError, TRUE);
        }
    }

    LOG_INFO(L"Processing...");

    DWORD waitResult = WaitForMultipleObjects(threadCount, threads, TRUE, INFINITE);
    if (waitResult == WAIT_FAILED)
    {
        HandleErrorAndFail(L"WaitForMultipleObjects failed", GetLastError(), FALSE);
    }

    for (DWORD i = 0; i < threadCount; ++i)
    {
        if (!GetExitCodeThread(threads[i], &(threadExitCodes[i])))
        {
            HandleErrorAndFail(L"GetExitCodeThread failed", GetLastError(), FALSE);
        }

        if (!CloseHandle(threads[i]))
        {
            HandleErrorAndFail(L"CloseHandle failed", GetLastError(), FALSE);
        }
    }

    const ULONGLONG expectedResult = (ULONGLONG)threadCount * (ULONGLONG)GOAL_COUNT_PER_THREAD;
    wprintf(L"Thread Count    : %lu\n"
            L"Expected Result : %llu\n"
            L"Actual Result   : %llu\n",
            threadCount, expectedResult, g_sharedData);

    if (g_sharedData == expectedResult)
    {
        wprintf(L"Verification    : SUCCESS\n");
    }
    else
    {
        wprintf(L"Verification    : FAILED\n");
    }

    free(threads);
    threads = NULL;

    free(threadExitCodes);
    threadExitCodes = NULL;

    DeleteCriticalSection(&g_counterLock);

    return 0;
}