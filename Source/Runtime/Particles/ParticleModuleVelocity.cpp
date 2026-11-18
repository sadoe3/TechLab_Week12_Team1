#include "EnginePCH.h"
#include "Particles/ParticleModuleVelocity.h"
#include "Particles/BaseParticle.h"

UParticleModuleVelocity::UParticleModuleVelocity()
{
	bSpawnModule = true;
}

void UParticleModuleVelocity::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	FBaseParticle& Particle = *ParticleBase;

	// Unreal UParticleModuleVelocity::SpawnEx
	FVector Vel = GetDistributionValue(Owner, StartVelocity);
	const FVector FromOrigin = (Particle.Location - GetSimulationOrigin(Owner)).GetSafeNormal();

	Vel = bInWorldSpace ? WorldToSimulationVector(Owner, Vel) : EmitterToSimulationVector(Owner, Vel);
	Vel += FromOrigin * GetDistributionValue(Owner, StartVelocityRadial);

	// Base에도 더해야 ResetParticleParameters가 매 프레임 이 속도로 되돌린다.
	Particle.Velocity += Vel;
	Particle.BaseVelocity += Vel;
}
