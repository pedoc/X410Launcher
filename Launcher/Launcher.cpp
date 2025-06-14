#include <Windows.h>
#include <detours/detours.h>
#include <algorithm>
#include <iterator>
#include <string>

// Helper function to convert LPCWSTR to LPCSTR
std::string ConvertWideToNarrow(LPCWSTR wideString) {
    int narrowStringLength = WideCharToMultiByte(CP_ACP, 0, wideString, -1, NULL, 0, NULL, NULL);
    if (narrowStringLength == 0) {
        return std::string();
    }
    std::string narrowString(narrowStringLength, '\0');
    WideCharToMultiByte(CP_ACP, 0, wideString, -1, &narrowString[0], narrowStringLength, NULL, NULL);
    return narrowString;
}

extern "C"
__declspec(dllexport)
BOOL
StartProcessPreloadedW(
    LPCWSTR lpApplicationName,
    LPCWSTR lpDllName
)
{
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    // Convert LPCWSTR lpDllName to LPCSTR
    std::string narrowDllName = ConvertWideToNarrow(lpDllName);

    // Use the converted narrow string
    BOOL result = DetourCreateProcessWithDllW(
        lpApplicationName,
        NULL,           // Command line
        NULL,           // Process attributes
        NULL,           // Thread attributes
        FALSE,          // Inherit handles
        0,              // Creation flags
        NULL,           // Environment
        NULL,           // Current directory
        &si,
        &pi,
        narrowDllName.c_str(), // DLL name as LPCSTR
        NULL            // Reserved
    );

    if (result)
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    return result;
}