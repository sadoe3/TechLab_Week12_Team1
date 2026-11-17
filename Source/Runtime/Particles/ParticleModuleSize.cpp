#include "EnginePCH.h"
#include "Particles/ParticleModuleSize.h"
#include "Particles/BaseParticle.h"

UParticleModuleSize::UParticleModuleSize()
{
	bSpawnModule = true;
}

void UParticleModuleSize::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	// Unreal은 PreSpawn에서 Size가 0이고 이 모듈이 더한다(+=).
	// 이 엔진은 Size 모듈 없이도 보이도록 기본 Size가 1이므로 대입한다.
	const FVector Size = RandomRange(Owner, StartSizeMin, StartSizeMax);
	ParticleBase->Size = Size;
	ParticleBase->BaseSize = Size;
}
