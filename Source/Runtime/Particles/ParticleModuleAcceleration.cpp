#include "EnginePCH.h"
#include "Particles/ParticleModuleAcceleration.h"
#include "Particles/BaseParticle.h"
#include "Particles/ParticleEmitterInstance.h"

namespace
{
	FParticleAccelerationPayload& GetPayload(FBaseParticle* Particle, int32 Offset)
	{
		return *reinterpret_cast<FParticleAccelerationPayload*>(reinterpret_cast<uint8*>(Particle) + Offset);
	}
}

UParticleModuleAcceleration::UParticleModuleAcceleration()
{
	bSpawnModule = true;
	bUpdateModule = true;
}

uint32 UParticleModuleAcceleration::RequiredBytes(UParticleModuleTypeDataBase* TypeData) const
{
	return sizeof(FParticleAccelerationPayload);
}

void UParticleModuleAcceleration::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	// Unreal UParticleModuleAcceleration::SpawnEx
	FVector Accel = RandomRange(Owner, AccelerationMin, AccelerationMax);
	Accel = bInWorldSpace ? WorldToSimulationVector(Owner, Accel) : EmitterToSimulationVector(Owner, Accel);

	GetPayload(ParticleBase, Offset).Acceleration = Accel;

	// 스폰 프레임 안에서 이미 지난 시간만큼 반영 (PostSpawn이 Velocity * SpawnTime 만큼 진행시킨다)
	ParticleBase->Velocity += Accel * SpawnTime;
	ParticleBase->BaseVelocity += Accel * SpawnTime;
}

void UParticleModuleAcceleration::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	for (int32 i = 0; i < Owner->ActiveParticles; ++i)
	{
		FBaseParticle* Particle = Owner->GetParticleAtIndex(i);
		if (!Particle)
		{
			continue;
		}

		const FVector& Accel = GetPayload(Particle, Offset).Acceleration;
		// ResetParticleParameters가 Velocity = BaseVelocity로 되돌리므로 Base에도 누적해야 속도가 계속 늘어난다
		Particle->Velocity += Accel * DeltaTime;
		Particle->BaseVelocity += Accel * DeltaTime;
	}
}
