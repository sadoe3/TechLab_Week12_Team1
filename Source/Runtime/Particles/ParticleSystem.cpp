#include "EnginePCH.h"
#include "Particles/ParticleSystem.h"

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
