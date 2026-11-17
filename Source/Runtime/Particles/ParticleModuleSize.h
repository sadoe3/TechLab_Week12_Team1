#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleSize.generated.h"

/**
 * 초기 크기를 정하는 Spawn 모듈 (Unreal UParticleModuleSize, Cascade의 "Initial Size")
 * Sprite는 X=가로, Y=세로, Mesh는 XYZ 스케일로 쓴다.
 */
UCLASS()
class UParticleModuleSize : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleSize();
	virtual ~UParticleModuleSize() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

public:
	UPROPERTY(EditAnywhere)
	FVector StartSizeMin = FVector::OneVector;

	UPROPERTY(EditAnywhere)
	FVector StartSizeMax = FVector::OneVector;
};
