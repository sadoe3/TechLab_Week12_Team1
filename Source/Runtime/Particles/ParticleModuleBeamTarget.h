#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleBeamTarget.generated.h"

/**
 * 빔의 끝점(Target)을 정하는 모듈 (Unreal UParticleModuleBeamTarget 대응)
 * Unreal은 Spawn 모듈로 파티클 Payload에 Target을 기록하지만, 여기서는 이미터당 빔 1개라 인스턴스가 프로퍼티를 직접 읽는다.
 */
UCLASS()
class UParticleModuleBeamTarget : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleBeamTarget() = default;
	virtual ~UParticleModuleBeamTarget() override = default;

public:
	/** PSC(이미터) 로컬 공간 기준 끝점 */
	UPROPERTY(EditAnywhere)
	FVector TargetLocation = FVector(0.0f, 0.0f, 5.0f);
};
