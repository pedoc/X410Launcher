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

static decltype(&MessageBoxW) RealMessageBoxW = MessageBoxW;
static decltype(&RegisterClassExW) RealRegisterClassExW = RegisterClassExW;

int WINAPI HookMessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType)
{
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
		frame.AddrPC.Offset = context.Rip;
		frame.AddrPC.Mode = AddrModeFlat;
		frame.AddrFrame.Offset = context.Rbp;
		frame.AddrFrame.Mode = AddrModeFlat;
		frame.AddrStack.Offset = context.Rsp;
		frame.AddrStack.Mode = AddrModeFlat;
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

			context.ContextFlags = CONTEXT_ALL;
			RtlRestoreContext(&context, nullptr);

			__assume(0);
		}
	}

	return RealMessageBoxW(hWnd, lpText, lpCaption, uType);
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

ATOM HookRegisterClassExW(const PWNDCLASSEXW pWndClassEx)
{
	if (wcscmp(pWndClassEx->lpszClassName, L"X410_RootWin") == 0)
	{
		g_OriginalWndProc = pWndClassEx->lpfnWndProc;
		pWndClassEx->lpfnWndProc = HookedWndProc;
	}

	return RealRegisterClassExW(pWndClassEx);
}

extern "C"
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
