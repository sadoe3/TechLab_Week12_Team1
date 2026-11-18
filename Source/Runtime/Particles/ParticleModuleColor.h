#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleColor.generated.h"

/**
 * 초기 색/알파를 정하는 Spawn 모듈 (Unreal UParticleModuleColor, Cascade의 "Initial Color")
 * Spawn-time values are distributions (Constant / Uniform / Curve over the emitter loop, see GetDistributionValue).
 */
UCLASS()
class UParticleModuleColor : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleColor();
	virtual ~UParticleModuleColor() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

public:
	/** RGB (0~1) */
	UPROPERTY(EditAnywhere)
	FDistributionVector StartColor = FDistributionVector::MakeConstant(FVector::OneVector);

	UPROPERTY(EditAnywhere)
	FDistributionFloat StartAlpha = FDistributionFloat::MakeConstant(1.0f);
};
