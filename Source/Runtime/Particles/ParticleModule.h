#pragma once

#include "UObject/Object.h"
#include "ParticleModule.generated.h"

UCLASS()
class UParticleModule : public UObject
{
	GENERATED_BODY()

public:
	UParticleModule();

	// Phase 2에서 FParticleEmitterInstance, FBaseParticle이 생기면 연다.
	// virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) {}
	// virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) {}

	// LODLevel이 이 값을 보고 SpawnModules/UpdateModules 목록을 나누므로 밖에서 읽을 수 있어야 한다
	bool bSpawnModule = false;
	bool bUpdateModule = false;
};
