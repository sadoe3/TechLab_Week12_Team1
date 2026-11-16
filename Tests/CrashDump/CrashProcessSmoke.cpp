#include "Core/Windows/WindowsCrashReporting.h"
#include <iostream>
#include <thread>

__declspec(noinline) void CauseAccessViolation()
{
	volatile int* InvalidAddress = nullptr;
	*InvalidAddress = 42;
}

int wmain(int ArgumentCount, wchar_t** Arguments)
{
	if (ArgumentCount != 3)
		return ERROR_INVALID_PARAMETER;

	const DWORD StartupError = WindowsCrashReporting::Initialize(std::filesystem::absolute(Arguments[1]));
	if (StartupError != ERROR_SUCCESS)
	{
		std::cout << "StartupError=" << StartupError << std::endl;
		return StartupError;
	}

	const std::wstring Mode = Arguments[2];
	if (Mode == L"crash")
		CauseAccessViolation();
	else if (Mode == L"worker")
	{
		std::thread CrashThread(CauseAccessViolation);
		CrashThread.join();
	}

	WindowsCrashReporting::Shutdown();
	std::cout << "Normal shutdown" << std::endl;
	return 0;
}
