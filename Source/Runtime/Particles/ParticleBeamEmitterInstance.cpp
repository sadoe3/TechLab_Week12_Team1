#include "EnginePCH.h"
#include "Particles/ParticleBeamEmitterInstance.h"
#include "Particles/ParticleDynamicData.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleTypeDataBeam2.h"
#include "Particles/ParticleModuleBeamTarget.h"
#include "Particles/ParticleModuleBeamNoise.h"
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
	BeamModule_Noise = nullptr;
	NoiseOffsets.Empty();
	NoiseTargets.Empty();
	NoiseLockTimer = 0.0f;
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
		if (!BeamModule_Target)
		{
			BeamModule_Target = Cast<UParticleModuleBeamTarget>(Module);
		}
		if (!BeamModule_Noise)
		{
			BeamModule_Noise = Cast<UParticleModuleBeamNoise>(Module);
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
	// 노이즈의 빔 길이 방향 성분을 빼서, 점이 옆으로만 흔들리고 앞뒤로 겹치지 않게 한다
	const FVector BeamDir = Target.SizeSquared() > 1e-8f ? Target.Normalized() : FVector::ZeroVector;
	OutPoints.SetNum(PointNum);
	for (int32 i = 0; i < PointNum; ++i)
	{
		float Alpha = static_cast<float>(i) / Segments;
		FVector Noise = SampleNoise(Alpha);
		Noise = Noise - BeamDir * Noise.Dot(BeamDir);
		OutPoints[i].Location = FMath::Lerp(FVector::ZeroVector, Target, Alpha) + Noise;
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

void FParticleBeamEmitterInstance::Tick(float DeltaTime, bool bSuppressSpawning)
{
	FParticleEmitterInstance::Tick(DeltaTime, bSuppressSpawning);
	UpdateNoise(DeltaTime);
}

void FParticleBeamEmitterInstance::UpdateNoise(float DeltaTime)
{
	if (!BeamModule_Noise || !BeamModule_Noise->bLowFreq_Enabled || !Component)
	{
		NoiseOffsets.Empty();
		NoiseTargets.Empty();
		return;
	}

	// 에디터에서 Frequency를 바꾸면 배열 크기부터 다시 맞춘다
	const int32 NoisePointCount = FMath::Max(1, BeamModule_Noise->Frequency) + 1;
	if (NoiseOffsets.Num() != NoisePointCount)
	{
		NoiseOffsets.SetNum(NoisePointCount);
		NoiseTargets.SetNum(NoisePointCount);
		for (int32 i = 0; i < NoisePointCount; ++i)
		{
			NoiseOffsets[i] = FVector::ZeroVector;
			NoiseTargets[i] = FVector::ZeroVector;
		}
		NoiseLockTimer = 0.0f;
	}

	NoiseLockTimer -= DeltaTime;
	if (NoiseLockTimer <= 0.0f)
	{
		NoiseLockTimer = BeamModule_Noise->NoiseLockTime;
		const FVector& Range = BeamModule_Noise->NoiseRange;
		for (int32 i = 0; i < NoisePointCount; ++i)
		{
			// Source는 PSC에 붙어 있어야 하고, Target은 bTargetNoise일 때만 흔든다
			const bool bPinned = (i == 0) || (i == NoisePointCount - 1 && !BeamModule_Noise->bTargetNoise);
			NoiseTargets[i] = bPinned ? FVector::ZeroVector : FVector(
				(Component->GetRandomFraction() * 2.0f - 1.0f) * Range.X,
				(Component->GetRandomFraction() * 2.0f - 1.0f) * Range.Y,
				(Component->GetRandomFraction() * 2.0f - 1.0f) * Range.Z);
		}
	}

	const float Step = BeamModule_Noise->NoiseSpeed * DeltaTime;
	for (int32 i = 0; i < NoisePointCount; ++i)
	{
		const FVector Delta = NoiseTargets[i] - NoiseOffsets[i];
		const float Dist = Delta.Size();
		// 속도 0은 "즉시 점프"라 번개처럼 툭툭 바뀌고, 속도가 있으면 출렁이듯 움직인다
		if (BeamModule_Noise->NoiseSpeed <= 0.0f || Dist <= Step)
		{
			NoiseOffsets[i] = NoiseTargets[i];
		}
		else
		{
			NoiseOffsets[i] = NoiseOffsets[i] + Delta * (Step / Dist);
		}
	}
}

FVector FParticleBeamEmitterInstance::SampleNoise(float Alpha) const
{
	if (NoiseOffsets.Num() < 2)
	{
		return FVector::ZeroVector;
	}

	const int32 LastSegment = NoiseOffsets.Num() - 2;
	const float Scaled = Alpha * static_cast<float>(NoiseOffsets.Num() - 1);
	const int32 Index = FMath::Min(static_cast<int32>(Scaled), LastSegment);
	float T = Scaled - static_cast<float>(Index);
	if (BeamModule_Noise && BeamModule_Noise->bSmooth)
	{
		T = T * T * (3.0f - 2.0f * T);
	}
	return FMath::Lerp(NoiseOffsets[Index], NoiseOffsets[Index + 1], T);
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
	float HalfWidth = Beam->Size.X * 0.5f * MaxScale;
	// 수직 성분만 남기면 축별 크기는 바뀔 수 있지만 길이는 NoiseRange 벡터 길이를 넘지 않는다
	if (NoiseOffsets.Num() > 0 && BeamModule_Noise)
	{
		HalfWidth += BeamModule_Noise->NoiseRange.Size();
	}
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
