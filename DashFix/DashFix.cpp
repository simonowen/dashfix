// DashFix: https://github.com/simonowen/dashfix
//
// Source code released under MIT License.

#include <WinSDKVer.h>
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>  
#include <commctrl.h>
#include <string>
#include <vector>
#include <map>
#include <thread>

#include "SDL2\SDL.h"
#include "resource.h"


const auto APP_NAME{ TEXT("DashFix") };
const auto SETTINGS_KEY{ TEXT(R"(Software\SimonOwen\DashFix)") };
const auto STEAM_KEY{ TEXT(R"(Software\Valve\Steam)") };

HWND g_hDlg{ NULL };

////////////////////////////////////////////////////////////////////////////////

_Success_(return)
bool GetProcessId(
	_In_ const std::wstring &strProcess,
	_Out_ DWORD & dwProcessId,
	_In_opt_ DWORD dwExcludeProcessId = 0)
{
	bool ret{ false };
	dwProcessId = 0;

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot != INVALID_HANDLE_VALUE)
	{
		PROCESSENTRY32 pe{ sizeof(pe) };

		auto f = Process32First(hSnapshot, &pe);
		while (f)
		{
			if (!lstrcmpi(pe.szExeFile, strProcess.c_str()) &&
				pe.th32ProcessID != dwExcludeProcessId)
			{
				dwProcessId = pe.th32ProcessID;
				ret = true;
				break;
			}

			f = Process32Next(hSnapshot, &pe);
		}

		CloseHandle(hSnapshot);
	}

	return ret;
}

