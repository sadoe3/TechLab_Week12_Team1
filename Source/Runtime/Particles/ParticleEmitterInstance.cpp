#include "EnginePCH.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleDynamicData.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModule.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleSpawn.h"
#include <cfloat>
#include <cmath>
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
	PayloadOffset = sizeof(FBaseParticle);

	if (SpriteTemplate)
	{
		SpriteTemplate->CacheEmitterModuleInfo();
		CurrentLODLevel = SpriteTemplate->GetLODLevel(CurrentLODLevelIndex);
		ParticleSize = SpriteTemplate->ParticleSize;
	}
	// 슬롯마다 시작 주소가 16바이트 경계에 오도록 크기를 올림한다
	ParticleStride = (ParticleSize + 15) & ~15;

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
	// ParticleIndices가 uint16이므로 슬롯 번호가 65535를 넘으면 안 된다.
	NewMaxActiveParticles = std::min(NewMaxActiveParticles, static_cast<int32>(MAX_PARTICLES_PER_EMITTER));
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

	// Unreal FParticleEmitterInstance::Tick과 같은 순서:
	// EmitterTime → Kill → ResetParticleParameters → 모듈 Update → Spawn → (PostUpdate) → UpdateBoundingBox(적분) → FinalUpdate
	Tick_EmitterTimeSetup(DeltaTime);

	// bKillOnCompleted: 마지막 루프가 끝나면 남은 파티클이 수명을 다 채우길 기다리지 않고 즉시 제거한다
	const UParticleModuleRequired* Required = CurrentLODLevel->RequiredModule;
	if (bEmitterIsDone && Required && Required->bKillOnCompleted && ActiveParticles > 0)
	{
		KillParticlesForced();
	}

	// 1. 지난 프레임에 수명(RelativeTime > 1)을 넘긴 파티클 제거
	KillParticles();

	// 2. Velocity/Size/RotationRate를 Base 값으로 되돌리고 수명을 진행한다.
	//    (모듈 Update는 매 프레임 Base 값 위에 다시 더하므로 누적되지 않는다)
	ResetParticleParameters(DeltaTime);

	// 3. Update 모듈
	Tick_ModuleUpdate(DeltaTime);

	// 4. 새 파티클 스폰 (EmitterDelay 동안, 루프 종료 후, 억제 중에는 스폰하지 않는다)
	if (!bSuppressSpawning && !bEmitterIsDone && EmitterTime >= CurrentDelay)
	{
		SpawnFraction = Spawn(DeltaTime);
	}

	// 5. 위치/회전 적분 + 바운딩 박스 갱신 (Unreal은 UpdateBoundingBox 안에서 적분한다)
	UpdateBoundingBox(DeltaTime);

	// 6. FinalUpdate 모듈
	Tick_ModuleFinalUpdate(DeltaTime);
}

void FParticleEmitterInstance::Tick_EmitterTimeSetup(float DeltaTime)
{
	const UParticleModuleRequired* Required = CurrentLODLevel->RequiredModule;
	const float Duration = (Required && Required->EmitterDuration > 0.0f) ? Required->EmitterDuration : 1.0f;
	const int32 MaxLoops = Required ? Required->EmitterLoops : 0;
	CurrentDelay = Required ? std::max(Required->EmitterDelay, 0.0f) : 0.0f;

	// EmitterTime은 [0, Delay + Duration) 범위를 돈다. Delay는 매 루프 적용(Unreal bDelayFirstLoopOnly=false 기본값).
	EmitterTime += DeltaTime;
	const float LoopLength = CurrentDelay + Duration;
	if (EmitterTime >= LoopLength)
	{
		if (MaxLoops == 0 || LoopCount + 1 < MaxLoops)
		{
			EmitterTime -= LoopLength;
			++LoopCount;
			// 새 루프에서는 버스트가 다시 터진다
			for (int32 i = 0; i < BurstFired.Num(); ++i)
			{
				BurstFired[i] = 0;
			}
		}
		else
		{
			bEmitterIsDone = true;
		}
	}
}

