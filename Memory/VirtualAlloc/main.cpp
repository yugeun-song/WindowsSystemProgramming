#include <Windows.h>
#include <intrin.h>
#include <psapi.h>
#include <stdio.h>

#pragma comment(lib, "psapi.lib")

[[noreturn]] void HandleErrorAndFailW(LPCWSTR pwszMessage, DWORD dwErrorCode)
{
    LPWSTR pwszSysMsg = nullptr;

    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                   dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&pwszSysMsg, 0, NULL);

    if (pwszSysMsg == nullptr)
    {
        fwprintf(stderr, L"%ls: %lu\n", pwszMessage, dwErrorCode);
    }
    else
    {
        fwprintf(stderr, L"%ls: %lu - %ls", pwszMessage, dwErrorCode, pwszSysMsg);
        LocalFree(pwszSysMsg);
    }

    if (IsDebuggerPresent())
    {
        __debugbreak();
    }

    ExitProcess(dwErrorCode);
}

void PrintMemoryStats(const char* pszMessage)
{
    PROCESS_MEMORY_COUNTERS pmc;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    {
        HandleErrorAndFailW(L"Failed to get process memory information");
    }

    printf("%s: %.4f MB\n", pszMessage, (double)pmc.WorkingSetSize / (1024 * 1024));
}

int main(void)
{
    constexpr SIZE_T cbReserveSize = 1024ULL * 1024 * 1024;
    constexpr SIZE_T cbActualAvailableSize = cbReserveSize / 2;

    SYSTEM_INFO si = { 0 };
    GetSystemInfo(&si);
    SIZE_T dwPageSize = si.dwPageSize;

    PrintMemoryStats("Initial State      ");

    LPVOID lpvReservedArea = VirtualAlloc(NULL, cbReserveSize, MEM_RESERVE, PAGE_NOACCESS);
    if (lpvReservedArea == nullptr)
    {
        HandleErrorAndFailW(L"Failed to reserve memory");
    }
    PrintMemoryStats("After MEM_RESERVE  ");

    LPVOID lpvCommittedArea = VirtualAlloc(lpvReservedArea, cbActualAvailableSize, MEM_COMMIT, PAGE_READWRITE);
    if (lpvCommittedArea == nullptr)
    {
        DWORD dwCommitError = GetLastError();
        if (!VirtualFree(lpvReservedArea, 0, MEM_RELEASE))
        {
            HandleErrorAndFailW(L"Critical failure during handling commit error");
        }
        SetLastError(dwCommitError);
        HandleErrorAndFailW(L"Failed to commit memory");
    }
    PrintMemoryStats("After MEM_COMMIT   ");

    // [WARN] Access Violation Exception: for (SIZE_T i = 0; i < cbActualAvailableSize + 1; i += dwPageSize)
    for (SIZE_T i = 0; i < cbActualAvailableSize; i += dwPageSize)
    {
        *((BYTE*)lpvCommittedArea + i) = 0xFF;
    }
    PrintMemoryStats("After Writing Data ");

    if (!VirtualFree(lpvReservedArea, 0, MEM_RELEASE))
    {
        HandleErrorAndFailW(L"Failed to release memory");
    }
    PrintMemoryStats("After MEM_RELEASE  ");

    lpvReservedArea = nullptr;
    lpvCommittedArea = nullptr;
    return 0;
}