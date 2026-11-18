#include "EnginePCH.h"
#include "Particles/ParticleRibbonEmitterInstance.h"
#include "Particles/ParticleDynamicData.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleTypeDataRibbon.h"
#include "Particles/ParticleModuleRequired.h"
#include "Asset/AssetManager.h"

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
	HeadSlot = INDEX_NONE;
}

FRibbonTypeDataPayload* FParticleRibbonEmitterInstance::GetRibbonPayload(int32 SlotIndex) const
{
	return reinterpret_cast<FRibbonTypeDataPayload*>(ParticleData + static_cast<size_t>(SlotIndex) * ParticleStride + TypeDataOffset);
}

FBaseParticle* FParticleRibbonEmitterInstance::GetParticleAtSlot(int32 SlotIndex) const
{
	return reinterpret_cast<FBaseParticle*>(ParticleData + static_cast<size_t>(SlotIndex) * ParticleStride);
}

UMaterial* FParticleRibbonEmitterInstance::GetMaterial() const
{
	// Ribbon vertices (FParticleRibbonVertex) only match the Ribbon PSO. A sprite/mesh material left in
	// Required (e.g. after switching TypeData in the editor) would read the vertex buffer with the wrong layout.
	if (CurrentLODLevel && CurrentLODLevel->RequiredModule)
	{
		UMaterial* Material = CurrentLODLevel->RequiredModule->Material;
		if (Material && Material->PSOType == EPSOType::Particle_Ribbon)
		{
			return Material;
		}
	}
	return UAssetManager::GetAssetByPath<UMaterial>("RibbonMaterial");
}

void FParticleRibbonEmitterInstance::SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity)
{
	// 기반 클래스는 새 파티클을 ParticleIndices[ActiveParticles]부터 차례로 채운다
	const int32 FirstNewIndex = ActiveParticles;
	FParticleEmitterInstance::SpawnParticles(Count, StartTime, Increment, InitialLocation, InitialVelocity);

	// ParticleIndices[FirstNewIndex .. ActiveParticles) 의 새 파티클을 태어난 순서대로 머리에 연결
	for (int32 i = FirstNewIndex; i < ActiveParticles; ++i)
	{
		const int32 Slot = ParticleIndices[i];
		FRibbonTypeDataPayload* Ribbon = GetRibbonPayload(Slot);

		// memset이 0을 써 두었는데 0도 유효한 슬롯이라 반드시 직접 덮어쓴다
		Ribbon->Prev = INDEX_NONE;
		Ribbon->Next = HeadSlot;

		if (HeadSlot != INDEX_NONE)
		{
			GetRibbonPayload(HeadSlot)->Prev = Slot;
		}

		// 위에서 예전 머리를 Next에 다 쓴 뒤에 바꿔야 링크가 끊기지 않는다
		HeadSlot = Slot;
	}
}

void FParticleRibbonEmitterInstance::KillParticles()
{
	// 수명이 끝난 파티클의 Prev/Next 이웃을 서로 잇고, 머리가 죽으면 HeadSlot을 옮긴다
	for (int32 i = 0; i < ActiveParticles; ++i)
	{
		const int32 Slot = ParticleIndices[i];
		FBaseParticle* Particle = GetParticleAtSlot(Slot);
		FRibbonTypeDataPayload* Dead = GetRibbonPayload(Slot);

		if (Particle->RelativeTime <= 1.f)
		{
			continue;
		}

		if (Dead->Prev != INDEX_NONE)
		{
			GetRibbonPayload(Dead->Prev)->Next = Dead->Next;
		}
		if (Dead->Next != INDEX_NONE)
		{
			GetRibbonPayload(Dead->Next)->Prev = Dead->Prev;
		}
		if (Slot == HeadSlot)
		{
			HeadSlot = Dead->Next;
		}
	}
	FParticleEmitterInstance::KillParticles();
}

void FParticleRibbonEmitterInstance::KillParticlesForced(bool bFireEvents)
{
	FParticleEmitterInstance::KillParticlesForced(bFireEvents);
	HeadSlot = INDEX_NONE;
}

TUniquePtr<FDynamicEmitterDataBase> FParticleRibbonEmitterInstance::GetDynamicData(bool bSelected)
{
	// 띠는 점 2개부터 만들어진다
	if (!bEnabled || ActiveParticles < 2 || HeadSlot == INDEX_NONE)
	{
		return nullptr;
	}

	UMaterial* Material = GetMaterial();
	if (!Material)
	{
		return nullptr;
	}

	if (!RenderBuffers)
	{
		RenderBuffers = MakeShared<FRibbonRenderBuffers>();
	}

	auto Data = MakeUnique<FDynamicRibbonEmitterData>();
	Data->bSelected = bSelected;
	Data->Material = Material;
	Data->Buffers = RenderBuffers;
	Data->Source.bUseLocalSpace = IsLocalSpace();

	// ParticleIndices 순서는 swap-and-pop으로 섞여 있으므로 머리부터 Next 링크를 따라간다
	Data->Source.Points.Reserve(ActiveParticles);
	for (int32 Slot = HeadSlot; Slot != INDEX_NONE && Data->Source.Points.Num() < ActiveParticles; Slot = GetRibbonPayload(Slot)->Next)
	{
		const FBaseParticle* Particle = GetParticleAtSlot(Slot);
		FDynamicRibbonEmitterReplayData::FRibbonPoint& Point = Data->Source.Points.AddDefaulted_GetRef();
		Point.Location = Particle->Location;
		Point.Width = Particle->Size.X;
		Point.Color = Particle->Color;
	}

	Data->Source.ActiveParticleCount = Data->Source.Points.Num();
	if (Data->Source.ActiveParticleCount < 2)
	{
		return nullptr;
	}
	return Data;
}
