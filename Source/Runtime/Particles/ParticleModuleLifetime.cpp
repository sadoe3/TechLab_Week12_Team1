#include "EnginePCH.h"
#include "Particles/ParticleModuleLifetime.h"
#include "Particles/ParticleHelper.h"

UParticleModuleLifetime::UParticleModuleLifetime()
{
	// 태어날 때 수명을 한 번 정하면 끝이고, 나이 먹이기와 소멸은 Instance가 맡는다
	bSpawnModule = true;
	bUpdateModule = false;
}

void UParticleModuleLifetime::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	// Min~Max에서 수명을 뽑아 OneOverMaxLifetime에 기록 (0이면 0)
	float Lifetime = RandRange(LifetimeMin, LifetimeMax);
	ParticleBase->OneOverMaxLifetime = Lifetime > 0.f ? 1 / Lifetime : 0.f;
}
