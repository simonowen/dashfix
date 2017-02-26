// DashFix: https://github.com/simonowen/dashfix
//
// Source code released under MIT License.

#include <WinSDKVer.h>
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include <map>

#include <SDL2\SDL.h>
#pragma comment(lib, "SDL2.lib")

const auto APP_NAME{ TEXT("DashFix") };
const auto SETTINGS_KEY{ TEXT(R"(Software\SimonOwen\DashFix)") };


typedef Sint16 (SDLCALL *PFNSDL_JOYSTICKGETAXIS)(SDL_Joystick *, int);
typedef const char * (SDLCALL *PFNSDL_JOYSTICKNAME)(SDL_Joystick *);

PFNSDL_JOYSTICKGETAXIS pfnOrig_SDL_JoystickGetAxis;
PFNSDL_JOYSTICKGETAXIS g_pfnJoystickGetAxis;
PFNSDL_JOYSTICKNAME g_pfnJoystickName;

HKEY g_hkey;
HANDLE g_hEvent;


Sint16 SDLCALL Hooked_SDL_JoystickGetAxis(SDL_Joystick * joystick, int axis)
{
	static std::map<SDL_Joystick *, BOOL> g_mapSettings;

	// The event is signalled when the GUI has changed a controller setting,
	// or on the first pass through this loop as it's created signalled.
	if (WaitForSingleObject(g_hEvent, 0) != WAIT_TIMEOUT)
	{
		// Clear out the settings cache.
		g_mapSettings.clear();

		// Start a new watch for registry changes.
		RegNotifyChangeKeyValue(
			g_hkey,
			FALSE,
			REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_THREAD_AGNOSTIC,
			g_hEvent,
			TRUE);
	}

	// Look for cached settings for this controller.
	auto it = g_mapSettings.find(joystick);
	if (it == g_mapSettings.end())
	{
		DWORD dwType = REG_DWORD;
		DWORD dwData = 1;	// not blocked
		DWORD cbData{ sizeof(dwData) };

		// Look up the controller name.
		auto pJoystickName = g_pfnJoystickName(joystick);
		if (pJoystickName)
		{
			// Read the current registry setting for this controller.
			// Unrecognised controllers will be allowed by default.
			RegQueryValueExA(
				g_hkey,
				pJoystickName,
				NULL,
				&dwType,
				reinterpret_cast<LPBYTE>(&dwData),
				&cbData);
		}

		g_mapSettings[joystick] = (dwData != 0);
	}

	// If this controller is to be blocked, return zero axis movement.
	if (g_mapSettings[joystick] == FALSE)
	{
		return 0;
	}

	// Chain unblocked controllers through to the original handler.
	return pfnOrig_SDL_JoystickGetAxis(joystick, axis);
}

void InstallHook()
{
	// The original target is an indirect call, which means it points to an
	// address holding the address of the function. We need to do the same.
	static auto pfnHookedFunction = Hooked_SDL_JoystickGetAxis;
	auto ppfnHookedFunction = &pfnHookedFunction;

	// The address to patch is 6 bytes into the target function preamble.
	auto pb = reinterpret_cast<uint8_t *>(g_pfnJoystickGetAxis);
	auto ppfn = reinterpret_cast<PFNSDL_JOYSTICKGETAXIS **>(pb + 6);

	// Ensure the hook point is compatible and not already installed.
	if (memcmp(pb, "\x55\x8b\xec\x5d\xff\x25", 6) == 0 &&
		*ppfn != &pfnHookedFunction)
	{
		// Save the original function address.
		pfnOrig_SDL_JoystickGetAxis = **ppfn;

		// Write our own address to intercept calls to it.
		WriteProcessMemory(
			GetCurrentProcess(),
			ppfn,
			&ppfnHookedFunction,
			sizeof(pfnHookedFunction),
			NULL);
	}

}

BOOL APIENTRY DllMain(
	_In_ HMODULE /*hinstDLL*/,
	_In_ DWORD  fdwReason,
	_In_ LPVOID /*lpvReserved*/)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
#ifdef _DEBUG
		// Convenient delay point to allow us to attach the debugger.
		MessageBox(NULL, TEXT("In DllMain for inject.dll"), APP_NAME, MB_OK | MB_ICONINFORMATION);
#endif

		// The target process will have loaded SDL2 already, so find it.
		auto hmodSDL2 = LoadLibrary(TEXT("SDL2.dll"));
		if (hmodSDL2)
		{
			// One function to hook, one to use to find the controller name.
			g_pfnJoystickGetAxis = reinterpret_cast<PFNSDL_JOYSTICKGETAXIS>(
				GetProcAddress(hmodSDL2, "SDL_JoystickGetAxis"));
			g_pfnJoystickName = reinterpret_cast<PFNSDL_JOYSTICKNAME>(
				GetProcAddress(hmodSDL2, "SDL_JoystickName"));
		}

		// Event used for registry change monitoring, signalled by default.
		g_hEvent = CreateEvent(NULL, FALSE, TRUE, NULL);

		// Registry containing settings, written to by the GUI.
		auto status = RegCreateKey(
			HKEY_CURRENT_USER,
			SETTINGS_KEY,
			&g_hkey);

		// Ensure we have everything needed before installing the hook.
		if (g_pfnJoystickGetAxis && g_pfnJoystickName && g_hEvent && !status)
		{
			InstallHook();
		}
		else
		{
			MessageBox(
				NULL,
				TEXT("Failed to install hook!"),
				APP_NAME,
				MB_ICONEXCLAMATION | MB_OK);
		}
	}

	return TRUE;
}