void FParticleEmitterInstance::Rewind()
{
	// Unreal FParticleEmitterInstance::Rewind: 이미터 시간을 처음으로 되돌려 다시 스폰할 수 있게 한다 (살아있는 파티클은 유지)
	EmitterTime = 0.0f;
	LoopCount = 0;
	SpawnFraction = 0.0f;
	bEmitterIsDone = false;
	for (int32 i = 0; i < BurstFired.Num(); ++i)
	{
		BurstFired[i] = 0;
	}
}

bool FParticleEmitterInstance::ShouldKillOnDeactivate() const
{
	return CurrentLODLevel && CurrentLODLevel->RequiredModule && CurrentLODLevel->RequiredModule->bKillOnDeactivate;
}

bool FParticleEmitterInstance::HasCompleted() const
{
	// 모든 루프를 마쳤고 남은 파티클도 없으면 완료 (Unreal HasCompleted). 무한 루프(EmitterLoops == 0)는 완료되지 않는다.
	return bEmitterIsDone && ActiveParticles == 0;
}

void FParticleEmitterInstance::ResetParticleParameters(float DeltaTime)
{
	for (int32 i = 0; i < ActiveParticles; ++i)
	{
		FBaseParticle* Particle = GetParticleAtIndex(i);
		Particle->OldLocation = Particle->Location;
		Particle->Velocity = Particle->BaseVelocity;
		Particle->Size = Particle->BaseSize;
		Particle->RotationRate = Particle->BaseRotationRate;
		Particle->RelativeTime += Particle->OneOverMaxLifetime * DeltaTime;
	}
}

void FParticleEmitterInstance::Tick_ModuleUpdate(float DeltaTime)
{
	for (UParticleModule* Module : CurrentLODLevel->GetUpdateModules())
	{
		if (Module && Module->bEnabled && Module->bUpdateModule)
		{
			Module->Update(this, SpriteTemplate->GetModuleOffset(Module), DeltaTime);
		}
	}
}

void FParticleEmitterInstance::Tick_ModuleFinalUpdate(float DeltaTime)
{
	for (UParticleModule* Module : CurrentLODLevel->GetUpdateModules())
	{
		if (Module && Module->bEnabled && Module->bFinalUpdateModule)
		{
			Module->FinalUpdate(this, SpriteTemplate->GetModuleOffset(Module), DeltaTime);
		}
	}
}

float FParticleEmitterInstance::Spawn(float DeltaTime)
{
	if (!CurrentLODLevel || !CurrentLODLevel->SpawnModule)
	{
		return SpawnFraction;
	}

	const UParticleModuleSpawn* SpawnModule = CurrentLODLevel->SpawnModule;
	// 스폰 위치는 로컬 공간이면 원점(컴포넌트 기준), 월드 공간이면 컴포넌트 월드 위치. 초기 속도는 0(Velocity 모듈이 더한다).
	const FVector InitialLocation = (!IsLocalSpace() && Component) ? Component->GetWorldLocation() : FVector::ZeroVector;

	// ── 1. Rate 스폰 ──
	// Unreal FParticleEmitterInstance::Spawn과 같은 계산.
	// 이번 프레임에 나올 파티클들의 "생성 후 경과 시간"(SpawnTime)을 오래된 것부터 Increment 간격으로 배정한다.
	float NewLeftover = SpawnFraction;
	const float SpawnRate = SpawnModule->GetSpawnRate() * SpawnModule->RateScale;
	if (SpawnRate > 0.0f)
	{
		const float OldLeftover = SpawnFraction;
		NewLeftover = OldLeftover + DeltaTime * SpawnRate;
		const int32 Number = static_cast<int32>(std::floor(NewLeftover));
		const float Increment = 1.0f / SpawnRate;
		const float StartTime = DeltaTime + OldLeftover * Increment - Increment;
		NewLeftover -= static_cast<float>(Number);

		if (Number > 0)
		{
			SpawnParticles(Number, StartTime, Increment, InitialLocation, FVector::ZeroVector);
		}
	}

	// ── 2. Burst 스폰 ──
	if (SpawnModule->bProcessBurstList)
	{
		float BurstSpawnTime = 0.0f;
		const int32 BurstCount = GetCurrentBurstCount(DeltaTime, BurstSpawnTime);
		if (BurstCount > 0)
		{
			// 한 버스트의 파티클은 같은 순간에 태어나므로 Increment = 0 (모두 같은 SpawnTime)
			SpawnParticles(BurstCount, BurstSpawnTime, 0.0f, InitialLocation, FVector::ZeroVector);
		}
	}

	return NewLeftover;
}

