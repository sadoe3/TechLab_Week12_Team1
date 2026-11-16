#include "EnginePCH.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleSpawn.h"
#include "Particles/ParticleModuleTypeDataBase.h"

void UParticleLODLevel::UpdateModuleLists()
{
	SpawnModules.Empty();
	UpdateModules.Empty();

	for (UParticleModule* Module : Modules)
	{
		if (!Module || !Module->bEnabled)
		{
			continue;
		}

		if (Module->bSpawnModule)
		{
			SpawnModules.Add(Module);
		}
		if (Module->bUpdateModule)
		{
			UpdateModules.Add(Module);
		}
	}
}

void UParticleLODLevel::AddModule(UParticleModule* InModule)
{
	if (InModule)
	{
		Modules.Add(InModule);
	}
}

bool UParticleLODLevel::RemoveModule(UParticleModule* InModule)
{
	if (!InModule)
	{
		return false;
	}

	for (int32 Index = 0; Index < Modules.Num(); ++Index)
	{
		if (Modules[Index] == InModule)
		{
			Modules.RemoveAt(Index);
			return true;
		}
	}
	return false;
}

void UParticleLODLevel::ClearModules()
{
	Modules.Empty();
	SpawnModules.Empty();
	UpdateModules.Empty();
}
