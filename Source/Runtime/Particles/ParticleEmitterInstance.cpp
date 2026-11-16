#include "EnginePCH.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleSpawn.h"
#include "Particles/ParticleModuleLifetime.h"

FParticleEmitterInstance::FParticleEmitterInstance(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent)
	: SpriteTemplate(InTemplate), Component(InComponent)
{
	// 일단 항상 0번 LOD를 쓴다
	CurrentLODLevel = SpriteTemplate->LODLevels[CurrentLODLevelIndex];
}

FParticleEmitterInstance::~FParticleEmitterInstance()
{
	// 할당한 ParticleData 해제
	_aligned_free(ParticleData);
}

void FParticleEmitterInstance::Init()
{
	ParticleSize = SpriteTemplate->ParticleSize;
	PayloadOffset = sizeof(FBaseParticle);
	ParticleStride = (ParticleSize + 15) & ~15;

	// MaxActiveParticles 어림
	for (const auto& Module : CurrentLODLevel->Modules)
	{
		if (const auto& Lifetime = Cast<UParticleModuleLifetime>(Module))
		{
			MaxActiveParticles = static_cast<int32>(CurrentLODLevel->SpawnModule->Rate * Lifetime->LifetimeMax);
			break;
		}
	}
	MaxActiveParticles = FMath::Clamp(MaxActiveParticles, 100, static_cast<int32>(UINT16_MAX));

	// ParticleData 할당
	ParticleData = (uint8*)_aligned_malloc(ParticleStride * MaxActiveParticles + sizeof(uint16) * MaxActiveParticles, 16);
	ParticleIndices = (uint16*)(ParticleData + ParticleStride * MaxActiveParticles);

	// Indices 초기화
	for (uint16 i = 0; i < MaxActiveParticles; ++i)
	{
		ParticleIndices[i] = i;
	}
	ActiveParticles = 0;
}
