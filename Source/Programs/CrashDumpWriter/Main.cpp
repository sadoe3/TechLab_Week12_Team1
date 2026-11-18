#include "Core/Windows/CrashReportChannel.h"
#include <fstream>
#include <cstdlib>
#include <format>

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

	const wchar_t* GetCrashDescription(DWORD ExceptionCode)
	{
		switch (ExceptionCode)
		{
		case EXCEPTION_ACCESS_VIOLATION: return L"Access violation: 유효하지 않은 memory에 접근했습니다.";
		case EXCEPTION_STACK_OVERFLOW: return L"Stack overflow: thread의 stack 공간을 초과했습니다.";
		case EXCEPTION_INT_DIVIDE_BY_ZERO: return L"Integer divide by zero: 정수를 0으로 나누었습니다.";
		case static_cast<DWORD>(EWindowsRuntimeError::InvalidParameter): return L"Invalid parameter: CRT 함수에 잘못된 인자가 전달되었습니다.";
		case static_cast<DWORD>(EWindowsRuntimeError::PureCall): return L"Pure virtual call: 순수 가상 함수 호출 오류가 발생했습니다.";
		case static_cast<DWORD>(EWindowsRuntimeError::Abort): return L"Abort: runtime에서 프로그램 중단을 요청했습니다.";
		case static_cast<DWORD>(EWindowsRuntimeError::Terminate): return L"Terminate: C++ runtime의 종료 handler가 호출되었습니다.";
		default: return L"Unhandled exception: 처리되지 않은 exception이 발생했습니다.";
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
	if (WaitResult == WAIT_OBJECT_0 && WriterError == ERROR_SUCCESS)
	{
		// 완료 event를 먼저 보내 engine 종료를 허용한다. 사용자 응답은 writer만 기다린다.
		// 오류 종류를 안내하며, 실제 원인은 저장된 Dump를 분석해야 한다.
		const DWORD ExceptionCode = ReportChannel.Data->CrashContext.ExceptionRecord.ExceptionCode;
		const auto Message = std::format(L"Crash가 발생하여 프로그램을 종료합니다.\n\n{}\nExceptionCode: 0x{:08X}\n\nDump를 저장했습니다.\n{}\n\n정확한 원인은 Dump를 분석해 확인할 수 있습니다.", GetCrashDescription(ExceptionCode), ExceptionCode, (ReportDirectory / L"Crash.dmp").wstring());
		MessageBoxW(nullptr, Message.c_str(), L"Crash — Dump 저장 완료", MB_OK | MB_ICONERROR);
	}
	return WriterError;
}
