#include "EnginePCH.h"
#include "Particles/ParticleModuleRotation.h"
#include "Particles/BaseParticle.h"

UParticleModuleRotation::UParticleModuleRotation()
{
	bSpawnModule = true;
}

void UParticleModuleRotation::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	// Unreal과 같이 더한다(+=). 스폰 직전 Rotation은 0이고, 같은 모듈을 여러 개 두면 합쳐진다.
	ParticleBase->Rotation += GetDistributionValue(Owner, StartRotation);
}
