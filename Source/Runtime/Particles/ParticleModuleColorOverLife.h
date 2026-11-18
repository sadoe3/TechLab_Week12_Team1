#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleColorOverLife.generated.h"

/**
 * 수명(RelativeTime 0~1)에 따라 색/알파를 바꾸는 Spawn+Update 모듈 (Unreal UParticleModuleColorOverLife 대응)
 * Like Unreal, ColorOverLife / AlphaOverLife are curves whose X axis is RelativeTime (0~1).
 */
UCLASS()
class UParticleModuleColorOverLife : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleColorOverLife();
	virtual ~UParticleModuleColorOverLife() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;

	/** Every curve here is read with the particle's RelativeTime (see Apply) */
	virtual EParticleCurveInput GetCurveInput(const FProperty& CurveProperty) const override { return EParticleCurveInput::RelativeTime; }

private:
	void Apply(FBaseParticle& Particle) const;

public:
	/** RGB over RelativeTime (0~1) */
	UPROPERTY(EditAnywhere)
	FInterpCurveVector ColorOverLife = FInterpCurveVector::MakeLinear(FVector::OneVector, FVector::OneVector);

	/** Alpha over RelativeTime (0~1) */
	UPROPERTY(EditAnywhere)
	FInterpCurveFloat AlphaOverLife = FInterpCurveFloat::MakeLinear(1.0f, 0.0f);
};
