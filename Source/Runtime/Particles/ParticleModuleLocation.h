#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleLocation.generated.h"

/**
 * 스폰 위치에 오프셋을 더하는 Spawn 모듈 (Unreal UParticleModuleLocation 대응)
 * Spawn-time values are distributions (Constant / Uniform / Curve over the emitter loop, see GetDistributionValue). 오프셋은 이미터(컴포넌트) 공간 기준.
 */
UCLASS()
class UParticleModuleLocation : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleLocation();
	virtual ~UParticleModuleLocation() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

public:
	UPROPERTY(EditAnywhere)
	FDistributionVector StartLocation = FDistributionVector::MakeConstant(FVector::ZeroVector);
};
