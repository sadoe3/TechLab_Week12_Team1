#include "EnginePCH.h"
#include "Particles/ParticleBeamEmitterInstance.h"
#include "Particles/ParticleDynamicData.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleTypeDataBeam2.h"
#include "Particles/ParticleModuleBeamTarget.h"
#include "Particles/ParticleModuleRequired.h"
#include "Asset/AssetManager.h"
#include "Components/ParticleSystemComponent.h"

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
	FVector Target;
	if (ActiveParticles == 0 || !GetLocalTarget(Target))
	{
		return;
	}

	const FBaseParticle* Beam = GetParticleAtIndex(0);
	int32 Segments = FMath::Max(1, BeamTypeData->Segments);
	int32 PointNum = Segments + 1;
	OutPoints.SetNum(PointNum);
	for (int32 i = 0; i < PointNum; ++i)
	{
		float Alpha = static_cast<float>(i) / Segments;
		OutPoints[i].Location = FMath::Lerp(FVector::ZeroVector, Target, Alpha);
		OutPoints[i].Color = Beam->Color;
		OutPoints[i].Width = Beam->Size.X * GetTaperScale(Alpha);
	}
}

bool FParticleBeamEmitterInstance::GetLocalTarget(FVector& OutTarget) const
{
	if (!BeamTypeData)
	{
		return false;
	}

	if (BeamTypeData->BeamMethod == EBeam2Method::PEBM2_Distance)
	{
		OutTarget = FVector(BeamTypeData->Distance, 0.0f, 0.0f);
		return true;
	}

	if (!BeamModule_Target)
	{
		return false;
	}
	OutTarget = BeamModule_Target->TargetLocation;
	// 빔 점은 항상 로컬로 넘기므로 월드 끝점은 PSC 로컬로 되돌린다
	if (BeamModule_Target->bTargetAbsolute && Component)
	{
		OutTarget = Component->GetWorldMatrix().Inverse().TransformPosition(OutTarget);
	}
	return true;
}

float FParticleBeamEmitterInstance::GetTaperScale(float Alpha) const
{
	if (!BeamTypeData || BeamTypeData->TaperMethod == EBeamTaperMethod::PEBTM_None)
	{
		return 1.0f;
	}
	return FMath::Lerp(1.0f, BeamTypeData->TaperFactor, Alpha);
}

void FParticleBeamEmitterInstance::UpdateBoundingBox(float DeltaTime)
{
	// 기반 버전이 파티클 적분도 하므로 먼저 호출한다
	FParticleEmitterInstance::UpdateBoundingBox(DeltaTime);

	FVector Target;
	if (ActiveParticles <= 0 || !GetLocalTarget(Target))
	{
		return;
	}

	// 빔 파티클은 원점 하나뿐이라, 실제로 그려지는 Source~Target 선분을 박스에 넣어야 컬링되지 않는다
	const FBaseParticle* Beam = GetParticleAtIndex(0);
	// 테이퍼는 양 끝 사이에서 선형이라 두 끝 중 넓은 쪽이 최대 폭이다
	const float MaxScale = FMath::Max(GetTaperScale(0.0f), GetTaperScale(1.0f));
	const float HalfWidth = Beam->Size.X * 0.5f * MaxScale;
	const FVector Pad(HalfWidth, HalfWidth, HalfWidth);
	FBox BeamBox = { FMath::Min(FVector::ZeroVector, Target) - Pad, FMath::Max(FVector::ZeroVector, Target) + Pad };

	// 빔 점은 항상 로컬이지만, 박스는 RequiredModule 공간(월드일 수 있음)으로 맞춰야 PSC가 올바로 합친다
	if (!IsLocalSpace() && Component)
	{
		BeamBox = BeamBox.GetWorldAABB(Component->GetWorldMatrix());
	}
	ParticleBoundingBox.Expand(BeamBox);
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
	// Ribbon 렌더 경로의 거리 기준 UV를 그대로 쓴다. U는 Source부터 누적한 거리 / TextureTileDistance
	Data->Source.TilingDistance = BeamTypeData ? BeamTypeData->TextureTileDistance : 0.0f;

	BuildBeamPoints(Data->Source.Points);

	Data->Source.ActiveParticleCount = Data->Source.Points.Num();
	if (Data->Source.ActiveParticleCount < 2)
	{
		return nullptr;
	}
	return Data;
}
