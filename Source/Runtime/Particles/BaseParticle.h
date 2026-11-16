#pragma once

#include "Math/EngineMath.h"

/**
 * 파티클 1개의 런타임 시뮬레이션 데이터 (Unreal FBaseParticle 대응)
 */
struct FBaseParticle
{
	FVector OldLocation = FVector::ZeroVector;
	FVector Location = FVector::ZeroVector;
	FVector BaseVelocity = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;

	FVector BaseSize = FVector::OneVector;
	FVector Size = FVector::OneVector;

	/** 오일러 회전 (Pitch, Yaw, Roll - Degree) */
	FVector Rotation = FVector::ZeroVector;
	FVector RotationRate = FVector::ZeroVector;

	/** 수명 진행도 (0.0 ~ 1.0) */
	float RelativeTime = 0.0f;
	/** 1.0f / MaxLifetime */
	float OneOverMaxLifetime = 1.0f;

	FVector4 BaseColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	FVector4 Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f);

	int32 Flags = 0;
};

#define DECLARE_PARTICLE(Name, Address) \
	FBaseParticle& Name = *((FBaseParticle*)(Address));

#define DECLARE_PARTICLE_PTR(Name, Address) \
	FBaseParticle* Name = (FBaseParticle*)(Address);
