#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleColor.generated.h"

/**
 * 초기 색/알파를 정하는 Spawn 모듈 (Unreal UParticleModuleColor, Cascade의 "Initial Color")
 * StartColor(Vector)/StartAlpha(Float) Distribution을 Min~Max 균등 분포로 대신한다.
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
	FVector StartColorMin = FVector::OneVector;

	UPROPERTY(EditAnywhere)
	FVector StartColorMax = FVector::OneVector;

	UPROPERTY(EditAnywhere)
	float StartAlphaMin = 1.0f;

	UPROPERTY(EditAnywhere)
	float StartAlphaMax = 1.0f;
};
