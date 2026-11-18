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

void FParticleBeamEmitterInstance::BuildBeamPoints(TArray<FDynamicRibbonEmitterReplayData::FRibbonPoint>& OutPoints) const
{
	if (ActiveParticles == 0 || !BeamTypeData || !BeamModule_Target)
	{
		return;
	}

	const FBaseParticle* Beam = GetParticleAtIndex(0);
	int32 Segments = FMath::Max(1, BeamTypeData->Segments);
	int32 PointNum = Segments + 1;
	FVector Target = BeamModule_Target->TargetLocation;
	OutPoints.SetNum(PointNum);
	for (int32 i = 0; i < PointNum; ++i)
	{
		float Alpha = static_cast<float>(i) / Segments;
		OutPoints[i].Location = FMath::Lerp(FVector::ZeroVector, Target, Alpha);
		OutPoints[i].Color = Beam->Color;
		OutPoints[i].Width = Beam->Size.X;
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
	// BuildBeamPoints로 점을 만들어 FDynamicBeam2EmitterData 스냅샷으로 넘긴다 

	if (!bEnabled || ActiveParticles == 0)
	{
		return nullptr;
	}
	if (!RenderBuffers)
	{
		RenderBuffers = MakeShared<FRibbonRenderBuffers>();
	}

	auto Data = MakeUnique<FDynamicBeam2EmitterData>();
	Data->bSelected = bSelected;
	Data->Material = GetMaterial();
	Data->Source.bUseLocalSpace = true;
	Data->Buffers = RenderBuffers;

	BuildBeamPoints(Data->Source.Points);

	Data->Source.ActiveParticleCount = Data->Source.Points.Num();
	if (Data->Source.ActiveParticleCount < 2)
	{
		return nullptr;
	}
	return Data;
}
