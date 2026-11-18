#include "EnginePCH.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleSystemFile.h"

int32 UParticleSystem::AddLODLevel(float Distance)
{
	if (Emitters.Num() == 0)
	{
		return -1;
	}

	const int32 NewIndex = LODDistances.Num();
	for (UParticleEmitter* Emitter : Emitters)
	{
		UParticleLODLevel* LOD0 = Emitter ? Emitter->GetLODLevel(0) : nullptr;
		if (!LOD0)
		{
			continue;
		}

		// LOD0을 복제해야 모듈 순서가 같아져 LOD0 기준 Payload 오프셋을 모든 LOD가 함께 쓸 수 있다
		UParticleLODLevel* NewLOD = ParticleSystemFile::DuplicateLODLevel(*Emitter, 0);
		NewLOD->Level = NewIndex;
		Emitter->AddLODLevel(NewLOD);
		Emitter->CacheEmitterModuleInfo();
	}

	LODDistances.Add(Distance);
	return NewIndex;
}

UParticleEmitter* UParticleSystem::GetEmitter(int32 Index) const
{
	if (Index < 0 || Index >= Emitters.Num())
	{
		return nullptr;
	}
	return Emitters[Index];
}

void UParticleSystem::AddEmitter(UParticleEmitter* InEmitter)
{
	if (InEmitter)
	{
		Emitters.Add(InEmitter);
	}
}

void UParticleSystem::InsertEmitter(UParticleEmitter* InEmitter, int32 Index)
{
	if (!InEmitter)
	{
		return;
	}

	if (Index < 0 || Index > Emitters.Num())
	{
		Emitters.Add(InEmitter);
		return;
	}
	Emitters.Insert(InEmitter, static_cast<uint32>(Index));
}

int32 UParticleSystem::FindEmitterIndex(const UParticleEmitter* InEmitter) const
{
	for (int32 Index = 0; Index < Emitters.Num(); ++Index)
	{
		if (Emitters[Index] == InEmitter)
		{
			return Index;
		}
	}
	return -1;
}

bool UParticleSystem::RemoveEmitter(UParticleEmitter* InEmitter)
{
	if (!InEmitter)
	{
		return false;
	}

	for (int32 Index = 0; Index < Emitters.Num(); ++Index)
	{
		if (Emitters[Index] == InEmitter)
		{
			Emitters.RemoveAt(Index);
			return true;
		}
	}
	return false;
}

bool UParticleSystem::RemoveEmitterAt(int32 Index)
{
	if (Index < 0 || Index >= Emitters.Num())
	{
		return false;
	}
	Emitters.RemoveAt(Index);
	return true;
}

void UParticleSystem::ClearEmitters()
{
	Emitters.Empty();
}

void UParticleSystem::UpdateAllModuleLists()
{
	for (UParticleEmitter* Emitter : Emitters)
	{
		if (Emitter)
		{
			Emitter->CacheEmitterModuleInfo();
		}
	}
}
