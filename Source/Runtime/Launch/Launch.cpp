#include "EnginePCH.h"

#include "Launch/EntryPoint.h"
#include "Launch/LaunchEngineLoop.h"
#include "Core/CoreGlobals.h"
#include "Core/Windows/WindowsCrashReporting.h"

// 플랫폼과 무관한 프로그램의 일생. UE의 GuardedMain(Launch.cpp)과 같은 자리다.
// 순서는 PreInit, Init, Tick 반복, Exit이다.
int32 GuardedMain(HINSTANCE hInstance)
{
	FEngineLoop EngineLoop;

	// 정상 종료와 초기화 실패의 return에서 cleanup을 실행한다. Crash의 강제 종료는 이 경로를 거치지 않는다.
	// 강제 종료 case는 HandleUnhandledException에서 process 종료 전에 처리한다.
	// EngineLoop보다 뒤에 선언했으므로 EngineLoop가 파괴되기 전에 먼저 실행된다.
	struct FEngineLoopCleanupGuard
	{
		FEngineLoop& Loop;
		~FEngineLoopCleanupGuard()
		{
			Loop.Exit();
			// 이 cleanup 경로는 Dump를 기록하지 않고, 대기 중인 CrashDumpWriter에 정상 종료를 요청한다.
			WindowsCrashReporting::Shutdown();
		}
	} CleanupGuard{ EngineLoop };

	if (!EngineLoop.PreInit(hInstance, GetEngineClass())) return -1;
	if (!EngineLoop.Init()) return -1;

	while (!IsEngineExitRequested())
	{
		EngineLoop.Tick();
	}

	return 0;
}
