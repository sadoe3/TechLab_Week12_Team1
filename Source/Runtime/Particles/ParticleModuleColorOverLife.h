#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleColorOverLife.generated.h"

/**
 * 수명(RelativeTime 0~1)에 따라 색/알파를 바꾸는 Spawn+Update 모듈 (Unreal UParticleModuleColorOverLife 대응)
 * Unreal의 ColorOverLife/AlphaOverLife 커브를 시작값→끝값 선형 보간으로 대신한다.
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

private:
	void Apply(FBaseParticle& Particle) const;

public:
	UPROPERTY(EditAnywhere)
	FVector ColorStart = FVector::OneVector;

	UPROPERTY(EditAnywhere)
	FVector ColorEnd = FVector::OneVector;

	UPROPERTY(EditAnywhere)
	float AlphaStart = 1.0f;

	UPROPERTY(EditAnywhere)
	float AlphaEnd = 0.0f;
};
