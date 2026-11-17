#include "EnginePCH.h"
#include "Particles/ParticleRibbonEmitterInstance.h"
#include "Particles/ParticleDynamicData.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleTypeDataRibbon.h"

FParticleRibbonEmitterInstance::FParticleRibbonEmitterInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent, UParticleModuleTypeDataRibbon* InRibbonTypeData)
	: RibbonTypeData(InRibbonTypeData)
{
	InitParameters(InEmitterParent, InComponent);
}

void FParticleRibbonEmitterInstance::Init()
{
	FParticleEmitterInstance::Init();

	if (!RibbonTypeData && CurrentLODLevel)
	{
		RibbonTypeData = Cast<UParticleModuleTypeDataRibbon>(CurrentLODLevel->TypeDataModule);
	}
}

TUniquePtr<FDynamicEmitterDataBase> FParticleRibbonEmitterInstance::GetDynamicData(bool bSelected)
{
	// TODO: Step 3 — 생성 순서대로 정렬한 점 목록을 FDynamicRibbonEmitterData로 넘긴다
	return nullptr;
}
