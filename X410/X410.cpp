#include <iostream>
#include <limits.h>
#include <wchar.h>
#include <windows.h>
#include <winnt.h>

#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "dbghelp.lib")

extern "C" {
#include <dbghelp.h>
}

#include <detours/detours.h>

extern "C" void RestoreContextAndJump_x64(CONTEXT* context);
extern "C" void RestoreContextAndJump_arm64(CONTEXT* context);

static decltype(&MessageBoxW) RealMessageBoxW = MessageBoxW;
static decltype(&RegisterClassExW) RealRegisterClassExW = RegisterClassExW;

int WINAPI HookMessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType)
{
    static BOOL IsFirstMessage = TRUE;

    if (IsFirstMessage)
    {
        CONTEXT context = {}; // Important to initialize to zero
        context.ContextFlags = CONTEXT_FULL;
        
        // RtlCaptureContext is available for x64 and ARM64
        // For x86, you'd use a different approach or assembly to capture context.
        // However, your original code only targets x64 and ARM64 for the asm block.
        RtlCaptureContext(&context);

        DWORD machine;
        STACKFRAME64 frame = {};

        // SymInitializeW process handle: GetCurrentProcess() returns a pseudo-handle
        // that is fine for this use.
        if (!SymInitializeW(GetCurrentProcess(), NULL, TRUE))
        {
            // Handle SymInitializeW error if necessary, though the original didn't.
            // For this translation, we'll keep the original flow.
        }


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
        frame.AddrFrame.Offset = context.Fp; // Frame Pointer on ARM64
        frame.AddrFrame.Mode   = AddrModeFlat;
        frame.AddrStack.Offset = context.Sp;
        frame.AddrStack.Mode   = AddrModeFlat;
#else
#error Fill in frame info for this architecture!
#endif

        BOOL success = TRUE;
        SIZE_T szFrames = 3; // Number of frames to walk, as in original

        for (SIZE_T i = 0; i < szFrames; ++i)
        {
            // For StackWalk64, GetCurrentThread() is also a pseudo-handle, which is fine.
            if (!StackWalk64(
                machine,
                GetCurrentProcess(), GetCurrentThread(),
                &frame,
                &context, // Pass the captured context
                NULL,     // ReadMemoryRoutine (NULL for current process)
                SymFunctionTableAccess64, // SymFunctionTableAccess64
                SymGetModuleBase64,       // SymGetModuleBase64
                NULL      // SymRegisterFunctionEntryCallback (not usually needed for simple walks)
            ))
            {
                success = FALSE;
                break;
            }
            // If StackWalk64 returns FALSE and AddrPC.Offset is 0, it means end of stack.
            // This check can be useful but wasn't in the original code's loop condition.
            if (frame.AddrPC.Offset == 0) 
            {
                // Reached end of stack earlier than szFrames
                break;
            }
        }

        SymCleanup(GetCurrentProcess());

        if (success)
        {
            IsFirstMessage = FALSE;

#if defined(_M_X64)
            RestoreContextAndJump_x64(&context);
#elif defined(_M_ARM64)
            RestoreContextAndJump_arm64(&context);
#else
#error Restore context for this architecture!
#endif
            // The assembly functions will not return here.
            __assume(0); // Tell MSVC this code path is unreachable.
        }
    }

    // If IsFirstMessage was FALSE, or if (success) was FALSE, call the real function.
    // This is also the fallback if the hook setup for RealMessageBoxW is missing.
    if (RealMessageBoxW)
    {
        return RealMessageBoxW(hWnd, lpText, lpCaption, uType);
    }
    else
    {
        // Fallback if RealMessageBoxW is somehow NULL (should not happen in a proper hook)
        // This would likely cause issues or default behavior.
        // For robustness, you might want to call the actual Windows API MessageBoxW,
        // but that could lead to recursion if this IS the MessageBoxW being hooked.
        // This case indicates a setup error.
        // Defaulting to a simple return or a specific error code.
        return 0; // Or some error indicator
    }
}

static WNDPROC g_OriginalWndProc = nullptr;

static LRESULT CALLBACK HookedWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == 33178 && wParam == 50 && lParam == 0)
	{
		return (1u << 16) | USHRT_MAX;
	}

	return g_OriginalWndProc(hWnd, uMsg, wParam, lParam);
}

static ATOM HookRegisterClassExW(const PWNDCLASSEXW pWndClassEx)
{
	if (wcscmp(pWndClassEx->lpszClassName, L"X410_RootWin") == 0)
	{
		g_OriginalWndProc = pWndClassEx->lpfnWndProc;
		pWndClassEx->lpfnWndProc = HookedWndProc;
	}

	return RealRegisterClassExW(pWndClassEx);
}

static void WaitForDebugger()
{
	if (!IsDebuggerPresent())
	{
		std::cout << "Attach debugger to PID: " << GetCurrentProcessId() << std::endl;

		while (!IsDebuggerPresent())
		{
			Sleep(100); // 每 100ms 检查一次
		}

		std::cout << "Debugger attached." << std::endl;
	}

	DebugBreak();
}


extern "C"
__declspec(dllexport)
BOOL WINAPI
DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
	// if (dwReason == DLL_PROCESS_ATTACH)
	// {
	// 	MessageBoxW(NULL, L"DLL Injected!", L"Success", MB_OK);
	// }

	if (DetourIsHelperProcess())
	{
		return TRUE;
	}

	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		WaitForDebugger();
		
		DetourRestoreAfterWith();
		
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)RealMessageBoxW, (PVOID)HookMessageBoxW);
		DetourAttach(&(PVOID&)RealRegisterClassExW, (PVOID)HookRegisterClassExW);
		DetourTransactionCommit();
		break;
	case DLL_PROCESS_DETACH:
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourDetach(&(PVOID&)RealRegisterClassExW, (PVOID)HookRegisterClassExW);
		DetourDetach(&(PVOID&)RealMessageBoxW, (PVOID)HookMessageBoxW);
		DetourTransactionCommit();
		break;
	}

	return TRUE;
}
