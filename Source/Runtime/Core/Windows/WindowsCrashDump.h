#pragma once

#include <Windows.h>
#include <DbgHelp.h>
#include <filesystem>

// CRT callback을 Windows software exception으로 연결할 때 사용하는 application-defined code다.
enum class EWindowsRuntimeError : DWORD
{
	InvalidParameter = 0xE0000001,
	PureCall = 0xE0000002,
	Abort = 0xE0000003,
	Terminate = 0xE0000004
};

// Dump 전체가 아니라 사고 지점을 전달한다. stack/module 등은 MiniDumpWriteDump가 target에서 읽는다.
// 첫 exception record와 CPU context를 소유해 원래 EXCEPTION_POINTERS의 수명에 의존하지 않는다.
// 같은 x64 build 사이의 전달용이며 영구 저장 형식이나 IPC protocol은 아니다.
struct FWindowsCrashContext
{
	DWORD ThreadId = 0;
	EXCEPTION_RECORD ExceptionRecord{};
	CONTEXT ThreadContext{};

	void Capture(DWORD CrashingThreadId, const EXCEPTION_POINTERS& SourceExceptionPointers);
};

namespace WindowsCrashDump
{
	// TargetProcessHandle은 호출자가 소유하며 기록이 끝날 때까지 target process를 유지해야 한다.
	// CrashContext는 writer process의 local memory에 있어야 한다. DbgHelp 호출은 한 writer에서 직렬화한다.
	// 성공은 ERROR_SUCCESS, 실패는 Win32 오류 또는 MiniDumpWriteDump의 HRESULT를 반환한다.
	DWORD Write(HANDLE TargetProcessHandle, DWORD TargetProcessId, const std::filesystem::path& ReportDirectory, FWindowsCrashContext& CrashContext, MINIDUMP_TYPE DumpType = MiniDumpNormal);
}
