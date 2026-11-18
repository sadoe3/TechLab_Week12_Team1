#include "EnginePCH.h"
#include "ParticleModuleCollision.h"
#include "BaseParticle.h"
#include "ParticleEmitterInstance.h"
#include "Components/ParticleSystemComponent.h"
#include "Engine/World.h"
#include "Collision/Ray.h"
#include "Collision/HitResult.h"

namespace
{
	constexpr float CollisionSkin = 0.01f;

	FParticleCollisionPayload& GetPayload(FBaseParticle* Particle, int32 Offset)
	{
		return *reinterpret_cast<FParticleCollisionPayload*>(reinterpret_cast<uint8*>(Particle) + Offset);
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
	const FMatrix WorldToSim = bEnabled ? SimToWorld.Inverse() : FMatrix();

	AActor* IgnoreActor = Owner->Component->GetOwner();

	for (int32 i = 0; i < Owner->ActiveParticles; ++i)
	{
		FBaseParticle* Particle = Owner->GetParticleAtIndex(i);
		if (!Particle || Particle->RelativeTime > 1.0f)
		{
			continue;
		}

		FParticleCollisionPayload& Payload = GetPayload(Particle, Offset);

		// 이번 프레임 이동 구간 (월드 공간)
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

		// 구 sweep
		FRay Ray;
		Ray.Origin = Start;
		Ray.Direction = Delta / Distance;

		const float Radius = Owner->GetParticleExtent(*Particle) * RadiusScale;

		FHitResult Hit;
		if (!World->SweepSingle(Ray, Distance, Radius, Hit, IgnoreActor, true))
		{
			continue;
		}

		const FVector Normal = Hit.ImpactNormal;
		if (Normal.Dot(Ray.Direction) >= 0.0f)
		{
			continue;
		}

		// 이번 충돌에 적용할 반응 결정
		++Payload.UsedCollisions;
		EParticleCollisionResponse ThisResponse = Response;
		if (MaxCollisions > 0 && Payload.UsedCollisions >= MaxCollisions)
		{
			ThisResponse = OnMaxCollisions;
		}

		if (ThisResponse == EParticleCollisionResponse::Kill)
		{
			Particle->RelativeTime = 1.1f;	// RelativeTime을 1.0보다 크게 조정하면서 Kill 대상으로 지정
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

		// 위치 결정 => 접촉 지점에서 살짝 띄우기
		FVector NewLocation = Hit.Location + Normal * CollisionSkin;

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
