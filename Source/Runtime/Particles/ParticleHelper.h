#pragma once

#include "Math/Vector.h"
#include "Math/Color.h"
#include <cstdlib>

// 모든 파티클 슬롯의 오프셋 0에 놓이는 공통 앞부분. 모듈 Payload는 이 뒤에 붙는다
struct FBaseParticle
{
	// FVector(12B) 뒤에 float(4B)를 붙여 16바이트 묶음을 맞춘다
	FVector Location;
	float RelativeTime;          // 0에서 시작해 1이 되면 죽는다

	FVector Velocity;
	float OneOverMaxLifetime;    // 1/수명초를 저장한다. 0이면 영원히 산다

	FVector BaseVelocity;
	float Rotation;

	FVector Size;
	float RotationRate;

	FLinearColor Color;
};
static_assert(sizeof(FBaseParticle) % 16 == 0, "FBaseParticle은 16바이트 배수여야 Payload 시작이 정렬된다");

#define DECLARE_PARTICLE_PTR(Name, Address) FBaseParticle* Name = (FBaseParticle*)(Address);

#define BEGIN_UPDATE_LOOP \
  for (int32 i = Owner->ActiveParticles - 1; i >= 0; i--) { \
    const int32 CurrentIndex = Owner->ParticleIndices[i]; \
    FBaseParticle& Particle = *((FBaseParticle*)(Owner->ParticleData + CurrentIndex * Owner->ParticleStride));
#define END_UPDATE_LOOP }


inline float RandRange(float Min, float Max)
{
	return Min + (Max - Min) * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
}
