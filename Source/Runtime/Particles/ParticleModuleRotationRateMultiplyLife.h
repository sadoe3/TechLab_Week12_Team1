#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleRotationRateMultiplyLife.generated.h"

/**
 * 수명(RelativeTime 0~1)에 따라 회전 속도에 배율을 곱하는 Spawn+Update 모듈
 * (Unreal UParticleModuleRotationRateMultiplyLife 대응, Cascade의 "Rotation Rate * Life")
 *
 * 매 프레임 ResetParticleParameters가 RotationRate = BaseRotationRate로 되돌린 뒤 이 모듈이 곱하므로
 * 결과는 항상 "BaseRotationRate × 현재 나이의 배율"이고 누적되지 않는다. (SizeMultiplyLife와 같은 구조)
 * Unreal의 LifeMultiplier 커브를 시작값→끝값 선형 보간으로 대신한다.
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

private:
	void Apply(FBaseParticle& Particle) const;

public:
	/** 수명 시작(RelativeTime = 0)의 배율 (X=Pitch, Y=Yaw, Z=Roll) */
	UPROPERTY(EditAnywhere)
	FVector MultiplierStart = FVector::OneVector;

	/** 수명 끝(RelativeTime = 1)의 배율 */
	UPROPERTY(EditAnywhere)
	FVector MultiplierEnd = FVector::OneVector;
};
