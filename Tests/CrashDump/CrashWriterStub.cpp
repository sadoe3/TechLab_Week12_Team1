#include "Core/Windows/CrashReportChannel.h"
#include <cstdlib>

// 실제 writer 대신 테스트 폴더에만 배치한다. Production EXE에는 실패 주입 옵션을 추가하지 않는다.
int wmain(int ArgumentCount, wchar_t** Arguments)
{
	if (ArgumentCount != 4)
		return ERROR_INVALID_PARAMETER;

	wchar_t Behavior[32]{};
	GetEnvironmentVariableW(L"CRASH_TEST_WRITER_BEHAVIOR", Behavior, 32);
	if (std::wstring(Behavior) == L"startup-exit")
		return ERROR_PROCESS_ABORTED;

	FCrashReportChannel Channel;
	const DWORD ChannelError = Channel.Open(Arguments[2]);
	if (ChannelError != ERROR_SUCCESS)
		return ChannelError;

	HANDLE TargetProcess = OpenProcess(SYNCHRONIZE, FALSE, wcstoul(Arguments[1], nullptr, 10));
	if (!TargetProcess)
		return GetLastError();

	SetEvent(Channel.ReadyEvent);
	const HANDLE WaitHandles[] = { Channel.RequestEvent, Channel.StopEvent, TargetProcess };
	if (WaitForMultipleObjects(3, WaitHandles, FALSE, INFINITE) == WAIT_OBJECT_0 && std::wstring(Behavior) == L"timeout")
		WaitForSingleObject(TargetProcess, INFINITE); // 완료 신호를 보내지 않아 engine의 15초 제한을 검증한다.

	CloseHandle(TargetProcess);
	return 0;
}
