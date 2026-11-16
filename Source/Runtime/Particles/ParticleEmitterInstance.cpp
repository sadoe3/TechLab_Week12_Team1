#include "EnginePCH.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleSpawn.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>

FParticleEmitterInstance::~FParticleEmitterInstance()
{
	if (ParticleData)
	{
		std::free(ParticleData);
		ParticleData = nullptr;
	}
	if (ParticleIndices)
	{
		std::free(ParticleIndices);
		ParticleIndices = nullptr;
	}
	ActiveParticles = 0;
	MaxActiveParticles = 0;
}

void FParticleEmitterInstance::InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent)
{
	SpriteTemplate = InTemplate;
	Component = InComponent;
	CurrentLODLevelIndex = 0;
	CurrentLODLevel = SpriteTemplate ? SpriteTemplate->GetLODLevel(0) : nullptr;
}

void FParticleEmitterInstance::Init()
{
	ParticleSize = sizeof(FBaseParticle);
	ParticleStride = sizeof(FBaseParticle);

	if (SpriteTemplate)
	{
		SpriteTemplate->CacheEmitterModuleInfo();
		CurrentLODLevel = SpriteTemplate->GetLODLevel(CurrentLODLevelIndex);
	}

	if (CurrentLODLevel && CurrentLODLevel->RequiredModule)
	{
		EmitterDuration = CurrentLODLevel->RequiredModule->EmitterDuration;
	}

	int32 InitialAlloc = 32;
	if (SpriteTemplate && SpriteTemplate->GetInitialAllocationCount() > 0)
	{
		InitialAlloc = SpriteTemplate->GetInitialAllocationCount();
	}
	else if (CurrentLODLevel && CurrentLODLevel->PeakActiveParticles > 0)
	{
		InitialAlloc = CurrentLODLevel->PeakActiveParticles;
	}

	Resize(InitialAlloc, true);
	EmitterTime = 0.0f;
	LoopCount = 0;
	SpawnFraction = 0.0f;
	bEmitterIsDone = false;
}

void FParticleEmitterInstance::Resize(int32 NewMaxActiveParticles, bool bSetMaxActiveCount)
{
	if (NewMaxActiveParticles <= MaxActiveParticles)
	{
		return;
	}

	const int32 OldMax = MaxActiveParticles;
	uint8* NewData = static_cast<uint8*>(std::realloc(ParticleData, static_cast<size_t>(NewMaxActiveParticles) * ParticleStride));
	uint16* NewIndices = static_cast<uint16*>(std::realloc(ParticleIndices, static_cast<size_t>(NewMaxActiveParticles) * sizeof(uint16)));

	if (!NewData || !NewIndices)
	{
		return;
	}

	ParticleData = NewData;
	ParticleIndices = NewIndices;

	for (int32 i = OldMax; i < NewMaxActiveParticles; ++i)
	{
		ParticleIndices[i] = static_cast<uint16>(i);
	}

	MaxActiveParticles = NewMaxActiveParticles;
}

void FParticleEmitterInstance::Tick(float DeltaTime, bool bSuppressSpawning)
{
	if (!bEnabled || !CurrentLODLevel || !CurrentLODLevel->bEnabled)
	{
		return;
	}

	const UParticleModuleRequired* Required = CurrentLODLevel->RequiredModule;
	const float Duration = (Required && Required->EmitterDuration > 0.0f) ? Required->EmitterDuration : 1.0f;
	const int32 MaxLoops = Required ? Required->EmitterLoops : 0;

	EmitterTime += DeltaTime;
	if (EmitterTime >= Duration)
	{
		if (MaxLoops == 0 || LoopCount + 1 < MaxLoops)
		{
			EmitterTime -= Duration;
			++LoopCount;
		}
		else
		{
			bEmitterIsDone = true;
		}
	}

	// 1. 기존 활성 파티클 수명/위치 업데이트
	for (int32 i = 0; i < ActiveParticles; ++i)
	{
		const uint16 Index = ParticleIndices[i];
		FBaseParticle* Particle = reinterpret_cast<FBaseParticle*>(ParticleData + static_cast<size_t>(Index) * ParticleStride);

		Particle->OldLocation = Particle->Location;
		Particle->RelativeTime += DeltaTime * Particle->OneOverMaxLifetime;
		Particle->Location += Particle->Velocity * DeltaTime;
		Particle->Rotation += Particle->RotationRate * DeltaTime;
	}

	// 2. 모듈 Update 실행
	const TArray<UParticleModule*>& UpdateModules = CurrentLODLevel->GetUpdateModules();
	for (UParticleModule* Module : UpdateModules)
	{
		if (Module && Module->bEnabled)
		{
			Module->Update(this, 0, DeltaTime);
		}
	}

	// 3. 수명이 다한 파티클 제거
	KillParticles();

	// 4. 새 파티클 스폰
	if (!bSuppressSpawning && !bEmitterIsDone)
	{
		SpawnFraction = Spawn(DeltaTime);
	}

	// 5. 바운딩 박스 갱신
	UpdateBoundingBox(DeltaTime);
}

