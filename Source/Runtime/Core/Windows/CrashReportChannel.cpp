#include "CrashReportChannel.h"
#include <new>

DWORD FCrashReportChannel::Create(const std::wstring& ChannelName)
{
	MappingHandle = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(FCrashReportData), (ChannelName + L"_Data").c_str());
	if (!MappingHandle)
		return GetLastError();

	Data = static_cast<FCrashReportData*>(MapViewOfFile(MappingHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(FCrashReportData)));
	if (!Data)
		return GetLastError();
	new (Data) FCrashReportData{};

	RequestEvent = CreateEventW(nullptr, TRUE, FALSE, (ChannelName + L"_Request").c_str());
	if (!RequestEvent)
		return GetLastError();
	ReadyEvent = CreateEventW(nullptr, TRUE, FALSE, (ChannelName + L"_Ready").c_str());
	if (!ReadyEvent)
		return GetLastError();
	CompletionEvent = CreateEventW(nullptr, TRUE, FALSE, (ChannelName + L"_Done").c_str());
	if (!CompletionEvent)
		return GetLastError();
	StopEvent = CreateEventW(nullptr, TRUE, FALSE, (ChannelName + L"_Stop").c_str());
	return StopEvent ? ERROR_SUCCESS : GetLastError();
}

DWORD FCrashReportChannel::Open(const std::wstring& ChannelName)
{
	MappingHandle = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, (ChannelName + L"_Data").c_str());
	if (!MappingHandle)
		return GetLastError();
	Data = static_cast<FCrashReportData*>(MapViewOfFile(MappingHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(FCrashReportData)));
	if (!Data)
		return GetLastError();

	RequestEvent = OpenEventW(SYNCHRONIZE, FALSE, (ChannelName + L"_Request").c_str());
	if (!RequestEvent)
		return GetLastError();
	ReadyEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, (ChannelName + L"_Ready").c_str());
	if (!ReadyEvent)
		return GetLastError();
	CompletionEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, (ChannelName + L"_Done").c_str());
	if (!CompletionEvent)
		return GetLastError();
	StopEvent = OpenEventW(SYNCHRONIZE, FALSE, (ChannelName + L"_Stop").c_str());
	return StopEvent ? ERROR_SUCCESS : GetLastError();
}

void FCrashReportChannel::Close()
{
	if (Data)
		UnmapViewOfFile(Data);
	Data = nullptr;

	for (HANDLE* OwnedHandle : { &RequestEvent, &ReadyEvent, &CompletionEvent, &StopEvent, &MappingHandle })
	{
		if (*OwnedHandle)
			CloseHandle(*OwnedHandle);
		*OwnedHandle = nullptr;
	}
}
