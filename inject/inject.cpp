#include <WinSDKVer.h>
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>


void PatchFunctions(
	_In_ LPCWSTR pszModule,
	_In_ const std::vector<std::string> &fns)
{
	HMODULE hModule = GetModuleHandle(pszModule);
	if (hModule != NULL)
	{
		for (auto &fn : fns)
		{
			auto pfn = GetProcAddress(hModule, fn.c_str());
			if (pfn)
			{
				const BYTE abCode[]{
					0x33, 0xc0,		// xor eax, eax
					0xc3			// ret
				};

				WriteProcessMemory(
					GetCurrentProcess(),
					pfn,
					abCode,
					sizeof(abCode),
					NULL);
			}
		}
	}
}

BOOL APIENTRY DllMain(
	_In_ HMODULE /*hinstDLL*/,
	_In_ DWORD  fdwReason,
	_In_ LPVOID /*lpvReserved*/)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		PatchFunctions( TEXT("SDL2.dll"), { "SDL_JoystickGetAxis" } );
		return FALSE;
	}

	return TRUE;
}
