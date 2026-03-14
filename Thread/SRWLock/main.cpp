#include <Windows.h>
#include <process.h>

#include "ErrorHelper.h"

SRWLOCK g_srwLock;
volatile ULONGLONG g_ullSharedData = 0;
volatile DWORD g_dwWriteGoalCount = 1000000;
volatile DWORD g_dwReadGoalCount = 500000;

UINT __stdcall RunWriterThread(PVOID pParam)
{
    for (DWORD i = 0; i < g_dwWriteGoalCount; ++i)
    {
        AcquireSRWLockExclusive(&g_srwLock);
        ++g_ullSharedData;
        ReleaseSRWLockExclusive(&g_srwLock);
    }

    return 0;
}

UINT __stdcall RunReaderThread(PVOID pParam)
{
    ULONGLONG ullLocalValue = 0;

    for (DWORD i = 0; i < g_dwReadGoalCount; ++i)
    {
        AcquireSRWLockShared(&g_srwLock);
        ullLocalValue = g_ullSharedData;
        ReleaseSRWLockShared(&g_srwLock);
    }

    return 0;
}

int main(void)
{
    SYSTEM_INFO sysInfo = { 0, };
    GetSystemInfo(&sysInfo);

    DWORD dwWriterCount = 2;
    DWORD dwReaderCount = sysInfo.dwNumberOfProcessors - dwWriterCount;
    DWORD dwTotalThreads = dwWriterCount + dwReaderCount;
    if (dwTotalThreads > MAXIMUM_WAIT_OBJECTS)
    {
        dwTotalThreads = MAXIMUM_WAIT_OBJECTS;
    }

    HANDLE* hThreads = (HANDLE*)calloc(dwTotalThreads, sizeof(HANDLE));
    if (hThreads == NULL)
    {
        ULONG ulDosError = 0;
        _get_doserrno(&ulDosError);
        HandleErrorAndFailW(L"calloc failed for hThreads", (DWORD)ulDosError, TRUE);
    }

    InitializeSRWLock(&g_srwLock);

    wprintf(L"Initializing Threads... (Writers: %lu, Readers: %lu)\n", dwWriterCount, dwReaderCount);

    for (DWORD i = 0; i < dwWriterCount; ++i)
    {
        hThreads[i] = (HANDLE)_beginthreadex(NULL, 0, RunWriterThread, NULL, 0, NULL);
        if (hThreads[i] == NULL)
        {
            ULONG ulDosError = 0;
            _get_doserrno(&ulDosError);
            HandleErrorAndFailW(L"_beginthreadex (Writer) failed", (DWORD)ulDosError, TRUE);
        }
    }

    for (DWORD i = dwWriterCount; i < dwTotalThreads; ++i)
    {
        hThreads[i] = (HANDLE)_beginthreadex(NULL, 0, RunReaderThread, NULL, 0, NULL);
        if (hThreads[i] == NULL)
        {
            ULONG ulDosError = 0;
            _get_doserrno(&ulDosError);
            HandleErrorAndFailW(L"_beginthreadex (Reader) failed", (DWORD)ulDosError, TRUE);
        }
    }

    wprintf(L"Processing Tasks...\n");

    DWORD dwWaitResult = WaitForMultipleObjects(dwTotalThreads, hThreads, TRUE, INFINITE);
    if (dwWaitResult == WAIT_FAILED)
    {
        HandleErrorAndFailW(L"WaitForMultipleObjects failed", GetLastError(), FALSE);
    }

    for (DWORD i = 0; i < dwTotalThreads; ++i)
    {
        CloseHandle(hThreads[i]);
    }

    const ULONGLONG ullExpectedResult = (ULONGLONG)dwWriterCount * (ULONGLONG)g_dwWriteGoalCount;
    wprintf(L"\n--- Result ---\n"
            L"Writer Threads  : %lu\n"
            L"Reader Threads  : %lu\n"
            L"Expected Result : %llu\n"
            L"Actual Result   : %llu\n",
            dwWriterCount, dwReaderCount, ullExpectedResult, g_ullSharedData);

    if (g_ullSharedData == ullExpectedResult)
    {
        wprintf(L"Verification    : SUCCESS\n");
    }
    else
    {
        wprintf(L"Verification    : FAILED\n");
    }

    free(hThreads);
    hThreads = NULL;

    return 0;
}