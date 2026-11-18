#include "EnginePCH.h"
#include "ParticleModuleCollision.h"
#include "BaseParticle.h"
#include "ParticleEmitterInstance.h"
#include "Components/ParticleSystemComponent.h"
#include "Engine/World.h"
#include "Collision/Ray.h"
#include "Collision/HitResult.h"
#include "Particles/ParticleCollisionDebug.h"
#include "Components/PrimitiveComponent.h"
#include "ParticleModuleTypeDataRibbon.h"
#include "ParticleRibbonEmitterInstance.h"
#include "ParticleLODLevel.h"
#include "ParticleStats.h"

namespace
{
	constexpr float CollisionSkin = 0.01f;

	FParticleCollisionPayload& GetPayload(FBaseParticle* Particle, int32 Offset)
	{
		return *reinterpret_cast<FParticleCollisionPayload*>(reinterpret_cast<uint8*>(Particle) + Offset);
	}

	// 리본이면 리본 인스턴스를, 아니면 nullptr 을 return
	FParticleRibbonEmitterInstance* AsRibbon(FParticleEmitterInstance* Owner)
	{
		UParticleLODLevel* LOD = Owner->GetCurrentLODLevel();
		if (LOD && LOD->TypeDataModule && LOD->TypeDataModule->IsA<UParticleModuleTypeDataRibbon>())
		{
			return static_cast<FParticleRibbonEmitterInstance*>(Owner);
		}
		return nullptr;
	}

	void KillRibbonFrom(FParticleRibbonEmitterInstance* Ribbon, int32 Slot)
	{
		for (int32 S = Slot; S != INDEX_NONE; S = Ribbon->GetRibbonPayload(S)->Next)
		{
			Ribbon->GetParticleAtSlot(S)->RelativeTime = 1.1f;
		}
	}
}

UParticleModuleCollision::UParticleModuleCollision()
{
	bSpawnModule = true;
	bFinalUpdateModule = true;
}

void UParticleModuleCollision::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	GetPayload(ParticleBase, Offset) = FParticleCollisionPayload();
}

