#pragma once
#include "WindowsCrashDump.h"
#include <string>

// 같은 build의 engine/writer가 공유한다. pointer 연결은 writer에서 다시 만든다.
struct FCrashReportData
{
	FWindowsCrashContext CrashContext;
	DWORD DumpError = ERROR_IO_PENDING;
};

// 이 channel의 mapping view와 event handle만 소유한다.
class FCrashReportChannel
{
public:
	FCrashReportChannel() = default;
	~FCrashReportChannel() { Close(); }
	FCrashReportChannel(const FCrashReportChannel&) = delete;
	FCrashReportChannel& operator=(const FCrashReportChannel&) = delete;

	DWORD Create(const std::wstring& ChannelName);
	DWORD Open(const std::wstring& ChannelName);
	void Close();

	FCrashReportData* Data = nullptr;
	HANDLE RequestEvent = nullptr;
	HANDLE ReadyEvent = nullptr;
	HANDLE CompletionEvent = nullptr;
	HANDLE StopEvent = nullptr;

private:
	HANDLE MappingHandle = nullptr;
};
