#include <limits.h>
#include <wchar.h>
#include <windows.h>
#include <winnt.h>

extern "C"
{

#include <dbghelp.h>

}

#include <detours.h>

static decltype(&MessageBoxW) RealMessageBoxW = MessageBoxW;
static decltype(&RegisterClassExW) RealRegisterClassExW = RegisterClassExW;

int WINAPI HookMessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType)
{
    // return RealMessageBoxW(hWnd, lpText, lpCaption, uType);

    static BOOL IsFirstMessage = TRUE;

    if (IsFirstMessage)
    {
        CONTEXT context = {};
        context.ContextFlags = CONTEXT_FULL;
        RtlCaptureContext(&context);

        DWORD machine;
        STACKFRAME64 frame = {};

        SymInitializeW(GetCurrentProcess(), NULL, TRUE);

#if defined(_M_X64)
        machine = IMAGE_FILE_MACHINE_AMD64;
        frame.AddrPC.Offset    = context.Rip;
        frame.AddrPC.Mode      = AddrModeFlat;
        frame.AddrFrame.Offset = context.Rbp;
        frame.AddrFrame.Mode   = AddrModeFlat;
        frame.AddrStack.Offset = context.Rsp;
        frame.AddrStack.Mode   = AddrModeFlat;
#elif defined(_M_ARM64)
        machine = IMAGE_FILE_MACHINE_ARM64;
        frame.AddrPC.Offset    = context.Pc;
        frame.AddrPC.Mode      = AddrModeFlat;
        frame.AddrFrame.Offset = context.Fp;
        frame.AddrFrame.Mode   = AddrModeFlat;
        frame.AddrStack.Offset = context.Sp;
        frame.AddrStack.Mode   = AddrModeFlat;
#else
#error Fill in frame info for this architecture!
#endif

        BOOL success = TRUE;

        SIZE_T szFrames = 3;
        for (SIZE_T i = 0; i < szFrames; ++i)
        {
            if (!StackWalk64(
                machine,
                GetCurrentProcess(), GetCurrentThread(),
                &frame,
                &context,
                NULL, NULL, NULL, NULL
            ))
            {
                success = FALSE;
                break;
            }
        }

        SymCleanup(GetCurrentProcess());

        if (success)
        {
            IsFirstMessage = FALSE;

#if defined(_M_X64)
            // Save and restore register states using the MSVC way
            // Because msvc does not support x64 inline assembly
            CONTEXT savedContext = context;
            
            savedContext.Rbx = context.Rbx;
            savedContext.Rsi = context.Rsi;
            savedContext.Rdi = context.Rdi;
            savedContext.R12 = context.R12;
            savedContext.R13 = context.R13;
            savedContext.R14 = context.R14;
            savedContext.R15 = context.R15;
            savedContext.Rsp = context.Rsp;
            savedContext.Rip = context.Rip;
            
            // Using SEH to perform context switches
            __try {
                // Set the return value
                savedContext.Rax = 1;
                
                // Use RtlRestoreContext to restore the context
                RtlRestoreContext(&savedContext, NULL);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
                // If an exception occurs, return the original MessageBox
                return RealMessageBoxW(hWnd, lpText, lpCaption, uType);
            }
#elif defined(_M_ARM64)
            // Save and restore register states using the MSVC way
            // Because msvc does not support arm64 inline assembly
            CONTEXT savedContext = context;
            
            savedContext.X19 = context.X19;
            savedContext.X20 = context.X20;
            savedContext.X21 = context.X21;
            savedContext.X22 = context.X22;
            savedContext.X23 = context.X23;
            savedContext.X24 = context.X24;
            savedContext.X25 = context.X25;
            savedContext.X26 = context.X26;
            savedContext.X27 = context.X27;
            savedContext.X28 = context.X28;
            savedContext.Fp = context.Fp;  // X29
            savedContext.Lr = context.Lr;  // X30
            
            // Using SEH to perform context switches
            __try {
                // Set the return value
                savedContext.X0 = 1;
                
                // Use RtlRestoreContext to restore the context
                RtlRestoreContext(&savedContext, NULL);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
                // If an exception occurs, return the original MessageBox
                return RealMessageBoxW(hWnd, lpText, lpCaption, uType);
            }
#else
#error Restore context for this architecture!
#endif

            // Replace __builtin_unreachable() with a MSVC compatible implementation
            __assume(0);
        }
    }

    return RealMessageBoxW(hWnd, lpText, lpCaption, uType);
}

ATOM HookRegisterClassExW(const PWNDCLASSEXW pWndClassEx)
{
    if (wcscmp(pWndClassEx->lpszClassName, L"X410_RootWin") == 0)
    {
        static WNDPROC pfnOriginalWndProc = pWndClassEx->lpfnWndProc;

        constexpr auto fnNewWndProc =
            [](HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) -> LRESULT
        {
            if (uMsg == 33178 && wParam == 50 && lParam == 0)
            {
                return (1u << 16) | USHRT_MAX;
            }

            return pfnOriginalWndProc(hWnd, uMsg, wParam, lParam);
        };

        pWndClassEx->lpfnWndProc = fnNewWndProc;
    }

    return RealRegisterClassExW(pWndClassEx);
}

extern "C"
__declspec(dllexport)
BOOL WINAPI
DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
    if (DetourIsHelperProcess())
    {
        return TRUE;
    }

    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
            DetourRestoreAfterWith();

            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(&(PVOID &)RealMessageBoxW, (PVOID)HookMessageBoxW);
            DetourAttach(&(PVOID &)RealRegisterClassExW, (PVOID)HookRegisterClassExW);
            DetourTransactionCommit();
        break;
        case DLL_PROCESS_DETACH:
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourDetach(&(PVOID &)RealRegisterClassExW, (PVOID)HookRegisterClassExW);
            DetourDetach(&(PVOID &)RealMessageBoxW, (PVOID)HookMessageBoxW);
            DetourTransactionCommit();
        break;
    }

    return TRUE;
}
