#include "EnginePCH.h"
#include "Particles/ParticleModuleSubUV.h"
#include "Particles/BaseParticle.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace
{
	FSubUVPayload& GetPayload(FBaseParticle& Particle, int32 Offset)
	{
		return *reinterpret_cast<FSubUVPayload*>(reinterpret_cast<uint8*>(&Particle) + Offset);
	}

	/** Offset이 FBaseParticle 뒤의 Payload 영역을 가리키는가. 오프셋 맵에 없는 모듈은 0이 와서 FBaseParticle을 덮어쓰게 된다. */
	bool IsValidPayloadOffset(int32 Offset)
	{
		return Offset >= static_cast<int32>(sizeof(FBaseParticle));
	}

	int32 GetTotalSubImages(const FParticleEmitterInstance* Owner)
	{
		const UParticleLODLevel* LOD = Owner ? Owner->GetCurrentLODLevel() : nullptr;
		const UParticleModuleRequired* Required = LOD ? LOD->RequiredModule : nullptr;
		if (!Required)
		{
			return 1;
		}
		return std::max(Required->SubImages_Horizontal, 1) * std::max(Required->SubImages_Vertical, 1);
	}
}

UParticleModuleSubUV::UParticleModuleSubUV()
{
	bSpawnModule = true;
	bUpdateModule = true;
}

float UParticleModuleSubUV::GetImageIndex(const FBaseParticle& Particle, int32 Offset)
{
	if (!IsValidPayloadOffset(Offset))
	{
		return 0.0f;
	}
	return reinterpret_cast<const FSubUVPayload*>(reinterpret_cast<const uint8*>(&Particle) + Offset)->ImageIndex;
}

void UParticleModuleSubUV::Apply(const FParticleEmitterInstance* Owner, FBaseParticle& Particle, FSubUVPayload& Payload) const
{
	// 재생 구간 [First, Last]를 아틀라스 범위 안으로 맞춘다. Last < First면 거꾸로 재생.
	const int32 Total = GetTotalSubImages(Owner);
	const int32 First = std::clamp(StartFrame, 0, Total - 1);
	const int32 Last = (EndFrame < 0) ? Total - 1 : std::clamp(EndFrame, 0, Total - 1);
	const int32 Count = std::abs(Last - First) + 1;
	const int32 Direction = (Last >= First) ? 1 : -1;

	const float T = std::clamp(Particle.RelativeTime, 0.0f, 1.0f);
	int32 Step = 0;
	switch (InterpolationMethod)
	{
	case EParticleSubUVInterpMethod::Linear:
	{
		// 구간을 Count칸으로 고르게 나눠 각 칸이 같은 시간만큼 보이게 한다 (끝 칸이 t=1 순간에만 보이지 않도록 Count를 곱한다)
		const float Frames = T * std::max(Cycles, 0.0f) * static_cast<float>(Count);
		Step = static_cast<int32>(std::floor(Frames));
		if (T >= 1.0f && Step > 0)
		{
			// 수명 끝 순간에는 다음 반복의 첫 칸이 아니라 이번 반복의 마지막 칸을 보인다
			Step -= 1;
		}
		Step += static_cast<int32>(Payload.RandomOffset);
		break;
	}
	case EParticleSubUVInterpMethod::Random:
		Step = static_cast<int32>(Payload.RandomOffset);
		break;
	case EParticleSubUVInterpMethod::FrameRate:
	{
		// 나이(초) = RelativeTime × 최대 수명
		const float Age = (Particle.OneOverMaxLifetime > 0.0f) ? Particle.RelativeTime / Particle.OneOverMaxLifetime : 0.0f;
		Step = static_cast<int32>(std::floor(std::max(Age, 0.0f) * std::max(FramesPerSecond, 0.0f)));
		Step += static_cast<int32>(Payload.RandomOffset);
		break;
	}
	}

	Step %= Count;
	Payload.ImageIndex = static_cast<float>(First + Direction * Step);
}

void UParticleModuleSubUV::Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase)
{
	if (!ParticleBase || !IsValidPayloadOffset(Offset))
	{
		return;
	}

	FSubUVPayload& Payload = GetPayload(*ParticleBase, Offset);

	// Random 방식의 칸, 또는 bRandomStartFrame의 시작 칸을 스폰 때 한 번만 뽑는다
	const bool bNeedsRandom = (InterpolationMethod == EParticleSubUVInterpMethod::Random) || bRandomStartFrame;
	if (bNeedsRandom)
	{
		const int32 Total = GetTotalSubImages(Owner);
		const int32 First = std::clamp(StartFrame, 0, Total - 1);
		const int32 Last = (EndFrame < 0) ? Total - 1 : std::clamp(EndFrame, 0, Total - 1);
		const int32 Count = std::abs(Last - First) + 1;
		const int32 Pick = static_cast<int32>(RandomRange(Owner, 0.0f, static_cast<float>(Count)));
		Payload.RandomOffset = static_cast<float>(std::clamp(Pick, 0, Count - 1));
	}
	else
	{
		Payload.RandomOffset = 0.0f;
	}

	// 스폰된 프레임에도 올바른 칸으로 그려지게 한다
	Apply(Owner, *ParticleBase, Payload);
}

void UParticleModuleSubUV::Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime)
{
	if (!Owner || !IsValidPayloadOffset(Offset))
	{
		return;
	}

	for (int32 i = 0; i < Owner->ActiveParticles; ++i)
	{
		if (FBaseParticle* Particle = Owner->GetParticleAtIndex(i))
		{
			Apply(Owner, *Particle, GetPayload(*Particle, Offset));
		}
	}
}
