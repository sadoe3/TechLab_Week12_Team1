#include "EnginePCH.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"

UParticleLODLevel* UParticleEmitter::GetLODLevel(int32 LODLevel) const
{
	if (LODLevel < 0 || LODLevel >= LODLevels.Num())
	{
		return nullptr;
	}
	return LODLevels[LODLevel];
}

void UParticleEmitter::AddLODLevel(UParticleLODLevel* InLODLevel)
{
	if (InLODLevel)
	{
		LODLevels.Add(InLODLevel);
	}
}

void UParticleEmitter::ClearLODLevels()
{
	LODLevels.Empty();
}

void UParticleEmitter::CacheEmitterModuleInfo()
{
	for (UParticleLODLevel* LODLevel : LODLevels)
	{
		if (LODLevel)
		{
			LODLevel->UpdateModuleLists();
		}
	}
}
