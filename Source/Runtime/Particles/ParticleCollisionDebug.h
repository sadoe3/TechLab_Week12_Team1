#pragma once

#include "Math/EngineMath.h"
#include "Math/Box.h"

class UWorld;
class FLineBatcher;

/** 파티클 콜리전 디버그 표시 on/off (Settings > Debug Draw의 "Show Particle Collision") */
extern bool GShowParticleCollisionDebug;

/**
 * UParticleModuleCollision 동작을 눈으로 확인하기 위한 디버그 기록/표시.
 * 모듈은 Tick 중에 기록만 하고, 뷰포트가 그릴 때 Draw로 LineBatcher에 넣는다.
 * (LineBatcher는 그리기 직전에 BeginFrame으로 비워지므로 Tick에서 바로 선을 넣을 수 없다)
 */
namespace ParticleCollisionDebug
{
	struct FStats
	{
		int32 TestedEmitters = 0;   // 최근 프레임에 충돌 검사를 한 이미터 수
		int32 Candidates = 0;       // 최근 프레임 후보 컴포넌트 수 (이미터 합)
		int32 HitsPerSecond = 0;    // 최근 1초 동안 충돌 횟수
		int32 TotalHits = 0;        // Reset 이후 누적 충돌 횟수
	};

	/** 이미터 1개가 이번 프레임에 쓴 Swept AABB와 후보 바운드 */
	void RecordFrame(UWorld* World, const FBox& SweptBox, const TArray<FBox>& CandidateBounds);
	/** 충돌 1회 (월드 공간 접촉 위치, 법선) */
	void RecordHit(UWorld* World, const FVector& Location, const FVector& Normal);

	/** World에 속한 기록을 LineBatcher에 넣는다. GShowParticleCollisionDebug가 꺼져 있으면 아무것도 안 한다. */
	void Draw(UWorld* World, FLineBatcher* LineBatcher);

	FStats GetStats();
	void Reset();
}
