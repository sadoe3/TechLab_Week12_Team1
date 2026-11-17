#include "EnginePCH.h"
#include "Particles/ParticleSpriteEmitterInstance.h"
#include "Asset/AssetManager.h"
#include "Particles/ParticleDynamicData.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleTypeDataSprite.h"
#include "Render/RenderPacket.h"

FParticleSpriteEmitterInstance::FParticleSpriteEmitterInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent, UParticleModuleTypeDataSprite* InSpriteTypeData)
	: SpriteTypeData(InSpriteTypeData)
{
	InitParameters(InEmitterParent, InComponent);
}

void FParticleSpriteEmitterInstance::Init()
{
	FParticleEmitterInstance::Init();

	if (!SpriteTypeData && CurrentLODLevel)
	{
		SpriteTypeData = Cast<UParticleModuleTypeDataSprite>(CurrentLODLevel->TypeDataModule);
	}
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

	Data->Source.Particles.Reserve(ActiveParticles);
	for (int32 i = 0; i < ActiveParticles; ++i)
	{
		const FBaseParticle* Particle = GetParticleAtIndex(i);
		if (!Particle)
		{
			continue;
		}
		Data->Source.Particles.Add({ Particle->Location, Particle->Size, Particle->Color });
	}

	Data->Source.ActiveParticleCount = Data->Source.Particles.Num();
	if (Data->Source.ActiveParticleCount == 0)
	{
		return nullptr;
	}
	if (SpriteTypeData)
	{
		Data->SubImagesHorizontal = SpriteTypeData->SubImages_Horizontal;
		Data->SubImagesVertical = SpriteTypeData->SubImages_Vertical;
	}
	Data->BuildParticleParams();
	return Data;
}