_Success_(return)
bool InjectRemoteThread(
	_In_ DWORD dwProcessId,
	_In_ const std::wstring &strDLL)
{
	bool ret{ false };

	auto hProcess = OpenProcess(
		PROCESS_ALL_ACCESS,
		FALSE,
		dwProcessId);

	if (hProcess != NULL)
	{
		auto pfnLoadLibrary = GetProcAddress(
			GetModuleHandle(TEXT("kernel32.dll")),
			"LoadLibraryW");

		auto uStringSize = (strDLL.length() + 1) * sizeof(strDLL[0]);
		auto pszRemoteString = VirtualAllocEx(
			hProcess,
			NULL,
			uStringSize,
			MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

		if (pszRemoteString)
		{
			WriteProcessMemory(hProcess, pszRemoteString, strDLL.c_str(), uStringSize, NULL);

			auto hThread = CreateRemoteThread(
				hProcess, NULL, 0,
				reinterpret_cast<LPTHREAD_START_ROUTINE>(pfnLoadLibrary),
				pszRemoteString,
				0,
				NULL);

			if (hThread != NULL)
			{
				WaitForSingleObject(hThread, INFINITE);
				CloseHandle(hThread);
				ret = true;
			}

			VirtualFreeEx(hProcess, pszRemoteString, 0, MEM_RELEASE);
		}

		WaitForSingleObject(hProcess, INFINITE);
		CloseHandle(hProcess);
	}

	return ret;
}


void InjectModules(
	_In_ const std::vector<std::wstring> &mods)
{
	WCHAR szEXE[MAX_PATH];
	GetModuleFileName(NULL, szEXE, _countof(szEXE));

	// The injection DLL is in the same directory as the EXE.
	std::wstring strDLL{ szEXE };
	strDLL = strDLL.substr(0, strDLL.rfind('\\'));
	strDLL += TEXT("\\inject.dll");

	std::vector<std::thread> threads{};
	for (auto &mod : mods)
	{
		std::thread([=]
		{
			// Loop forever watching
			for (;;)
			{
				DWORD dwProcessId;

				// Poll for process start, with 3 seconds between checks.
				if (!GetProcessId(mod, dwProcessId))
				{
					Sleep(3000);
				}
				// Inject the patch DLL into the process and wait for it to
				// terminate. 1 second between injection attempts.
				else if (!InjectRemoteThread(dwProcessId, strDLL))
				{
					Sleep(1000);
				}
			}
		}).detach();
	}
}

HMODULE LoadSDL2()
{
	WCHAR szPath[MAX_PATH]{};

	HKEY hkey;
	if (RegCreateKey(
		HKEY_CURRENT_USER,
		STEAM_KEY,
		&hkey) == ERROR_SUCCESS)
	{
		DWORD cchValue{ _countof(szPath) };
		DWORD dwType{ REG_SZ };

		RegQueryValueEx(
			hkey,
			TEXT("SteamPath"),
			NULL,
			&dwType,
			reinterpret_cast<LPBYTE>(szPath),
			&cchValue);

		RegCloseKey(hkey);
	}

	if (szPath[0])
	{
		wcscat_s(szPath, _countof(szPath), TEXT(R"(\SDL2.dll)"));
		return LoadLibrary(szPath);
	}

	return NULL;
}


void PopulateControllerList(
	_In_ HWND hListView)
{
	std::map<std::string, BOOL> joy_list;
	ListView_DeleteAllItems(hListView);

	// We need SDL2 for a connected controller list, and should be able to load
	// the one Steam itself uses.
	if (GetModuleHandle(TEXT("SDL2.dll")) || LoadSDL2())
	{
		SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
		int numControllers = SDL_NumJoysticks();

		for (int i = 0; i < numControllers; ++i)
		{
			auto pJoystick = SDL_JoystickOpen(i);
			if (pJoystick)
			{
				auto pJoyName = SDL_JoystickName(pJoystick);
				if (pJoyName && *pJoyName)
				{
					// Controller enabled by default.
					joy_list[pJoyName] = TRUE;
				}
			}

			SDL_JoystickClose(pJoystick);
		}

		SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
	}

	HKEY hkey;
	if (RegCreateKey(
		HKEY_CURRENT_USER,
		SETTINGS_KEY,
		&hkey) == ERROR_SUCCESS)
	{
		// Populate from settings stored in the registry.
		for (DWORD idx = 0; ; ++idx)
		{
			char szValue[256];
			DWORD dwType{ REG_DWORD };
			DWORD cchValue{ _countof(szValue) };
			DWORD dwData{ 0 }, cbData{ sizeof(dwData) };

			if (RegEnumValueA(
				hkey,
				idx,
				szValue, &cchValue,
				NULL,
				&dwType,
				reinterpret_cast<BYTE *>(&dwData), &cbData) != ERROR_SUCCESS)
			{
				break;
			}

			if (szValue[0])
				joy_list[szValue] = (dwData != 0);
		}

		RegCloseKey(hkey);
	}

	// Add the combined list to the listview control, with its checked status.
	for (auto &joy : joy_list)
	{
		LVFINDINFOA lfi{};
		lfi.flags = LVFI_STRING;
		lfi.psz = joy.first.c_str();
		auto index = SendMessage(hListView, LVM_FINDITEMA, 0, reinterpret_cast<LPARAM>(&lfi));

		if (index < 0)
		{
			LVITEMA lvi{};
			lvi.mask = LVIF_TEXT;
			lvi.pszText = const_cast<char *>(lfi.psz);
			index = SendMessage(hListView, LVM_INSERTITEMA, 0, reinterpret_cast<LPARAM>(&lvi));

			ListView_SetCheckState(hListView, index, !joy.second);
		}
	}
}

void SaveControllerList(
	_In_ HWND hListView)
{
	HKEY hkey;
	if (RegCreateKey(
		HKEY_CURRENT_USER,
		SETTINGS_KEY,
		&hkey) == ERROR_SUCCESS)
	{
		// Save list entries and checked status to the registry.
		for (int idx = 0; ; ++idx)
		{
			char szItem[MAX_PATH]{};

			LV_ITEMA li{};
			li.mask = LVIF_TEXT;
			li.iItem = idx;
			li.pszText = szItem;
			li.cchTextMax = _countof(szItem);

			if (!SendMessage(hListView, LVM_GETITEMA, 0, reinterpret_cast<LPARAM>(&li)))
				break;

			DWORD dwData = !ListView_GetCheckState(hListView, idx);
			RegSetValueExA(
				hkey,
				szItem,
				0,
				REG_DWORD,
				reinterpret_cast<LPBYTE>(&dwData),
				sizeof(dwData));
		}

		RegCloseKey(hkey);
	}

}

INT_PTR CALLBACK DialogProc(
	_In_ HWND hDlg,
	_In_ UINT uMsg,
	_In_ WPARAM wParam,
	_In_ LPARAM /*lParam*/)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			// Enable listbox items to have checkboxes.
			HWND hwndList = GetDlgItem(hDlg, IDL_CONTROLLERS);
			ListView_SetExtendedListViewStyle(hwndList, LVS_EX_CHECKBOXES);

			LVCOLUMN col{};
			col.mask = LVCF_WIDTH;
			col.cx = 300;
			ListView_InsertColumn(hwndList, 0, &col);

			// Populate the controller list from connected devices and the registry.
			PopulateControllerList(hwndList);

			return TRUE;
		}

		case WM_DESTROY:
			g_hDlg = NULL;
			return TRUE;

		case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
				case IDOK:
				{
					// Save the controller checkbox states to the registry.
					SaveControllerList(GetDlgItem(hDlg, IDL_CONTROLLERS));

					DestroyWindow(hDlg);
					return TRUE;
				}

				case IDCANCEL:
				{
					DestroyWindow(hDlg);
					return TRUE;
				}
			}
			break;
		}

		case WM_DEVICECHANGE:
		{
			// Refresh the list if a controller is hotplugged, saving any
			// modified entries from the existing list before it's updated.
			HWND hwndListView = GetDlgItem(hDlg, IDL_CONTROLLERS);
			SaveControllerList(hwndListView);
			PopulateControllerList(hwndListView);

			return TRUE;
		}
	}

	return FALSE;
}

