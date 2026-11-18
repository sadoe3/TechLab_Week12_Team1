#include "EnginePCH.h"
#include "Particles/ParticleModuleRotationRate.h"
#include "Particles/BaseParticle.h"

UParticleModuleRotationRate::UParticleModuleRotationRate()
{
	bSpawnModule = true;
}

void UParticleModuleRotationRate::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	// BaseRotationRate에도 넣어야 ResetParticleParameters가 매 프레임 RotationRate를 이 값으로 되돌린다.
	const FVector Rate = GetDistributionValue(Owner, StartRotationRate);
	ParticleBase->RotationRate += Rate;
	ParticleBase->BaseRotationRate += Rate;
}