float FParticleEmitterInstance::Spawn(float DeltaTime)
{
	if (!CurrentLODLevel || !CurrentLODLevel->SpawnModule)
	{
		return SpawnFraction;
	}

	const float Rate = CurrentLODLevel->SpawnModule->GetSpawnRate() * CurrentLODLevel->SpawnModule->RateScale;
	if (Rate <= 0.0f)
	{
		return SpawnFraction;
	}

	const float TotalSpawn = SpawnFraction + Rate * DeltaTime;
	const int32 SpawnCount = static_cast<int32>(TotalSpawn);
	const float Leftover = TotalSpawn - static_cast<float>(SpawnCount);

	if (SpawnCount > 0)
	{
		const float Increment = DeltaTime / static_cast<float>(SpawnCount);
		SpawnParticles(SpawnCount, 0.0f, Increment, FVector::ZeroVector, FVector(0.0f, 0.0f, 100.0f));
	}

	return Leftover;
}

void FParticleEmitterInstance::SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity)
{
	if (Count <= 0 || !CurrentLODLevel)
	{
		return;
	}

	if (ActiveParticles + Count > MaxActiveParticles)
	{
		const int32 NewMax = std::max(MaxActiveParticles * 2, ActiveParticles + Count);
		Resize(NewMax, true);
	}

	const TArray<UParticleModule*>& SpawnModules = CurrentLODLevel->GetSpawnModules();

	for (int32 i = 0; i < Count; ++i)
	{
		if (ActiveParticles >= MaxActiveParticles)
		{
			break;
		}

		const uint16 ParticleIndex = ParticleIndices[ActiveParticles];
		FBaseParticle* Particle = reinterpret_cast<FBaseParticle*>(ParticleData + static_cast<size_t>(ParticleIndex) * ParticleStride);
		*Particle = FBaseParticle();

		Particle->Location = InitialLocation;
		Particle->OldLocation = InitialLocation;
		Particle->BaseVelocity = InitialVelocity;
		Particle->Velocity = InitialVelocity;
		Particle->OneOverMaxLifetime = 1.0f; // 기본 수명 1초 (모듈에서 덮어쓸 수 있음)

		const float SpawnTime = StartTime + static_cast<float>(i) * Increment;
		for (UParticleModule* Module : SpawnModules)
		{
			if (Module && Module->bEnabled)
			{
				Module->Spawn(this, 0, SpawnTime, Particle);
			}
		}

		++ActiveParticles;
	}
}

void FParticleEmitterInstance::KillParticles()
{
	for (int32 i = ActiveParticles - 1; i >= 0; --i)
	{
		const uint16 Index = ParticleIndices[i];
		const FBaseParticle* Particle = reinterpret_cast<const FBaseParticle*>(ParticleData + static_cast<size_t>(Index) * ParticleStride);
		if (Particle->RelativeTime >= 1.0f)
		{
			// Swap-and-pop으로 O(1) 제거
			std::swap(ParticleIndices[i], ParticleIndices[ActiveParticles - 1]);
			--ActiveParticles;
		}
	}
}

void FParticleEmitterInstance::KillParticlesForced(bool bFireEvents)
{
	ActiveParticles = 0;
}

void FParticleEmitterInstance::UpdateBoundingBox(float DeltaTime)
{
	if (ActiveParticles <= 0)
	{
		ParticleBoundingBox = { FVector::ZeroVector, FVector::ZeroVector };
		return;
	}

	FVector MinPt(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector MaxPt(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (int32 i = 0; i < ActiveParticles; ++i)
	{
		const FBaseParticle* Particle = GetParticleAtIndex(i);
		if (!Particle)
		{
			continue;
		}
		MinPt.X = std::min(MinPt.X, Particle->Location.X);
		MinPt.Y = std::min(MinPt.Y, Particle->Location.Y);
		MinPt.Z = std::min(MinPt.Z, Particle->Location.Z);
		MaxPt.X = std::max(MaxPt.X, Particle->Location.X);
		MaxPt.Y = std::max(MaxPt.Y, Particle->Location.Y);
		MaxPt.Z = std::max(MaxPt.Z, Particle->Location.Z);
	}

	ParticleBoundingBox = { MinPt, MaxPt };
}

FBaseParticle* FParticleEmitterInstance::GetParticleAtIndex(int32 ActiveIndex) const
{
	if (ActiveIndex < 0 || ActiveIndex >= ActiveParticles || !ParticleData || !ParticleIndices)
	{
		return nullptr;
	}
	const uint16 SlotIndex = ParticleIndices[ActiveIndex];
	return reinterpret_cast<FBaseParticle*>(ParticleData + static_cast<size_t>(SlotIndex) * ParticleStride);
}
