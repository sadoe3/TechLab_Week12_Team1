#pragma once
#include <Windows.h>
#include <filesystem>

namespace WindowsCrashReporting
{
	// 정상 startup에서 한 번 호출한다. 실패하면 filter를 등록하지 않고 오류를 반환한다.
	DWORD Initialize(const std::filesystem::path& ReportDirectory);
	// Engine 정상 cleanup이 끝난 뒤 writer와 channel을 정리한다.
	void Shutdown();
}