void UParticleModuleCollision::FinalUpdate(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(ParticleStats::STAT_ParticleCollision);
	if (!Owner || !Owner->Component)
	{
		return;
	}

	UWorld* World = Owner->Component->GetWorld();
	if (!World)
	{
		return;
	}

	// 로컬 스페이스 이미터는 파티클 위치, 속도가 컴포넌트 공간이므로 월드로 변환 후 검사
	const bool bLocal = Owner->IsLocalSpace();
	const FMatrix SimToWorld = bLocal ? Owner->Component->GetWorldMatrix() : FMatrix();
	const FMatrix WorldToSim = bLocal ? SimToWorld.Inverse() : FMatrix();

	AActor* IgnoreActor = Owner->Component->GetOwner();

	// 이미터 인스턴스의 모든 파티클이 지나간 공간 전체 AABB 를 구함
	FVector BoxMin(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector BoxMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	float MaxRadius = 0.0f;

	for (int32 i = 0; i < Owner->ActiveParticles; ++i)
	{
		FBaseParticle* P = Owner->GetParticleAtIndex(i);
		if (!P)
		{
			continue;
		}

		FVector S = P->OldLocation;
		FVector E = P->Location;

		if (bLocal)
		{
			S = SimToWorld.TransformPosition(S);
			E = SimToWorld.TransformPosition(E);
		}

		for (int32 a = 0; a < 3; ++a)
		{
			BoxMin[a] = FMath::Min(BoxMin[a], FMath::Min(S[a], E[a]));
			BoxMax[a] = FMath::Max(BoxMax[a], FMath::Max(S[a], E[a]));
		}
		MaxRadius = FMath::Max(MaxRadius, Owner->GetParticleExtent(*P) * RadiusScale);
	}

	if (BoxMin.X > BoxMax.X)
	{
		return;
	}

	const FVector Pad(MaxRadius, MaxRadius, MaxRadius);
	const FBox SweptBox = { BoxMin - Pad, BoxMax + Pad };

	// AABB와 겹치는 블로킹 컴포넌트만 골라서 목록화
	Candidates.Reset();
	World->GatherBlockingComponents(SweptBox, Candidates, IgnoreActor);

	// 후보 바운드는 파티클 루프 밖에서 한 번만 계산한다
	CandidateBounds.Reset();
	for (UPrimitiveComponent* Candidate : Candidates)
	{
		CandidateBounds.Add(Candidate->CalcBounds());
	}

	ParticleCollisionDebug::RecordFrame(World, SweptBox, CandidateBounds);

	if (CandidateBounds.IsEmpty())
	{
		// 근처에 겹치는 컴포넌트가 없으면 충돌처리 X
		return;
	}

	// 실제 충돌 파티클 루프
	for (int32 i = 0; i < Owner->ActiveParticles; ++i)
	{
		FBaseParticle* Particle = Owner->GetParticleAtIndex(i);
		if (!Particle || Particle->RelativeTime > 1.0f)
		{
			continue;
		}

		FParticleCollisionPayload& Payload = GetPayload(Particle, Offset);

		// 이번 프레임 이동 구간 (월드 공간 기준)
		FVector Start = Particle->OldLocation;
		FVector End = Particle->Location;
		FVector Velocity = Particle->Velocity;

		if (bLocal)
		{
			Start = SimToWorld.TransformPosition(Start);
			End = SimToWorld.TransformPosition(End);
			Velocity = SimToWorld.TransformVector(Velocity);
		}

		const FVector Delta = End - Start;
		const float Distance = Delta.Size();
		if (Distance <= SMALL_NUMBER)
		{
			continue;
		}

		// 구 반경 Sweep
		FRay Ray;
		Ray.Origin = Start;
		Ray.Direction = Delta / Distance;

		const float Radius = Owner->GetParticleExtent(*Particle) * RadiusScale;

		float BestT = Distance;
		FVector Normal;
		bool bHit = false;
		for (const FBox& Bounds : CandidateBounds)
		{
			float T;
			FVector N;
			if (SweepSphereVsBox(Ray, Bounds, Radius, BestT, T, N))
			{
				BestT = T;
				Normal = N;
				bHit = true;
			}
		}

		if (!bHit)
		{
			continue;
		}
		if (Normal.Dot(Ray.Direction) >= 0.0f)
		{
			continue;
		}
		const FVector HitLocation = Ray.PointAt(BestT);
		ParticleCollisionDebug::RecordHit(World, HitLocation, Normal);

		// 이번 충돌에 적용할 Response 결정
		++Payload.UsedCollisions;
		EParticleCollisionResponse ThisResponse = Response;
		if (MaxCollisions > 0 && Payload.UsedCollisions >= MaxCollisions)
		{
			ThisResponse = OnMaxCollisions;
		}

		if (ThisResponse == EParticleCollisionResponse::Kill)
		{
			Particle->Flags |= STATE_Particle_KilledByCollision;
			Particle->RelativeTime = 1.1f;
			continue;
		}

		// 속도 반응
		FVector NewVelocity = FVector::ZeroVector;
		if (ThisResponse == EParticleCollisionResponse::Bounce)
		{
			const FVector Vn = Normal * Velocity.Dot(Normal);
			const FVector Vt = Velocity - Vn;
			NewVelocity = Vt * (1.0f - Friction) - Vn * Restitution;
			if (NewVelocity.Size() < MinBounceSpeed)
			{
				NewVelocity = FVector::ZeroVector;
			}
		}

		// 위치 결정 
		FVector NewLocation = HitLocation + Normal * CollisionSkin;
		if (bLocal)
		{
			NewLocation = WorldToSim.TransformPosition(NewLocation);
			NewVelocity = WorldToSim.TransformVector(NewVelocity);
		}
		
		Particle->Location = NewLocation;
		Particle->Velocity = NewVelocity;
		Particle->BaseVelocity = NewVelocity;
	}

}

uint32 UParticleModuleCollision::RequiredBytes(UParticleModuleTypeDataBase* TypeData) const
{
	return sizeof(FParticleCollisionPayload);
}
