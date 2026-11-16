#include "EnginePCH.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleSpawn.h"
#include "Particles/ParticleHelper.h"

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

void UParticleEmitter::UpdateModuleLists()
{
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		LODLevel->UpdateModuleLists();
	}
	CacheEmitterModuleInfo();
}

void UParticleEmitter::CacheEmitterModuleInfo()
{
	ModuleOffsetMap.Reset();
	UParticleLODLevel* LOD0 = LODLevels[0];
	int32 TotalBytes = sizeof(FBaseParticle);
	for (UParticleModule* Module : LOD0->Modules)
	{
		ModuleOffsetMap.FindOrAdd(Module) = TotalBytes;
		TotalBytes += Module->RequiredBytes();
	}
	ParticleSize = TotalBytes;
}
