#include <Windows.h>
#include <psapi.h>

#include "ErrorHelper.h"

#pragma comment(lib, "psapi.lib")

void PrintMemoryStats(LPCWSTR message)
{
    PROCESS_MEMORY_COUNTERS memoryCounters = { sizeof(memoryCounters) };

    if (!GetProcessMemoryInfo(GetCurrentProcess(), &memoryCounters, sizeof(memoryCounters)))
    {
        HandleErrorAndFail(L"Failed to get process memory information", GetLastError(), FALSE);
    }

    LOG_INFO(L"%s: %.4f MB", message, (double)memoryCounters.WorkingSetSize / (1024.0 * 1024.0));
}

int main(void)
{
    constexpr SIZE_T reserveSize = 1024ULL * 1024 * 1024;
    constexpr SIZE_T commitSize = reserveSize / 2;

    SYSTEM_INFO systemInfo = { 0, };
    GetSystemInfo(&systemInfo);
    const DWORD pageSize = systemInfo.dwPageSize;

    PrintMemoryStats(L"Initial State      ");

    LPVOID reservedArea = VirtualAlloc(NULL, reserveSize, MEM_RESERVE, PAGE_NOACCESS);
    if (reservedArea == NULL)
    {
        HandleErrorAndFail(L"Failed to reserve memory", GetLastError(), FALSE);
    }
    PrintMemoryStats(L"After MEM_RESERVE  ");

    LPVOID committedArea = VirtualAlloc(reservedArea, commitSize, MEM_COMMIT, PAGE_READWRITE);
    if (committedArea == NULL)
    {
        DWORD commitError = GetLastError();
        if (!VirtualFree(reservedArea, 0, MEM_RELEASE))
        {
            HandleErrorAndFail(L"Critical failure during handling commit error", GetLastError(), FALSE);
        }
        SetLastError(commitError);
        HandleErrorAndFail(L"Failed to commit memory", GetLastError(), FALSE);
    }
    PrintMemoryStats(L"After MEM_COMMIT   ");

    // [WARN] Access Violation Exception: offset < commitSize + 1
    for (SIZE_T offset = 0; offset < commitSize; offset += pageSize)
    {
        *((BYTE*)committedArea + offset) = 0xFF;
    }
    PrintMemoryStats(L"After Writing Data ");

    if (!VirtualFree(reservedArea, 0, MEM_RELEASE))
    {
        HandleErrorAndFail(L"Failed to release memory", GetLastError(), FALSE);
    }
    PrintMemoryStats(L"After MEM_RELEASE  ");

    reservedArea = NULL;
    committedArea = NULL;

    return 0;
}