int32 FParticleEmitterInstance::GetCurrentBurstCount(float DeltaTime, float& OutSpawnTime)
{
	// Unreal FParticleEmitterInstance::GetCurrentBurstRateOffset 대응
	OutSpawnTime = 0.0f;
	const UParticleModuleSpawn* SpawnModule = CurrentLODLevel->SpawnModule;
	const TArray<FParticleBurst>& BurstList = SpawnModule->GetBurstList();
	if (BurstList.IsEmpty())
	{
		return 0;
	}

	// 버스트마다 "이번 루프에서 이미 터졌는가"를 기억한다. 루프가 넘어가면 Tick_EmitterTimeSetup이 지운다.
	if (BurstFired.Num() != BurstList.Num())
	{
		BurstFired.SetNum(BurstList.Num());
		for (int32 i = 0; i < BurstFired.Num(); ++i)
		{
			BurstFired[i] = 0;
		}
	}

	const UParticleModuleRequired* Required = CurrentLODLevel->RequiredModule;
	const float Duration = (Required && Required->EmitterDuration > 0.0f) ? Required->EmitterDuration : 1.0f;
	// 딜레이를 뺀 "이번 루프에서 이미터가 실제로 돈 시간"
	const float LocalEmitterTime = EmitterTime - CurrentDelay;

	int32 TotalCount = 0;
	for (int32 i = 0; i < BurstList.Num(); ++i)
	{
		const FParticleBurst& Burst = BurstList[i];
		// Time은 이미터 Duration에 대한 비율(0~1). Unreal FParticleBurst::Time과 같은 의미.
		const float BurstTime = std::clamp(Burst.Time, 0.0f, 1.0f) * Duration;
		if (BurstFired[i] || LocalEmitterTime < BurstTime)
		{
			continue;
		}

		BurstFired[i] = 1;

		// CountLow >= 0이면 [CountLow, Count] 사이 랜덤, 아니면 Count 고정
		int32 Count = Burst.Count;
		if (Burst.CountLow > -1 && Burst.CountLow < Burst.Count && Component)
		{
			const int32 Range = Burst.Count - Burst.CountLow + 1;
			Count = Burst.CountLow + std::min(static_cast<int32>(Component->GetRandomFraction() * Range), Range - 1);
		}
		TotalCount += std::max(Count, 0);

		// 이번 프레임 안에서 버스트 시각이 지난 만큼 미리 진행시킨다 (여러 개면 가장 이른 것 기준)
		OutSpawnTime = std::max(OutSpawnTime, std::clamp(LocalEmitterTime - BurstTime, 0.0f, DeltaTime));
	}
	return TotalCount;
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

		// PreSpawn: 재사용 슬롯이라 Payload에 이전 파티클 값이 남아 있으므로 슬롯 전체를 먼저 지운다
		std::memset(Particle, 0, ParticleStride);
		*Particle = FBaseParticle();
		Particle->Location = InitialLocation;
		Particle->OldLocation = InitialLocation;
		Particle->BaseVelocity = InitialVelocity;
		Particle->Velocity = InitialVelocity;
		// Unreal은 Lifetime 모듈이 없으면 OneOverMaxLifetime = 0(영원히 생존)이다.
		// 이 엔진은 인덱스가 uint16이라 무한 증가를 막기 위해 Lifetime 모듈이 없을 때 1초로 둔다.
		Particle->OneOverMaxLifetime = 1.0f;

		// Spawn 모듈: SpawnTime = 이 파티클이 이번 프레임 안에서 이미 살아 있었던 시간
		const float SpawnTime = StartTime - static_cast<float>(i) * Increment;
		for (UParticleModule* Module : SpawnModules)
		{
			if (Module && Module->bEnabled)
			{
				Module->Spawn(this, SpriteTemplate->GetModuleOffset(Module), SpawnTime, Particle);
			}
		}

		// PostSpawn: 프레임 중간에 태어난 만큼 앞으로 진행시켜 같은 프레임 파티클이 한 점에 뭉치지 않게 한다.
		// (UpdateBoundingBox가 이번 프레임 DeltaTime 적분을 따로 하므로 여기서는 SpawnTime만큼만)
		Particle->OldLocation = Particle->Location;
		Particle->Location += Particle->Velocity * SpawnTime;
		Particle->Flags |= STATE_Particle_JustSpawned;

		++ActiveParticles;
	}
}

