#include "WindowsCrashDump.h"

void FWindowsCrashContext::Capture(DWORD CrashingThreadId, const EXCEPTION_POINTERS& SourceExceptionPointers)
{
	ThreadId = CrashingThreadId;
	ExceptionRecord = *SourceExceptionPointers.ExceptionRecord;

	// 최초 사고를 기록한다. 원래 process의 linked record pointer를 역참조하지 않는다.
	ExceptionRecord.ExceptionRecord = nullptr;

	ThreadContext = *SourceExceptionPointers.ContextRecord;
}

DWORD WindowsCrashDump::Write(HANDLE TargetProcessHandle, DWORD TargetProcessId, const std::filesystem::path& ReportDirectory, FWindowsCrashContext& CrashContext, MINIDUMP_TYPE DumpType)
{
	if (!ReportDirectory.is_absolute())
		return ERROR_BAD_PATHNAME;

	std::error_code DirectoryError;
	std::filesystem::create_directories(ReportDirectory, DirectoryError);
	if (DirectoryError)
		return static_cast<DWORD>(DirectoryError.value());

	// CREATE_NEW로 기존 사고의 Dump를 덮어쓰지 않는다.
	const auto DumpFilePath = ReportDirectory / L"Crash.dmp";
	HANDLE DumpFileHandle = CreateFileW(DumpFilePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (DumpFileHandle == INVALID_HANDLE_VALUE)
		return GetLastError();

	// 복사된 writer 쪽 주소로 연결한다. target의 code address 자체는 변경하지 않는다.
	// ClientPointers=FALSE: EXCEPTION_POINTERS가 writer의 local memory를 가리킨다.
	EXCEPTION_POINTERS LocalExceptionPointers{ &CrashContext.ExceptionRecord, &CrashContext.ThreadContext };
	MINIDUMP_EXCEPTION_INFORMATION DumpExceptionInfo{ CrashContext.ThreadId, &LocalExceptionPointers, FALSE };

	const BOOL DumpWritten = MiniDumpWriteDump(TargetProcessHandle, TargetProcessId, DumpFileHandle, DumpType, &DumpExceptionInfo, nullptr, nullptr);
	// CloseHandle이 last error를 바꾸기 전에 기록 실패의 원인을 보존한다.
	DWORD WriteError = DumpWritten ? ERROR_SUCCESS : GetLastError();

	if (!CloseHandle(DumpFileHandle) && WriteError == ERROR_SUCCESS)
		WriteError = GetLastError();

	// 실패한 파일의 삭제를 시도하되 최초 기록/close 오류를 반환한다.
	if (WriteError != ERROR_SUCCESS)
		DeleteFileW(DumpFilePath.c_str());

	return WriteError;
}
