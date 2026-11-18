#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleRotationRateMultiplyLife.generated.h"

/**
 * 수명(RelativeTime 0~1)에 따라 회전 속도에 배율을 곱하는 Spawn+Update 모듈
 * (Unreal UParticleModuleRotationRateMultiplyLife 대응, Cascade의 "Rotation Rate * Life")
 *
 * 매 프레임 ResetParticleParameters가 RotationRate = BaseRotationRate로 되돌린 뒤 이 모듈이 곱하므로
 * 결과는 항상 "BaseRotationRate × 현재 나이의 배율"이고 누적되지 않는다. (SizeMultiplyLife와 같은 구조)
 * Like Unreal, LifeMultiplier is a curve whose X axis is RelativeTime (0~1).
 *
 * 주의: Spawn에서도 곱하므로 Modules 배열에서 Initial Rotation Rate 모듈보다 뒤에 있어야 한다.
 */
UCLASS()
class UParticleModuleRotationRateMultiplyLife : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleRotationRateMultiplyLife();
	virtual ~UParticleModuleRotationRateMultiplyLife() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;

	/** Every curve here is read with the particle's RelativeTime (see Apply) */
	virtual EParticleCurveInput GetCurveInput(const FProperty& CurveProperty) const override { return EParticleCurveInput::RelativeTime; }

private:
	void Apply(FBaseParticle& Particle) const;

public:
	/** Rotation rate multiplier over RelativeTime (0~1) (X=Pitch, Y=Yaw, Z=Roll) */
	UPROPERTY(EditAnywhere)
	FInterpCurveVector LifeMultiplier = FInterpCurveVector::MakeLinear(FVector::OneVector, FVector::OneVector);
};
