#include "EnginePCH.h"
#include "Particles/ParticleBeamEmitterInstance.h"
#include "Particles/ParticleDynamicData.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleTypeDataBeam2.h"
#include "Particles/ParticleModuleBeamTarget.h"
#include "Particles/ParticleModuleRequired.h"
#include "Asset/AssetManager.h"

FParticleBeamEmitterInstance::FParticleBeamEmitterInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent, UParticleModuleTypeDataBeam2* InBeamTypeData)
	: BeamTypeData(InBeamTypeData)
{
	InitParameters(InEmitterParent, InComponent);
}

void FParticleBeamEmitterInstance::Init()
{
	FParticleEmitterInstance::Init();

	BeamModule_Target = nullptr;
	if (!CurrentLODLevel)
	{
		return;
	}

	if (!BeamTypeData)
	{
		BeamTypeData = Cast<UParticleModuleTypeDataBeam2>(CurrentLODLevel->TypeDataModule);
	}
	for (UParticleModule* Module : CurrentLODLevel->GetModules())
	{
		if (UParticleModuleBeamTarget* Target = Cast<UParticleModuleBeamTarget>(Module))
		{
			BeamModule_Target = Target;
			break;
		}
	}
}

UMaterial* FParticleBeamEmitterInstance::GetMaterial() const
{
	if (CurrentLODLevel && CurrentLODLevel->RequiredModule && CurrentLODLevel->RequiredModule->Material)
	{
		return CurrentLODLevel->RequiredModule->Material;
	}
	return UAssetManager::GetAssetByPath<UMaterial>("RibbonMaterial");
}

TUniquePtr<FDynamicEmitterDataBase> FParticleBeamEmitterInstance::GetDynamicData(bool bSelected)
{
	// TODO: Step 2~4 — Source(PSC)→Target 분할 점을 만들어 FDynamicBeam2EmitterData 스냅샷으로 넘긴다
	return nullptr;
}
