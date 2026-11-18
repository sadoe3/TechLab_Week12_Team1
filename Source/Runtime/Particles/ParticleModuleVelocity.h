#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleVelocity.generated.h"

/**
 * 초기 속도를 더하는 Spawn 모듈 (Unreal UParticleModuleVelocity, Cascade의 "Initial Velocity")
 * Spawn-time values are distributions (Constant / Uniform / Curve over the emitter loop, see GetDistributionValue).
 */
UCLASS()
class UParticleModuleVelocity : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleVelocity();
	virtual ~UParticleModuleVelocity() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

public:
	UPROPERTY(EditAnywhere)
	FDistributionVector StartVelocity = FDistributionVector::MakeConstant(FVector::ZeroVector);

	/** 이미터 원점에서 파티클 방향으로 더할 속도 크기 */
	UPROPERTY(EditAnywhere)
	FDistributionFloat StartVelocityRadial = FDistributionFloat::MakeConstant(0.0f);

	/** true면 StartVelocity를 월드 공간 값으로 본다 (false면 이미터 공간) */
	UPROPERTY(EditAnywhere)
	bool bInWorldSpace = false;
};
