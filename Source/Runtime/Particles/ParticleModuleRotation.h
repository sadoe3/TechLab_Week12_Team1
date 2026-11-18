#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleRotation.generated.h"

/**
 * 초기 회전을 정하는 Spawn 모듈 (Unreal UParticleModuleRotation / UParticleModuleMeshRotation, Cascade의 "Initial Rotation")
 * Unreal은 스프라이트용(float, 1 = 360도)과 메시용(Vector)을 따로 두지만, 이 엔진은 FBaseParticle::Rotation이
 * 오일러 각(Pitch, Yaw, Roll, 도)이므로 하나로 합쳤다.
 *  - Sprite: Z(Roll)만 쓴다. 화면 안에서 쿼드가 도는 각도.
 *  - Mesh:   XYZ 모두 쓴다.
 * StartRotation(Distribution)을 Min~Max 균등 분포로 대신한다.
 */
UCLASS()
class UParticleModuleRotation : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleRotation();
	virtual ~UParticleModuleRotation() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;

public:
	/** 도 단위 (X=Pitch, Y=Yaw, Z=Roll) */
	UPROPERTY(EditAnywhere)
	FVector StartRotationMin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere)
	FVector StartRotationMax = FVector::ZeroVector;
};
