#include "EnginePCH.h"
#include "Particles/ParticleModuleLocation.h"
#include "Particles/BaseParticle.h"

UParticleModuleLocation::UParticleModuleLocation()
{
	bSpawnModule = true;
}

void UParticleModuleLocation::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	const FVector LocationOffset = RandomRange(Owner, StartLocationMin, StartLocationMax);
	ParticleBase->Location += EmitterToSimulationVector(Owner, LocationOffset);
}
