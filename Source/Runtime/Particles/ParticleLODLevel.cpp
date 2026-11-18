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
		// Unreal과 같이 FinalUpdate 모듈도 UpdateModules에 넣고, 호출 시 플래그로 구분한다.
		if (Module->bUpdateModule || Module->bFinalUpdateModule)
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

void UParticleLODLevel::InsertModule(UParticleModule* InModule, int32 Index)
{
	if (!InModule)
	{
		return;
	}

	if (Index < 0 || Index > Modules.Num())
	{
		Modules.Add(InModule);
		return;
	}
	Modules.Insert(InModule, static_cast<uint32>(Index));
}

UParticleModule* UParticleLODLevel::RemoveModuleAt(int32 Index)
{
	if (Index < 0 || Index >= Modules.Num())
	{
		return nullptr;
	}

	UParticleModule* Module = Modules[Index];
	Modules.RemoveAt(static_cast<uint32>(Index));
	return Module;
}

int32 UParticleLODLevel::FindModuleIndex(const UParticleModule* InModule) const
{
	for (int32 Index = 0; Index < Modules.Num(); ++Index)
	{
		if (Modules[Index] == InModule)
		{
			return Index;
		}
	}
	return -1;
}

void UParticleLODLevel::ClearModules()
{
	Modules.Empty();
	SpawnModules.Empty();
	UpdateModules.Empty();
}
