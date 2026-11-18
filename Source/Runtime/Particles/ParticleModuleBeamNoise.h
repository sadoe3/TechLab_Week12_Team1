#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleBeamNoise.generated.h"

/**
 * 빔 중간 점을 빔과 수직 방향으로 흔들어 번개처럼 보이게 하는 모듈 (Unreal UParticleModuleBeamNoise 대응)
 * Unreal의 NoiseRange·NoiseSpeed는 분포(Distribution)지만, 여기서는 고정값이고 NoiseSpeed는 float 하나다.
 */
UCLASS()
class UParticleModuleBeamNoise : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleBeamNoise() = default;
	virtual ~UParticleModuleBeamNoise() override = default;

public:
	/** 노이즈 전체 스위치 */
	UPROPERTY(EditAnywhere)
	bool bLowFreq_Enabled = true;

	/** Source~Target 사이 노이즈 구간 수. 꺾이는 점 개수 = Frequency - 1. 빔 Segments가 이보다 커야 꺾임이 다 보인다 */
	UPROPERTY(EditAnywhere)
	int32 Frequency = 5;

	/** 축마다 흔들리는 최대 거리 (PSC 로컬). 빔 길이 방향 성분은 빼고 쓴다 */
	UPROPERTY(EditAnywhere)
	FVector NoiseRange = FVector(0.5f, 0.5f, 0.5f);

	/** 노이즈 점이 새 목표로 움직이는 속도(초당 거리). 0이면 목표로 바로 점프한다 */
	UPROPERTY(EditAnywhere)
	float NoiseSpeed = 0.0f;

	/** 새 목표를 뽑는 간격(초). 0이면 매 프레임 */
	UPROPERTY(EditAnywhere)
	float NoiseLockTime = 0.1f;

	/** true면 노이즈 점 사이를 부드럽게(smoothstep), false면 직선으로 잇는다 */
	UPROPERTY(EditAnywhere)
	bool bSmooth = false;

	/** true면 Target 끝점도 흔든다 */
	UPROPERTY(EditAnywhere)
	bool bTargetNoise = false;
};
