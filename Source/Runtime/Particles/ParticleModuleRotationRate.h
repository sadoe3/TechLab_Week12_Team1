#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleRotationRate.generated.h"

/**
 * 초기 회전 속도를 정하는 Spawn 모듈 (Unreal UParticleModuleRotationRate / UParticleModuleMeshRotationRate,
 * Cascade의 "Initial Rotation Rate")
 * 회전은 UpdateBoundingBox의 적분(Rotation += RotationRate * DeltaTime)에서 진행된다.
 *  - Sprite: Z(Roll)만 쓴다.
 *  - Mesh:   XYZ 모두 쓴다.
 * Spawn-time values are distributions (Constant / Uniform / Curve over the emitter loop, see GetDistributionValue).
 */
UCLASS()
class UParticleModuleRotationRate : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleRotationRate();
	virtual ~UParticleModuleRotationRate() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

public:
	/** 도/초 단위 (X=Pitch, Y=Yaw, Z=Roll) */
	UPROPERTY(EditAnywhere)
	FDistributionVector StartRotationRate = FDistributionVector::MakeConstant(FVector::ZeroVector);
};
