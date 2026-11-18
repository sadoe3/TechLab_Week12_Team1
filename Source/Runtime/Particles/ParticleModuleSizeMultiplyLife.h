#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleSizeMultiplyLife.generated.h"

/**
 * 수명(RelativeTime 0~1)에 따라 크기에 배율을 곱하는 Spawn+Update 모듈 (Unreal UParticleModuleSizeMultiplyLife 대응)
 *
 * 매 프레임 ResetParticleParameters가 Size = BaseSize로 되돌린 뒤 이 모듈이 Size *= 배율(RelativeTime)을 하므로
 * 결과는 항상 "BaseSize × 현재 나이의 배율"이 되고 프레임마다 누적되지 않는다.
 *
 * Like Unreal, LifeMultiplier is a curve whose X axis is RelativeTime (0~1).
 *
 * 주의: Spawn에서도 Size에 곱하므로 Modules 배열에서 Size(Initial Size) 모듈보다 뒤에 있어야 한다.
 * 앞에 있으면 뒤의 Size 모듈이 Size를 대입하면서 이 모듈의 결과를 덮어쓴다(다음 프레임부터는 정상).
 */
UCLASS()
class UParticleModuleSizeMultiplyLife : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleSizeMultiplyLife();
	virtual ~UParticleModuleSizeMultiplyLife() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;

	/** Every curve here is read with the particle's RelativeTime (see Apply) */
	virtual EParticleCurveInput GetCurveInput(const FProperty& CurveProperty) const override { return EParticleCurveInput::RelativeTime; }

private:
	void Apply(FBaseParticle& Particle) const;

public:
	/** Size multiplier over RelativeTime (0~1) */
	UPROPERTY(EditAnywhere)
	FInterpCurveVector LifeMultiplier = FInterpCurveVector::MakeLinear(FVector::OneVector, FVector::OneVector);

	/** 축별 적용 여부 (Unreal MultiplyX/Y/Z). Sprite는 X=가로, Y=세로, Mesh는 XYZ 스케일. */
	UPROPERTY(EditAnywhere)
	bool bMultiplyX = true;

	UPROPERTY(EditAnywhere)
	bool bMultiplyY = true;

	UPROPERTY(EditAnywhere)
	bool bMultiplyZ = true;
};
