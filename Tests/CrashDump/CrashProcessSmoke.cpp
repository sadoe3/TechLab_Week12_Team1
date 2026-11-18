#include "Core/Windows/WindowsCrashReporting.h"
#include <iostream>
#include <thread>
#include <cstdlib>
#include <exception>
#include <csignal>
#include <crtdbg.h>

__declspec(noinline) void CauseAccessViolation()
{
	volatile int* InvalidAddress = nullptr;
	*InvalidAddress = 42;
}

int wmain(int ArgumentCount, wchar_t** Arguments)
{
	if (ArgumentCount != 3)
		return ERROR_INVALID_PARAMETER;

	const auto OriginalInvalidParameterHandler = _get_invalid_parameter_handler();
	const auto OriginalPureCallHandler = _get_purecall_handler();
	const auto OriginalTerminateHandler = std::get_terminate();
	const auto OriginalAbortHandler = std::signal(SIGABRT, SIG_DFL);
	std::signal(SIGABRT, OriginalAbortHandler);
	const auto OriginalAbortBehavior = _set_abort_behavior(0, 0);
#if defined(_DEBUG)
	const int OriginalAssertReportMode = _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_REPORT_MODE);
#endif

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
	else if (Mode == L"invalid-parameter")
	{
		char Buffer[4]{};
		strcpy_s(Buffer, sizeof(Buffer), nullptr);
	}
	else if (Mode == L"pure-call")
		_purecall(); // CRT 진입점을 직접 호출해 C++ object lifetime UB 없이 handler 연결을 검증한다.
	else if (Mode == L"abort")
		std::abort();
	else if (Mode == L"terminate")
		std::terminate();
	else if (Mode == L"uncaught")
		throw 42;
	else if (Mode == L"worker-uncaught")
	{
		std::thread CrashThread([] { throw 42; });
		CrashThread.join();
	}
	else if (Mode == L"caught")
	{
		try
		{
			throw 42;
		}
		catch (int)
		{
			// 정상 C++ exception 처리는 crash reporting을 거치지 않아야 한다.
		}
	}

	WindowsCrashReporting::Shutdown();
	const auto RestoredAbortHandler = std::signal(SIGABRT, OriginalAbortHandler);
	if (_get_invalid_parameter_handler() != OriginalInvalidParameterHandler
		|| _get_purecall_handler() != OriginalPureCallHandler
		|| std::get_terminate() != OriginalTerminateHandler
		|| RestoredAbortHandler != OriginalAbortHandler
		|| _set_abort_behavior(0, 0) != OriginalAbortBehavior)
		return ERROR_INVALID_STATE;
#if defined(_DEBUG)
	if (_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_REPORT_MODE) != OriginalAssertReportMode)
		return ERROR_INVALID_STATE;
#endif
	std::cout << "Normal shutdown" << std::endl;
	return 0;
}
