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
		FBaseParticle* Particle = reinterpret_cast<FBaseParticle*>(ParticleData + ParticleStride * Slot);
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
	// TODO: Step 3 — 생성 순서대로 정렬한 점 목록을 FDynamicRibbonEmitterData로 넘긴다
	return nullptr;
}
