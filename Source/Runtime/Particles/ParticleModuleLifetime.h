#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleLifetime.generated.h"

/**
 * 파티클 수명을 정하는 Spawn 모듈 (Unreal UParticleModuleLifetime 대응)
 * Spawn-time values are distributions (Constant / Uniform / Curve over the emitter loop, see GetDistributionValue).
 */
UCLASS()
class UParticleModuleLifetime : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleLifetime();
	virtual ~UParticleModuleLifetime() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

	/** 이미터 시간 기준 최대 수명 (에디터 표시·바운드 추정용) */
	float GetMaxLifetime() const { return Lifetime.GetMaxValue(); }

public:
	/** 수명(초). 0 이하이면 영원히 산다 (Unreal과 동일) */
	UPROPERTY(EditAnywhere)
	FDistributionFloat Lifetime = FDistributionFloat::MakeConstant(1.0f);
};
