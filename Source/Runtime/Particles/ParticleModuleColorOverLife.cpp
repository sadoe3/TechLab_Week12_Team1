#include "EnginePCH.h"
#include "Particles/ParticleModuleColorOverLife.h"
#include "Particles/BaseParticle.h"
#include "Particles/ParticleEmitterInstance.h"
#include <algorithm>

UParticleModuleColorOverLife::UParticleModuleColorOverLife()
{
	bSpawnModule = true;
	bUpdateModule = true;
}

void UParticleModuleColorOverLife::Apply(FBaseParticle& Particle) const
{
	const float T = std::clamp(Particle.RelativeTime, 0.0f, 1.0f);
	const FVector Rgb = ColorOverLife.Eval(T, FVector::OneVector);
	const float Alpha = AlphaOverLife.Eval(T, 1.0f);
	Particle.Color = FVector4(Rgb.X, Rgb.Y, Rgb.Z, Alpha);
}

void UParticleModuleColorOverLife::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	Apply(*ParticleBase);
}

void UParticleModuleColorOverLife::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	// Unreal BEGIN_UPDATE_LOOP / END_UPDATE_LOOP 대응: 활성 파티클 전체 순회
	for (int32 i = 0; i < Owner->ActiveParticles; ++i)
	{
		if (FBaseParticle* Particle = Owner->GetParticleAtIndex(i))
		{
			Apply(*Particle);
		}
	}
}
