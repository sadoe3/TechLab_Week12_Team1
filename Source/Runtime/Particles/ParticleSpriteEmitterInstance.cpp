#include "EnginePCH.h"
#include "Particles/ParticleSpriteEmitterInstance.h"
#include "Asset/AssetManager.h"
#include "Particles/ParticleDynamicData.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleSubUV.h"
#include "UObject/Casts.h"
#include "Render/RenderPacket.h"

FParticleSpriteEmitterInstance::FParticleSpriteEmitterInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent)
{
	InitParameters(InEmitterParent, InComponent);
}

void FParticleSpriteEmitterInstance::Init()
{
	FParticleEmitterInstance::Init();
}

void FParticleSpriteEmitterInstance::Tick(float DeltaTime, bool bSuppressSpawning)
{
	FParticleEmitterInstance::Tick(DeltaTime, bSuppressSpawning);
}

void FParticleSpriteEmitterInstance::UpdateBoundingBox(float DeltaTime)
{
	FParticleEmitterInstance::UpdateBoundingBox(DeltaTime);
}

UMaterial* FParticleSpriteEmitterInstance::GetMaterial() const
{
	if (CurrentLODLevel && CurrentLODLevel->RequiredModule)
	{
		return CurrentLODLevel->RequiredModule->Material;
	}
	return nullptr;
}

void FParticleSpriteEmitterInstance::BuildBillboardMatrices(const FViewContext& View, const FMatrix& ComponentToWorld, TArray<FMatrix>& OutMatrices) const
{
	OutMatrices.Empty();
	if (ActiveParticles <= 0)
	{
		return;
	}

	const bool bUseLocalSpace = (CurrentLODLevel && CurrentLODLevel->RequiredModule)
		? CurrentLODLevel->RequiredModule->bUseLocalSpace
		: false;

	OutMatrices.Reserve(ActiveParticles);
	for (int32 i = 0; i < ActiveParticles; ++i)
	{
		const FBaseParticle* Particle = GetParticleAtIndex(i);
		if (!Particle)
		{
			continue;
		}

		FVector WorldPos = Particle->Location;
		if (bUseLocalSpace)
		{
			const FVector4 Transformed = FVector4(Particle->Location.X, Particle->Location.Y, Particle->Location.Z, 1.0f) * ComponentToWorld;
			WorldPos = FVector(Transformed.X, Transformed.Y, Transformed.Z);
		}

		OutMatrices.Add(View.BuildBillboardMatrix(WorldPos, Particle->Size.X, Particle->Size.Y));
	}
}

TUniquePtr<FDynamicEmitterDataBase> FParticleSpriteEmitterInstance::GetDynamicData(bool bSelected)
{
	if (!bEnabled || ActiveParticles <= 0)
	{
		return nullptr;
	}

	UStaticMesh* QuadMesh = UAssetManager::GetAssetByPath<UStaticMesh>("ParticleQuad");
	UMaterial* SpriteMat = GetMaterial();
	if (!SpriteMat)
	{
		SpriteMat = UAssetManager::GetAssetByPath<UMaterial>("BillboardMaterial");
	}
	if (!QuadMesh || !SpriteMat)
	{
		return nullptr;
	}

	auto Data = MakeUnique<FDynamicSpriteEmitterData>();
	Data->bSelected = bSelected;
	Data->QuadMesh = QuadMesh;
	Data->Material = SpriteMat;
	Data->Source.bUseLocalSpace = (CurrentLODLevel && CurrentLODLevel->RequiredModule)
		? CurrentLODLevel->RequiredModule->bUseLocalSpace
		: false;
	Data->Source.SortMode = (CurrentLODLevel && CurrentLODLevel->RequiredModule)
		? static_cast<uint8>(CurrentLODLevel->RequiredModule->SortMode)
		: 0;

	// SubUV 모듈의 Payload 위치 (Unreal FParticleSpriteEmitterInstance::SubUVDataOffset). 없거나 꺼져 있으면 0 = 아틀라스 0번 칸.
	int32 SubUVDataOffset = 0;
	if (CurrentLODLevel && SpriteTemplate)
	{
		for (UParticleModule* Module : CurrentLODLevel->GetModules())
		{
			if (Module && Module->bEnabled && Cast<UParticleModuleSubUV>(Module))
			{
				SubUVDataOffset = SpriteTemplate->GetModuleOffset(Module);
				break;
			}
		}
	}

	Data->Source.Particles.Reserve(ActiveParticles);
	for (int32 i = 0; i < ActiveParticles; ++i)
	{
		const FBaseParticle* Particle = GetParticleAtIndex(i);
		if (!Particle)
		{
			continue;
		}
		FDynamicSpriteEmitterReplayData::FSpriteParticleSnapshot& Snapshot = Data->Source.Particles.AddDefaulted_GetRef();
		Snapshot.Location = Particle->Location;
		Snapshot.Size = Particle->Size;
		Snapshot.Color = Particle->Color;
		Snapshot.RelativeTime = Particle->RelativeTime;
		Snapshot.Rotation = Particle->Rotation.Z; // 스프라이트는 Roll만 쓴다
		Snapshot.SubImageIndex = UParticleModuleSubUV::GetImageIndex(*Particle, SubUVDataOffset);
	}

	Data->Source.ActiveParticleCount = Data->Source.Particles.Num();
	if (Data->Source.ActiveParticleCount == 0)
	{
		return nullptr;
	}
	if (CurrentLODLevel && CurrentLODLevel->RequiredModule)
	{
		Data->SubImagesHorizontal = CurrentLODLevel->RequiredModule->SubImages_Horizontal;
		Data->SubImagesVertical = CurrentLODLevel->RequiredModule->SubImages_Vertical;
	}
	Data->BuildMaterialParams();
	return Data;
}
