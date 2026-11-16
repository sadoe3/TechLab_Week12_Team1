#include "EnginePCH.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleSpawn.h"
#include "Particles/ParticleModuleLifetime.h"
#include "Components/ParticleSystemComponent.h"

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

void FParticleEmitterInstance::Tick(float DeltaTime)
{
	// Spawn 개수 계산 후 SpawnParticles 호출
	float FloatCount = SpawnFraction + CurrentLODLevel->SpawnModule->Rate * DeltaTime;
	int32 SpawnCount = static_cast<int32>(FloatCount);
	SpawnFraction = FloatCount - static_cast<float>(SpawnCount);
	SpawnCount = FMath::Clamp(static_cast<int32>(FloatCount), 0, MaxActiveParticles - ActiveParticles);

	if (SpawnCount > 0)
	{
		SpawnParticles(SpawnCount, 0.f, DeltaTime / static_cast<float>(SpawnCount), Component->GetWorldLocation(), FVector(1.f, 1.f, 1.f));
	}
	// Step 4 Update/Kill
}

void FParticleEmitterInstance::SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity)
{
	for (int32 i = 0; i < Count; ++i)
	{
		DECLARE_PARTICLE_PTR(Particle, ParticleData + ParticleIndices[ActiveParticles] * ParticleStride)
		float SpawnTime = StartTime + Increment * (Count - (i + 1));

		PreSpawn(Particle, InitialLocation, InitialVelocity);

		for (const auto& Module : CurrentLODLevel->SpawnModules)
		{
			Module->Spawn(this, SpriteTemplate->ModuleOffsetMap[Module], SpawnTime, Particle);
		}

		PostSpawn(Particle, 0.f, SpawnTime);

		ActiveParticles++;
		ParticleCounter++;
	}
}

void FParticleEmitterInstance::PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity)
{
	// 슬롯을 0으로 밀고 Location/Velocity/RelativeTime 기본값 설정
	memset(Particle, 0, ParticleStride);
	Particle->Location = InitialLocation;
	Particle->Velocity = InitialVelocity;
	Particle->BaseVelocity = InitialVelocity;
	Particle->RelativeTime = 0.f;
	Particle->Size = FVector(1.f, 1.f, 1.f);
	Particle->Color = FLinearColor(1.f, 1.f, 1.f, 1.f);
}

void FParticleEmitterInstance::PostSpawn(FBaseParticle* Particle, float Interp, float SpawnTime)
{
	// SpawnTime만큼 Velocity로 위치를 앞당김
	Particle->Location += Particle->Velocity * SpawnTime;
}
