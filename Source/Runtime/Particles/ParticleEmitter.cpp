#include "EnginePCH.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleSpawn.h"

UParticleEmitter::UParticleEmitter()
{
}

UParticleLODLevel* UParticleEmitter::CreateLODLevel()
{
	UParticleLODLevel* LODLevel = NewObject<UParticleLODLevel>(this);
	LODLevel->RequiredModule = NewObject<UParticleModuleRequired>(LODLevel);
	LODLevel->SpawnModule = NewObject<UParticleModuleSpawn>(LODLevel);
	LODLevel->Level = LODLevels.Num();
	LODLevels.Add(LODLevel);
	return LODLevel;
}
