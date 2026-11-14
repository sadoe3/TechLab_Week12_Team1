#pragma once

#include "UObject/Object.h"
#include "ParticleSystem.generated.h"

class UParticleEmitter;

UCLASS()
class UParticleSystem : public UObject
{
	GENERATED_BODY()

public:
	UParticleSystem();

	TArray<UParticleEmitter*> Emitters;
};
