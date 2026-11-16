#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleLifetime.generated.h"

UCLASS()
class UParticleModuleLifetime : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleLifetime();

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

	// 규칙만 담는다. 실제로 뽑힌 수명은 파티클마다 따로 저장된다
	UPROPERTY(EditAnywhere)
	float LifetimeMin = 1.0f;
	UPROPERTY(EditAnywhere)
	float LifetimeMax = 3.0f;
};
