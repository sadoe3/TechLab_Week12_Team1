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
