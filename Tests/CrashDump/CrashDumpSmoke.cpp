#include "Core/Windows/WindowsCrashDump.h"
#include <fstream>
#include <iostream>
#include <vector>

static FWindowsCrashContext CapturedCrashContext;

static LONG Capture(EXCEPTION_POINTERS* SourceExceptionPointers)
{
	CapturedCrashContext.Capture(GetCurrentThreadId(), *SourceExceptionPointers);
	return EXCEPTION_EXECUTE_HANDLER;
}

static void RaiseTestException()
{
	__try
	{
		RaiseException(0xE0420001, 0, 0, nullptr);
	}
	__except (Capture(GetExceptionInformation()))
	{
		// 테스트는 exception 정보를 보관한 뒤 writer 검사를 계속한다.
	}
}

int wmain(int argc, wchar_t** argv)
{
	if (argc != 2)
		return 1;

	const auto TestOutputDirectory = std::filesystem::absolute(argv[1]);

	RaiseTestException();
	auto CrashContext = CapturedCrashContext;
	// 원본을 지워 writer가 복사본만으로 기록하는지 검사한다.
	CapturedCrashContext = {};

	const DWORD ProcessId = GetCurrentProcessId();
	const auto ReportDirectory = TestOutputDirectory / std::to_wstring(ProcessId);
	if (WindowsCrashDump::Write(GetCurrentProcess(), ProcessId, ReportDirectory, CrashContext) != ERROR_SUCCESS)
		return 2;

	std::ifstream DumpInput(ReportDirectory / L"Crash.dmp", std::ios::binary);
	std::vector<char> DumpBytes((std::istreambuf_iterator<char>(DumpInput)), {});
	PMINIDUMP_DIRECTORY StreamDirectory = nullptr;
	void* ExceptionStreamData = nullptr;
	ULONG ExceptionStreamSize = 0;
	if (!MiniDumpReadDumpStream(DumpBytes.data(), ExceptionStream, &StreamDirectory, &ExceptionStreamData, &ExceptionStreamSize))
		return 3;

	const auto* DumpException = static_cast<MINIDUMP_EXCEPTION_STREAM*>(ExceptionStreamData);
	if (DumpException->ThreadId != CrashContext.ThreadId || DumpException->ExceptionRecord.ExceptionCode != 0xE0420001)
		return 4;

	const auto* DumpThreadContext = reinterpret_cast<const CONTEXT*>(DumpBytes.data() + DumpException->ThreadContext.Rva);
	if (DumpThreadContext->Rip != CrashContext.ThreadContext.Rip)
		return 5;

	const auto OriginalDumpSize = std::filesystem::file_size(ReportDirectory / L"Crash.dmp");
	if (WindowsCrashDump::Write(GetCurrentProcess(), ProcessId, ReportDirectory, CrashContext) != ERROR_FILE_EXISTS)
		return 6;
	if (std::filesystem::file_size(ReportDirectory / L"Crash.dmp") != OriginalDumpSize)
		return 7;
	if (WindowsCrashDump::Write(GetCurrentProcess(), ProcessId, L"relative", CrashContext) != ERROR_BAD_PATHNAME)
		return 8;
	if (WindowsCrashDump::Write(nullptr, MAXDWORD, ReportDirectory / L"Failure", CrashContext) == ERROR_SUCCESS)
		return 9;
	if (std::filesystem::exists(ReportDirectory / L"Failure/Crash.dmp"))
		return 10;

	std::cout << "PASS: exception/thread/RIP, copied context, no overwrite, relative path rejection, failed dump cleanup\n";
	return 0;
}