void FParticleEmitterInstance::KillParticles()
{
	for (int32 i = ActiveParticles - 1; i >= 0; --i)
	{
		const uint16 Index = ParticleIndices[i];
		const FBaseParticle* Particle = reinterpret_cast<const FBaseParticle*>(ParticleData + static_cast<size_t>(Index) * ParticleStride);
		if (Particle->RelativeTime > 1.0f)
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
		FBaseParticle* Particle = GetParticleAtIndex(i);
		if (!Particle)
		{
			continue;
		}

		// 적분 (Unreal: UpdateBoundingBox 안에서 Location += DeltaTime * Velocity)
		// 이번 프레임에 태어난 파티클은 PostSpawn에서 SpawnTime만큼 이미 진행했으므로 건너뛴다.
		if (Particle->Flags & STATE_Particle_JustSpawned)
		{
			Particle->Flags &= ~STATE_Particle_JustSpawned;
		}
		else
		{
			Particle->Location += Particle->Velocity * DeltaTime;
			Particle->Rotation += Particle->RotationRate * DeltaTime;
		}

		// 위치만 쓰면 크기가 있는 스프라이트/메시가 박스 밖으로 삐져나와 경계에서 컬링된다.
		const float Extent = GetParticleExtent(*Particle);
		MinPt.X = std::min(MinPt.X, Particle->Location.X - Extent);
		MinPt.Y = std::min(MinPt.Y, Particle->Location.Y - Extent);
		MinPt.Z = std::min(MinPt.Z, Particle->Location.Z - Extent);
		MaxPt.X = std::max(MaxPt.X, Particle->Location.X + Extent);
		MaxPt.Y = std::max(MaxPt.Y, Particle->Location.Y + Extent);
		MaxPt.Z = std::max(MaxPt.Z, Particle->Location.Z + Extent);
	}

	ParticleBoundingBox = { MinPt, MaxPt };
}

bool FParticleEmitterInstance::IsLocalSpace() const
{
	return CurrentLODLevel && CurrentLODLevel->RequiredModule && CurrentLODLevel->RequiredModule->bUseLocalSpace;
}

float FParticleEmitterInstance::GetParticleExtent(const FBaseParticle& Particle) const
{
	// 기본(스프라이트): 1x1 쿼드를 Size로 스케일하므로 카메라 방향과 무관하게 감싸려면 대각선 절반이면 충분하다.
	const float MaxSize = std::max({ std::abs(Particle.Size.X), std::abs(Particle.Size.Y), std::abs(Particle.Size.Z) });
	return MaxSize * 0.70710678f;
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

TUniquePtr<FDynamicEmitterDataBase> FParticleEmitterInstance::GetDynamicData(bool bSelected)
{
	return nullptr;
}
