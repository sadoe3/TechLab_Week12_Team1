#include "EnginePCH.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/BaseParticle.h"

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

	ModuleOffsetMap.Reset();
	int32 TotalBytes = sizeof(FBaseParticle);

	// LOD마다 오프셋이 다르면 LOD 전환 때 파티클 메모리를 다시 짜야 하므로 LOD0 기준으로 한 번만 배정한다
	UParticleLODLevel* LOD0 = GetLODLevel(0);
	if (LOD0)
	{
		for (UParticleModule* Module : LOD0->GetModules())
		{
			if (!Module)
			{
				continue;
			}
			ModuleOffsetMap.FindOrAdd(Module) = TotalBytes;
			TotalBytes += static_cast<int32>(Module->RequiredBytes(LOD0->TypeDataModule));
		}
	}
	ParticleSize = TotalBytes;
}

int32 UParticleEmitter::GetModuleOffset(UParticleModule* Module) const
{
	const int32* Offset = ModuleOffsetMap.Find(Module);
	return Offset ? *Offset : 0;
}
