#include <Windows.h>
#include <psapi.h>

#include "ErrorHelper.h"

#pragma comment(lib, "psapi.lib")

void PrintMemoryStats(LPCWSTR pwszMessage)
{
    PROCESS_MEMORY_COUNTERS pmc = {sizeof(pmc)};

    if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    {
        HandleErrorAndFailW(L"Failed to get process memory information", GetLastError(), FALSE);
    }

    wprintf(L"%s: %.4f MB\n", pwszMessage, (double)pmc.WorkingSetSize / (1024.0 * 1024.0));
}

int main(void)
{
    constexpr SIZE_T cbReserveSize = 1024ULL * 1024 * 1024;
    constexpr SIZE_T cbActualAvailableSize = cbReserveSize / 2;

    SYSTEM_INFO si = {0};
    GetSystemInfo(&si);
    const DWORD dwPageSize = si.dwPageSize;

    PrintMemoryStats(L"Initial State      ");

    LPVOID lpvReservedArea = VirtualAlloc(NULL, cbReserveSize, MEM_RESERVE, PAGE_NOACCESS);
    if (lpvReservedArea == NULL)
    {
        HandleErrorAndFailW(L"Failed to reserve memory", GetLastError(), FALSE);
    }
    PrintMemoryStats(L"After MEM_RESERVE  ");

    LPVOID lpvCommittedArea = VirtualAlloc(lpvReservedArea, cbActualAvailableSize, MEM_COMMIT, PAGE_READWRITE);
    if (lpvCommittedArea == NULL)
    {
        DWORD dwCommitError = GetLastError();
        if (!VirtualFree(lpvReservedArea, 0, MEM_RELEASE))
        {
            HandleErrorAndFailW(L"Critical failure during handling commit error", GetLastError(), FALSE);
        }
        SetLastError(dwCommitError);
        HandleErrorAndFailW(L"Failed to commit memory", GetLastError(), FALSE);
    }
    PrintMemoryStats(L"After MEM_COMMIT   ");

    // [WARN] Access Violation Exception: cbOffset < cbActualAvailableSize + 1
    for (SIZE_T cbOffset = 0; cbOffset < cbActualAvailableSize; cbOffset += dwPageSize)
    {
        *((BYTE*)lpvCommittedArea + cbOffset) = 0xFF;
    }
    PrintMemoryStats(L"After Writing Data ");

    if (!VirtualFree(lpvReservedArea, 0, MEM_RELEASE))
    {
        HandleErrorAndFailW(L"Failed to release memory", GetLastError(), FALSE);
    }
    PrintMemoryStats(L"After MEM_RELEASE  ");

    lpvReservedArea = NULL;
    lpvCommittedArea = NULL;

    return 0;
}