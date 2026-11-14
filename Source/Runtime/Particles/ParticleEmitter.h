#pragma once

#include "UObject/Object.h"
#include "ParticleEmitter.generated.h"

class UParticleLODLevel;

UCLASS()
class UParticleEmitter : public UObject
{
	GENERATED_BODY()

public:
	UParticleEmitter();

	// Required/Spawn 모듈이 꽂힌 LODLevel 하나를 만들어 LODLevels에 추가하고 돌려준다
	UParticleLODLevel* CreateLODLevel();

	FName EmitterName;
	TArray<UParticleLODLevel*> LODLevels;
};
