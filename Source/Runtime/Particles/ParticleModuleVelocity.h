#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleVelocity.generated.h"

/**
 * 초기 속도를 더하는 Spawn 모듈 (Unreal UParticleModuleVelocity, Cascade의 "Initial Velocity")
 * StartVelocity/StartVelocityRadial(Distribution)을 Min~Max 균등 분포로 대신한다.
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
	FVector StartVelocityMin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere)
	FVector StartVelocityMax = FVector::ZeroVector;

	/** 이미터 원점에서 파티클 방향으로 더할 속도 크기 */
	UPROPERTY(EditAnywhere)
	float StartVelocityRadialMin = 0.0f;

	UPROPERTY(EditAnywhere)
	float StartVelocityRadialMax = 0.0f;

	/** true면 StartVelocity를 월드 공간 값으로 본다 (false면 이미터 공간) */
	UPROPERTY(EditAnywhere)
	bool bInWorldSpace = false;
};