void ShowGUI(
	_In_ HINSTANCE hInstance,
	_In_ int nCmdShow)
{
	INITCOMMONCONTROLSEX icce{ sizeof(icce), ICC_LISTVIEW_CLASSES };
	InitCommonControlsEx(&icce);

	g_hDlg = CreateDialog(
		hInstance, MAKEINTRESOURCE(IDD_CONTROLLERS), NULL, DialogProc);

	ShowWindow(g_hDlg, nCmdShow);
}

////////////////////////////////////////////////////////////////////////////////

int CALLBACK WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE /*hPrevInstance*/,
	_In_ LPSTR lpCmdLine,
	_In_ int nCmdShow)
{
	auto uRegMsg = RegisterWindowMessage(APP_NAME);

	// Check for a running instance.
	HANDLE hMutex = CreateMutex(NULL, TRUE, APP_NAME);
	if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS)
	{
		ReleaseMutex(hMutex);

		// Ask the existing instance to show the GUI.
		DWORD dwRecipients = BSM_APPLICATIONS;
		BroadcastSystemMessage(
			BSF_POSTMESSAGE,
			&dwRecipients,
			uRegMsg,
			0,
			0L);
	}
	else
	{
		// Inject the hook DLL into the known affected applications.
		InjectModules({ TEXT("steam.exe"), TEXT("vrdashboard.exe") });

		// Show the GUI if not launched with a parameter (startup mode).
		if (!lpCmdLine[0])
		{
			ShowGUI(hInstance, nCmdShow);
		}

		// Dummy window to listen for broadcast messages.
		CreateWindow(TEXT("static"), TEXT(""), 0, 0, 0, 0, 0, NULL, NULL, hInstance, 0L);

		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0))
		{
			// Show the GUI if asked to by another instance, and not already active.
			if (msg.message == uRegMsg && !g_hDlg)
			{
				ShowGUI(hInstance, SW_SHOWNORMAL);
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return 0;
}
