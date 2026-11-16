#include "EnginePCH.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleLifetime.h"

UParticleSystem::UParticleSystem()
{
}

UParticleSystem* UParticleSystem::GetDefault()
{
	static UParticleSystem* DefaultSystem = nullptr;
	if (DefaultSystem)
	{
		return DefaultSystem;
	}

	// 에셋은 특정 PSC 소유가 아니므로 Outer 없이 만든다
	UParticleSystem* NewSystem = NewObject<UParticleSystem>(nullptr);

	// Outer 지정과 별개로 부모 배열에 직접 넣어야 순회된다
	UParticleEmitter* NewEmitter = NewObject<UParticleEmitter>(NewSystem);
	NewEmitter->EmitterName = FName("Particle Emitter");
	NewSystem->Emitters.Add(NewEmitter);

	// Required/Spawn 모듈은 CreateLODLevel이 함께 만든다
	UParticleLODLevel* LODLevel = NewEmitter->CreateLODLevel();

	UParticleModuleLifetime* LifetimeModule = NewObject<UParticleModuleLifetime>(LODLevel);
	LODLevel->Modules.Add(LifetimeModule);

	// 빠뜨리면 SpawnModules가 비고 ParticleSize도 0으로 남는다
	NewEmitter->UpdateModuleLists();

	DefaultSystem = NewSystem;
	return DefaultSystem;
}
