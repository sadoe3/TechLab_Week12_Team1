#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleAcceleration.generated.h"

/** 파티클마다 스폰 때 뽑은 가속도 (시뮬레이션 공간) */
struct FParticleAccelerationPayload
{
	FVector Acceleration = FVector::ZeroVector;
};

/**
 * 매 프레임 속도에 가속도를 더하는 모듈 (Unreal UParticleModuleAcceleration 대응)
 * 중력처럼 쓰려면 Acceleration = (0, 0, -9.8), bInWorldSpace = true.
 * Unreal의 Acceleration(Distribution)을 Min~Max 균등 분포로 대신한다.
 */
UCLASS()
class UParticleModuleAcceleration : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleAcceleration();
	virtual ~UParticleModuleAcceleration() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	virtual uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) const override;

public:
	UPROPERTY(EditAnywhere)
	FVector AccelerationMin = FVector(0.0f, 0.0f, -9.8f);

	UPROPERTY(EditAnywhere)
	FVector AccelerationMax = FVector(0.0f, 0.0f, -9.8f);

	/** true면 Acceleration을 월드 공간 값으로 본다 (로컬 스페이스 이미터가 회전해도 중력은 아래로) */
	UPROPERTY(EditAnywhere)
	bool bInWorldSpace = true;
};
