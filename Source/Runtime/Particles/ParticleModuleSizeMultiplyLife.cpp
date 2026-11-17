#include "EnginePCH.h"
#include "Particles/ParticleModuleSizeMultiplyLife.h"
#include "Particles/BaseParticle.h"
#include "Particles/ParticleEmitterInstance.h"
#include <algorithm>

UParticleModuleSizeMultiplyLife::UParticleModuleSizeMultiplyLife()
{
	bSpawnModule = true;
	bUpdateModule = true;
}

void UParticleModuleSizeMultiplyLife::Apply(FBaseParticle& Particle) const
{
	const float T = std::clamp(Particle.RelativeTime, 0.0f, 1.0f);
	const FVector Scale = MultiplierStart + (MultiplierEnd - MultiplierStart) * T;

	// Size만 곱하고 BaseSize는 건드리지 않는다. BaseSize까지 바꾸면 매 프레임 배율이 누적된다.
	if (bMultiplyX) { Particle.Size.X *= Scale.X; }
	if (bMultiplyY) { Particle.Size.Y *= Scale.Y; }
	if (bMultiplyZ) { Particle.Size.Z *= Scale.Z; }
}

void UParticleModuleSizeMultiplyLife::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	// 스폰된 프레임에도 배율이 적용된 크기로 그려지게 한다.
	// RelativeTime은 앞선 Lifetime 모듈이 SpawnTime 기준으로 정해 둔 값이다.
	Apply(*ParticleBase);
}

void UParticleModuleSizeMultiplyLife::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	// ResetParticleParameters가 이미 Size = BaseSize로 되돌려 놓았으므로 곱하기만 하면 된다.
	for (int32 i = 0; i < Owner->ActiveParticles; ++i)
	{
		if (FBaseParticle* Particle = Owner->GetParticleAtIndex(i))
		{
			Apply(*Particle);
		}
	}
}
