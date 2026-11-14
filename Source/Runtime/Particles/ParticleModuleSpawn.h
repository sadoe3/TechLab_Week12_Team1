#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleSpawn.generated.h"

UCLASS()
class UParticleModuleSpawn : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleSpawn();

	// 1초에 만들 파티클 수
	UPROPERTY(EditAnywhere)
	float Rate = 20.0f;
};
