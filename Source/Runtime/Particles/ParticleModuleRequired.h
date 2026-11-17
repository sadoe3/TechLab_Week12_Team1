#pragma once

#include "Particles/ParticleModule.h"
#include "Render/Buffer.h"
#include "Render/Material.h"
#include "ParticleModuleRequired.generated.h"

/**
 * 이미터 안에서 파티클을 그리는 순서 (Unreal EParticleSortMode 대응, 순서·이름 동일)
 * 이미터 하나가 드로우 1번이므로 정렬은 그 이미터의 파티클끼리만 한다. 반투명 블렌딩이 겹칠 때 결과를 맞추기 위한 것.
 */
UENUM()
enum class EParticleSortMode : uint8
{
	PSORTMODE_None = 0,             // 정렬 안 함 (가산 블렌딩·불투명이면 순서가 결과에 영향 없음)
	PSORTMODE_ViewProjDepth = 1,    // 카메라 앞 방향 깊이 기준, 먼 것부터
	PSORTMODE_DistanceToView = 2,   // 카메라까지 거리 기준, 먼 것부터
	PSORTMODE_Age_OldestFirst = 3,  // 오래된 파티클(RelativeTime 큰 것)부터
	PSORTMODE_Age_NewestFirst = 4,  // 새 파티클(RelativeTime 작은 것)부터
};

/**
 * 모든 LODLevel에 필수로 포함되는 이미터 기본 설정 모듈 (Unreal UParticleModuleRequired 대응)
 */
UCLASS()
class UParticleModuleRequired : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleRequired() = default;
	virtual ~UParticleModuleRequired() override = default;

public:
	UPROPERTY(EditAnywhere)
	UMaterial* Material = nullptr;

	UPROPERTY(EditAnywhere)
	bool bUseLocalSpace = false;

	/** 이미터 내부 파티클 정렬 방식. 뷰마다 GetDynamicMeshElements에서 인스턴스 순서를 정한다. */
	UPROPERTY(EditAnywhere)
	EParticleSortMode SortMode = EParticleSortMode::PSORTMODE_None;

	UPROPERTY(EditAnywhere)
	bool bKillOnDeactivate = false;

	UPROPERTY(EditAnywhere)
	bool bKillOnCompleted = false;

	UPROPERTY(EditAnywhere)
	float EmitterDuration = 1.0f;

	UPROPERTY(EditAnywhere)
	float EmitterDelay = 0.0f;

	/** 0이면 무한 루프 */
	UPROPERTY(EditAnywhere)
	int32 EmitterLoops = 0;

	UPROPERTY(EditAnywhere)
	int32 MaxDrawCount = 500;

	/** SubUV 가로 분할 수 (Unreal UParticleModuleRequired::SubImages_Horizontal 대응) */
	UPROPERTY(EditAnywhere)
	int32 SubImages_Horizontal = 1;

	/** SubUV 세로 분할 수 (Unreal UParticleModuleRequired::SubImages_Vertical 대응) */
	UPROPERTY(EditAnywhere)
	int32 SubImages_Vertical = 1;
};
