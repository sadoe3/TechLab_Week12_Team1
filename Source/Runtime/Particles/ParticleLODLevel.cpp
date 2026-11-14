#include "EnginePCH.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleSpawn.h"
#include "Particles/ParticleModuleTypeDataBase.h"

UParticleLODLevel::UParticleLODLevel()
{
}

void UParticleLODLevel::UpdateModuleLists()
{
	SpawnModules.Reset();
	UpdateModules.Reset();
	for (const auto& Module : Modules)
	{
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
