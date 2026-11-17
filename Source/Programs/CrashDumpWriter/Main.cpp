#include "Core/Windows/CrashReportChannel.h"
#include <fstream>
#include <cstdlib>

namespace
{
	const char* GetCrashReason(DWORD ExceptionCode)
	{
		switch (static_cast<EWindowsRuntimeError>(ExceptionCode))
		{
		case EWindowsRuntimeError::InvalidParameter: return "InvalidParameter";
		case EWindowsRuntimeError::PureCall: return "PureCall";
		case EWindowsRuntimeError::Abort: return "Abort";
		case EWindowsRuntimeError::Terminate: return "Terminate";
		default: return "UnhandledException";
		}
	}
}

int wmain(int ArgumentCount, wchar_t** Arguments)
{
	if (ArgumentCount != 4)
		return ERROR_INVALID_PARAMETER;

	const DWORD TargetProcessId = wcstoul(Arguments[1], nullptr, 10);
	const std::wstring ChannelName = Arguments[2];
	const std::filesystem::path ReportDirectory = Arguments[3];
	FCrashReportChannel ReportChannel;
	const DWORD ChannelError = ReportChannel.Open(ChannelName);
	if (ChannelError != ERROR_SUCCESS)
		return ChannelError;

	HANDLE TargetProcessHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE, FALSE, TargetProcessId);
	if (!TargetProcessHandle)
		return GetLastError();

	SetEvent(ReportChannel.ReadyEvent);
	// 정상 stop과 target process 종료에서는 Dump를 만들지 않는다.
	const HANDLE WaitHandles[] = { ReportChannel.RequestEvent, ReportChannel.StopEvent, TargetProcessHandle };
	const DWORD WaitResult = WaitForMultipleObjects(3, WaitHandles, FALSE, INFINITE);
	DWORD WriterError = ERROR_SUCCESS;
	if (WaitResult == WAIT_OBJECT_0)
	{
		// 공유 view에서 local로 복사한다. C2 writer가 이 process의 pointer를 다시 연결한다.
		FWindowsCrashContext CrashContext = ReportChannel.Data->CrashContext;
		WriterError = WindowsCrashDump::Write(TargetProcessHandle, TargetProcessId, ReportDirectory, CrashContext);
		ReportChannel.Data->DumpError = WriterError;

		// Dump와 metadata 저장 결과는 별개다. metadata 실패가 완료 통지를 막지 않게 한다.
		std::ofstream ReportFile(ReportDirectory / L"Report.json");
		ReportFile << "{\n  \"ProcessId\": " << TargetProcessId << ",\n  \"ThreadId\": " << CrashContext.ThreadId
			<< ",\n  \"ExceptionCode\": " << CrashContext.ExceptionRecord.ExceptionCode
			<< ",\n  \"CrashReason\": \"" << GetCrashReason(CrashContext.ExceptionRecord.ExceptionCode) << "\""
			<< ",\n  \"DumpError\": " << WriterError << ",\n  \"WriterProcessId\": " << GetCurrentProcessId() << "\n}\n";
		ReportFile.close();
		SetEvent(ReportChannel.CompletionEvent);
	}
	else if (WaitResult == WAIT_FAILED)
		WriterError = GetLastError();

	CloseHandle(TargetProcessHandle);
	return WriterError;
}
