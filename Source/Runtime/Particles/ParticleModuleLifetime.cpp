#include "EnginePCH.h"
#include "Particles/ParticleModuleLifetime.h"
#include "Particles/BaseParticle.h"

UParticleModuleLifetime::UParticleModuleLifetime()
{
	bSpawnModule = true;
}

void UParticleModuleLifetime::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	FBaseParticle& Particle = *ParticleBase;

	const float MaxLifetime = RandomRange(Owner, LifetimeMin, LifetimeMax);
	Particle.OneOverMaxLifetime = MaxLifetime > 0.0f ? 1.0f / MaxLifetime : 0.0f;
	// 프레임 중간에 태어난 만큼 수명도 이미 진행된 상태로 시작한다 (Unreal UParticleModuleLifetime::SpawnEx)
	Particle.RelativeTime = SpawnTime * Particle.OneOverMaxLifetime;
}
