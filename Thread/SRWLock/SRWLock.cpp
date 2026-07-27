#include <Windows.h>
#include <process.h>

#include "ErrorHelper.h"

SRWLOCK g_dataLock;
volatile ULONGLONG g_sharedData = 0;
volatile DWORD g_writeGoalCount = 1000000;
volatile DWORD g_readGoalCount = 500000;

UINT __stdcall RunWriterThread(PVOID param)
{
    for (DWORD i = 0; i < g_writeGoalCount; ++i)
    {
        AcquireSRWLockExclusive(&g_dataLock);
        ++g_sharedData;
        ReleaseSRWLockExclusive(&g_dataLock);
    }

    return 0;
}

UINT __stdcall RunReaderThread(PVOID param)
{
    ULONGLONG localValue = 0;

    for (DWORD i = 0; i < g_readGoalCount; ++i)
    {
        AcquireSRWLockShared(&g_dataLock);
        localValue = g_sharedData;
        ReleaseSRWLockShared(&g_dataLock);
    }

    return 0;
}

int main(void)
{
    SYSTEM_INFO systemInfo = { 0, };
    GetSystemInfo(&systemInfo);

    DWORD writerCount = 2;
    DWORD readerCount = systemInfo.dwNumberOfProcessors - writerCount;
    DWORD totalThreads = writerCount + readerCount;
    if (totalThreads > MAXIMUM_WAIT_OBJECTS)
    {
        totalThreads = MAXIMUM_WAIT_OBJECTS;
    }

    HANDLE* threads = (HANDLE*)calloc(totalThreads, sizeof(HANDLE));
    if (threads == NULL)
    {
        ULONG dosError = 0;
        _get_doserrno(&dosError);
        HandleErrorAndFail(L"calloc failed for threads", (DWORD)dosError, TRUE);
    }

    InitializeSRWLock(&g_dataLock);

    LOG_INFO(L"Initializing Threads... (Writers: %lu, Readers: %lu)", writerCount, readerCount);

    for (DWORD i = 0; i < writerCount; ++i)
    {
        threads[i] = (HANDLE)_beginthreadex(NULL, 0, RunWriterThread, NULL, 0, NULL);
        if (threads[i] == NULL)
        {
            ULONG dosError = 0;
            _get_doserrno(&dosError);
            HandleErrorAndFail(L"_beginthreadex (Writer) failed", (DWORD)dosError, TRUE);
        }
    }

    for (DWORD i = writerCount; i < totalThreads; ++i)
    {
        threads[i] = (HANDLE)_beginthreadex(NULL, 0, RunReaderThread, NULL, 0, NULL);
        if (threads[i] == NULL)
        {
            ULONG dosError = 0;
            _get_doserrno(&dosError);
            HandleErrorAndFail(L"_beginthreadex (Reader) failed", (DWORD)dosError, TRUE);
        }
    }

    LOG_INFO(L"Processing Tasks...");

    DWORD waitResult = WaitForMultipleObjects(totalThreads, threads, TRUE, INFINITE);
    if (waitResult == WAIT_FAILED)
    {
        HandleErrorAndFail(L"WaitForMultipleObjects failed", GetLastError(), FALSE);
    }

    for (DWORD i = 0; i < totalThreads; ++i)
    {
        CloseHandle(threads[i]);
    }

    const ULONGLONG expectedResult = (ULONGLONG)writerCount * (ULONGLONG)g_writeGoalCount;
    wprintf(L"\n--- Result ---\n"
            L"Writer Threads  : %lu\n"
            L"Reader Threads  : %lu\n"
            L"Expected Result : %llu\n"
            L"Actual Result   : %llu\n",
            writerCount, readerCount, expectedResult, g_sharedData);

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

    return 0;
}