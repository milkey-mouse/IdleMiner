#include <SDKDDKVer.h>

#include <stdio.h>
#include <tchar.h>

#include <windows.h>
#include <WtsApi32.h>

#include <string>
#include <sstream>

// Hidden window class name
static PCWSTR s_windowClassName = L"IdleMiner";

static bool s_connectedToConsole = !GetSystemMetrics(SM_REMOTESESSION);

static HWND hwnd = NULL;

static PWSTR s_minerArgs;
static PROCESS_INFORMATION s_minerPI;
static bool s_minerRunning = false;

static constexpr UINT_PTR TIMER_ID = 1;
static bool s_timerRunning = false;

static HANDLE s_lockStateChangedEvent = INVALID_HANDLE_VALUE;

// Show a message box for any errors.
static void DisplayLastError()
{
	DWORD errorCode = GetLastError();
	PWSTR errorString = nullptr;

	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, errorCode, 0, reinterpret_cast<PWSTR>(&errorString), 0, nullptr);
	MessageBoxW(HWND_DESKTOP, errorString, nullptr, MB_ICONERROR);
	LocalFree(reinterpret_cast<HLOCAL>(errorString));
}

// Handle attaching and reattaching to the console.
static void AttachToConsole()
{
	if (s_connectedToConsole)
	{

		if (s_timerRunning)
		{
			KillTimer(hwnd, TIMER_ID);
			s_timerRunning = false;
		}
		if (s_minerRunning)
		{
			if (WaitForSingleObject(s_minerPI.hProcess, 0) != WAIT_TIMEOUT)
			{
				// already dead
			}
			else if (AttachConsole(s_minerPI.dwProcessId))
			{
				SetConsoleCtrlHandler(NULL, true);
				GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
				WaitForSingleObject(s_minerPI.hProcess, 5000);
				if (WaitForSingleObject(s_minerPI.hProcess, 0) != WAIT_TIMEOUT)
				{
					TerminateProcess(s_minerPI.hProcess, 0);
					WaitForSingleObject(s_minerPI.hProcess, INFINITE);
				}
				FreeConsole();
				SetConsoleCtrlHandler(NULL, false);
			}
			else
			{
				TerminateProcess(s_minerPI.hProcess, 0);
				WaitForSingleObject(s_minerPI.hProcess, INFINITE);
			}

			CloseHandle(s_minerPI.hProcess);
			CloseHandle(s_minerPI.hThread);

			s_minerRunning = false;
		}
	}
}

// Handle detaching from the console.
static void DetachFromConsole()
{
	if (s_connectedToConsole)
	{
		SetTimer(hwnd, TIMER_ID, 60000, NULL);
		s_timerRunning = true;
	}
}

static void StartMining()
{
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&s_minerPI, sizeof(s_minerPI));
	if (!CreateProcessW(NULL, s_minerArgs, NULL, NULL, FALSE, 0, NULL, NULL, &si, &s_minerPI))
	{
		DisplayLastError();
	}
	else
	{
		s_minerRunning = true;
	}
}

// Process window messages to the hidden window.
static LRESULT CALLBACK HiddenWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
		WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION);
		break;

	case WM_DESTROY:
		WTSUnRegisterSessionNotification(hwnd);
		DetachFromConsole();
		PostQuitMessage(0);
		break;

	case WM_WTSSESSION_CHANGE:
		switch (wParam)
		{
		case WTS_CONSOLE_CONNECT:
			s_connectedToConsole = true;
			AttachToConsole();
			break;

		case WTS_CONSOLE_DISCONNECT:
			DetachFromConsole();
			s_connectedToConsole = false;
			break;

		case WTS_SESSION_LOCK:
			DetachFromConsole();
			break;

		case WTS_SESSION_UNLOCK:
			AttachToConsole();
			break;

		default:
			break;
		}

		break;

	case WM_TIMER:
		if (wParam == TIMER_ID)
		{
			KillTimer(hwnd, TIMER_ID);
			s_timerRunning = false;
			StartMining();
		}

		break;

	default:
		return DefWindowProcW(hwnd, message, wParam, lParam);
	}

	return 0;
}

// Run the program.
int WINAPI wWinMain(HINSTANCE hinstExe, HINSTANCE hinstPrev, PWSTR wzCmdLine, int nCmdShow)
{
	// TODO: Run in an interactive service? Tough to deal with user switching. Need to handle WM_DISPLAYCHANGE with a hidden top-level window.
	// Windows Service C++ sample: https://www.codeproject.com/articles/499465/simple-windows-service-in-cplusplus

	int argCount;
	LocalFree(CommandLineToArgvW(wzCmdLine, &argCount));

	if (argCount <= 1)
	{
		return 1;
	}
	else
	{
		s_minerArgs = wzCmdLine;
	}

	// Create the hidden window.
	WNDCLASSEXW windowClass = {
		sizeof(windowClass), // cbSize
		0,					 // style
		HiddenWindowProc,	// lpfnWndProc
		0,					 // cbClsExtra
		0,					 // cbWndExtra
		hinstExe,			 // hInstance
		NULL,				 // hIcon
		NULL,				 // hCursor
		NULL,				 // hbrBackground
		nullptr,			 // lpszMenuName
		s_windowClassName,   // lpszClassName
		NULL				 // hIconSm
	};

	if (!RegisterClassExW(&windowClass))
	{
		DisplayLastError();
		return 1;
	}

	s_lockStateChangedEvent = CreateEvent(NULL, FALSE, TRUE, L"Global\IdleMiner-LoggedIn");

	hwnd = CreateWindowExW(0, s_windowClassName, nullptr, 0, 0, 0, 0, 0, HWND_DESKTOP, NULL, hinstExe, nullptr);

	// Start processing messages/timers.
	AttachToConsole();

	BOOL result;
	MSG msg;

	while (result = GetMessageW(&msg, NULL, 0, 0))
	{
		if (-1 == result)
		{
			DisplayLastError();
			return 2;
		}

		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	return msg.wParam;
}
