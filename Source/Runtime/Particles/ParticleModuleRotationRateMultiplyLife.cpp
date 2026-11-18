#include "EnginePCH.h"
#include "Particles/ParticleModuleRotationRateMultiplyLife.h"
#include "Particles/BaseParticle.h"
#include "Particles/ParticleEmitterInstance.h"
#include <algorithm>

UParticleModuleRotationRateMultiplyLife::UParticleModuleRotationRateMultiplyLife()
{
	bSpawnModule = true;
	bUpdateModule = true;
}

void UParticleModuleRotationRateMultiplyLife::Apply(FBaseParticle& Particle) const
{
	const float T = std::clamp(Particle.RelativeTime, 0.0f, 1.0f);
	const FVector Scale = MultiplierStart + (MultiplierEnd - MultiplierStart) * T;

	// RotationRate만 곱하고 BaseRotationRate는 건드리지 않는다. Base까지 바꾸면 매 프레임 배율이 누적된다.
	Particle.RotationRate.X *= Scale.X;
	Particle.RotationRate.Y *= Scale.Y;
	Particle.RotationRate.Z *= Scale.Z;
}

void UParticleModuleRotationRateMultiplyLife::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	Apply(*ParticleBase);
}

void UParticleModuleRotationRateMultiplyLife::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	// ResetParticleParameters가 이미 RotationRate = BaseRotationRate로 되돌려 놓았으므로 곱하기만 하면 된다.
	for (int32 i = 0; i < Owner->ActiveParticles; ++i)
	{
		if (FBaseParticle* Particle = Owner->GetParticleAtIndex(i))
		{
			Apply(*Particle);
		}
	}
}
