#include "EnginePCH.h"
#include "CrashTest.h"

namespace
{
	// 요청과 소비 모두 main thread에서 수행한다.
	bool CrashRequested = false;

	// Dump에서 사고 함수와 source line을 확인할 수 있도록 inline하지 않는다.
	__declspec(noinline) void CauseMainCrash()
	{
		volatile int* InvalidAddress = nullptr;
		// Source Indexing 검증 표식: CRASH_SOURCE_REVISION_C — 새 Dump가 이 commit의 source를 가져오는지 확인한다.
		*InvalidAddress = 1;
	}
}

void CrashTest::Request()
{
	CrashRequested = true;
}

void CrashTest::Update()
{
	if (!CrashRequested)
		return;

	CrashRequested = false;
	CauseMainCrash();
}
