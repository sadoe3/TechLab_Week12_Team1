#include "WindowsCrashReporting.h"
#include "CrashReportChannel.h"

namespace
{
	constexpr DWORD WriterStartupTimeoutMs = 5000;
	constexpr DWORD DumpCompletionTimeoutMs = 15000;
	FCrashReportChannel ReportChannel;
	HANDLE WriterProcessHandle = nullptr;
	LPTOP_LEVEL_EXCEPTION_FILTER PreviousExceptionFilter = nullptr;
	bool FilterInstalled = false;
	volatile LONG CrashClaimed = 0;

	// Unhandled exception이 등록된 top-level filter에 도달하면 Windows가 process 종료 전에 호출한다.
	// 사고 context를 공유하고 CrashDumpWriter process에 Dump 기록을 요청한 뒤, 완료 또는 timeout 후 engine을 강제 종료한다.
	// 외부에서 process를 강제 종료하는 모든 경우에 호출되는 callback은 아니다.
	LONG WINAPI HandleUnhandledException(EXCEPTION_POINTERS* SourceExceptionPointers)
	{
		// 최초 사고만 공유 데이터를 쓴다. 이후 진입도 동일한 완료 event를 기다린다.
		if (InterlockedCompareExchange(&CrashClaimed, 1, 0) == 0)
		{
			ReportChannel.Data->CrashContext.Capture(GetCurrentThreadId(), *SourceExceptionPointers);
			SetEvent(ReportChannel.RequestEvent);
		}

		// target가 살아 있는 동안 writer가 메모리를 읽는다. crash thread에서 heap/log/UI는 사용하지 않는다.
		const HANDLE WaitHandles[] = { ReportChannel.CompletionEvent, WriterProcessHandle };
		const DWORD WaitResult = WaitForMultipleObjects(2, WaitHandles, FALSE, DumpCompletionTimeoutMs);
		if (WaitResult == WAIT_TIMEOUT)
		{
			OutputDebugStringA("Crash dump collection timed out.\n");
			TerminateProcess(WriterProcessHandle, ERROR_TIMEOUT);
		}
		else if (WaitResult != WAIT_OBJECT_0)
			OutputDebugStringA("Crash writer exited or wait failed.\n");
		else if (ReportChannel.Data->DumpError != ERROR_SUCCESS)
			OutputDebugStringA("Crash writer could not write the dump.\n");

		// 정상 engine cleanup으로 복귀하지 않는다. timeout 때도 무한 대기하지 않는다.
		// Crash 이후에는 정상 cleanup이 기대하는 상태가 유효하다고 보장할 수 없기 때문이다.
		TerminateProcess(GetCurrentProcess(), SourceExceptionPointers->ExceptionRecord->ExceptionCode);
		return EXCEPTION_EXECUTE_HANDLER;
	}
}

DWORD WindowsCrashReporting::Initialize(const std::filesystem::path& ReportDirectory)
{
	if (!ReportDirectory.is_absolute())
		return ERROR_BAD_PATHNAME;

	const std::wstring ChannelName = L"Local\\HitoriCrash_" + ReportDirectory.filename().wstring();
	DWORD StartupError = ReportChannel.Create(ChannelName);
	if (StartupError != ERROR_SUCCESS)
	{
		Shutdown();
		return StartupError;
	}

	wchar_t ExecutablePath[32768]{};
	const DWORD PathLength = GetModuleFileNameW(nullptr, ExecutablePath, 32768);
	if (PathLength == 0 || PathLength == 32768)
	{
		StartupError = PathLength ? ERROR_INSUFFICIENT_BUFFER : GetLastError();
		Shutdown();
		return StartupError;
	}

	const auto WriterPath = std::filesystem::path(ExecutablePath).parent_path() / L"CrashDumpWriter.exe";
	std::wstring CommandLine = L"\"" + WriterPath.wstring() + L"\" " + std::to_wstring(GetCurrentProcessId()) + L" \"" + ChannelName + L"\" \"" + ReportDirectory.wstring() + L"\"";
	STARTUPINFOW StartupInfo{};
	StartupInfo.cb = sizeof(StartupInfo);
	PROCESS_INFORMATION ProcessInfo{};
	if (!CreateProcessW(WriterPath.c_str(), CommandLine.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &StartupInfo, &ProcessInfo))
	{
		StartupError = GetLastError();
		Shutdown();
		return StartupError;
	}
	CloseHandle(ProcessInfo.hThread);
	WriterProcessHandle = ProcessInfo.hProcess;

	// ReadyEvent는 준비 완료, WriterProcessHandle은 준비 도중 writer의 종료를 감시한다.
	// FALSE: 둘 중 하나라도 signaled가 되면 반환하므로 조기 종료를 timeout 전에 확인할 수 있다.
	const HANDLE WaitHandles[] = { ReportChannel.ReadyEvent, WriterProcessHandle };
	const DWORD WaitResult = WaitForMultipleObjects(2, WaitHandles, FALSE, WriterStartupTimeoutMs);
	if (WaitResult != WAIT_OBJECT_0)
	{
		StartupError = WaitResult == WAIT_TIMEOUT ? ERROR_TIMEOUT : ERROR_PROCESS_ABORTED;
		Shutdown();
		return StartupError;
	}

	CrashClaimed = 0;
	PreviousExceptionFilter = SetUnhandledExceptionFilter(HandleUnhandledException);
	FilterInstalled = true;
	return ERROR_SUCCESS;
}

void WindowsCrashReporting::Shutdown()
{
	if (FilterInstalled)
		SetUnhandledExceptionFilter(PreviousExceptionFilter);
	FilterInstalled = false;

	if (WriterProcessHandle)
	{
		SetEvent(ReportChannel.StopEvent);
		if (WaitForSingleObject(WriterProcessHandle, WriterStartupTimeoutMs) == WAIT_TIMEOUT)
			TerminateProcess(WriterProcessHandle, ERROR_TIMEOUT);
		CloseHandle(WriterProcessHandle);
		WriterProcessHandle = nullptr;
	}
	ReportChannel.Close();
}
