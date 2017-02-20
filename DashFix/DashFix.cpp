#include <WinSDKVer.h>
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>  
#include <string>
#include <vector>
#include <thread>


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


void PatchModules(
	_In_ const std::vector<std::wstring> &mods)
{
	WCHAR szEXE[MAX_PATH];
	GetModuleFileName(NULL, szEXE, ARRAYSIZE(szEXE));

	// The injection DLL is in the same directory as the EXE.
	std::wstring strDLL{szEXE};
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


int CALLBACK WinMain(
	_In_ HINSTANCE /*hInstance*/,
	_In_opt_ HINSTANCE /*hPrevInstance*/,
	_In_ LPSTR /*lpCmdLine*/,
	_In_ int /*nCmdShow*/)
{
	HANDLE hMutex = CreateMutex(NULL, TRUE, TEXT("DashFixMutex"));
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		ReleaseMutex(hMutex);

		if (MessageBox( NULL,
			TEXT("Program is already running. Do you want to close it?"),
			TEXT("DashFix"), MB_ICONQUESTION | MB_YESNO) == IDYES)
		{
			DWORD dwProcessId{0};

			if (GetProcessId( TEXT("DashFix.exe"), dwProcessId, GetCurrentProcessId()))
			{
				HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessId);
				if (hProcess)
				{
					TerminateProcess(hProcess, 0);
					CloseHandle(hProcess);
				}
			}
		}
	}
	else
	{
		// So far only 2 modules seem to need patching.
		PatchModules( { TEXT("steam.exe"), TEXT("vrdashboard.exe") } );

		MSG msg;
		while (GetMessage(&msg, NULL, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return 0;
}
