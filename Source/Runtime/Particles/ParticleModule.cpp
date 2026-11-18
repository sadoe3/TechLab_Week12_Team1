#include "EnginePCH.h"
#include "Particles/ParticleModule.h"
#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleEmitterInstance.h"

float UParticleModule::RandomRange(FParticleEmitterInstance* Owner, float Min, float Max)
{
	if (Min == Max || !Owner || !Owner->Component)
	{
		return Min;
	}
	return Min + (Max - Min) * Owner->Component->GetRandomFraction();
}

FVector UParticleModule::RandomRange(FParticleEmitterInstance* Owner, const FVector& Min, const FVector& Max)
{
	// Unreal UDistributionVectorUniform과 같이 축마다 독립적으로 뽑는다.
	return FVector(
		RandomRange(Owner, Min.X, Max.X),
		RandomRange(Owner, Min.Y, Max.Y),
		RandomRange(Owner, Min.Z, Max.Z));
}

namespace
{
	float GetCurveTime(const FParticleEmitterInstance* Owner)
	{
		return Owner ? Owner->GetEmitterTimeFraction() : 0.0f;
	}

	float GetRandom01(const FParticleEmitterInstance* Owner)
	{
		return (Owner && Owner->Component) ? Owner->Component->GetRandomFraction() : 0.0f;
	}
}

float UParticleModule::GetDistributionValue(FParticleEmitterInstance* Owner, const FDistributionFloat& Distribution)
{
	return Distribution.GetValue(GetCurveTime(Owner), [Owner]() { return GetRandom01(Owner); });
}

FVector UParticleModule::GetDistributionValue(FParticleEmitterInstance* Owner, const FDistributionVector& Distribution)
{
	return Distribution.GetValue(GetCurveTime(Owner), [Owner]() { return GetRandom01(Owner); });
}

FVector UParticleModule::EmitterToSimulationVector(const FParticleEmitterInstance* Owner, const FVector& Vector)
{
	// 로컬 공간 시뮬레이션이면 이미터 공간 = 시뮬레이션 공간
	if (!Owner || Owner->IsLocalSpace() || !Owner->Component)
	{
		return Vector;
	}
	const FVector4 Transformed = FVector4(Vector.X, Vector.Y, Vector.Z, 0.0f) * Owner->Component->GetWorldMatrix();
	return FVector(Transformed.X, Transformed.Y, Transformed.Z);
}

FVector UParticleModule::WorldToSimulationVector(const FParticleEmitterInstance* Owner, const FVector& Vector)
{
	// 월드 공간 시뮬레이션이면 월드 = 시뮬레이션 공간
	if (!Owner || !Owner->IsLocalSpace() || !Owner->Component)
	{
		return Vector;
	}
	const FVector4 Transformed = FVector4(Vector.X, Vector.Y, Vector.Z, 0.0f) * Owner->Component->GetWorldMatrix().Inverse();
	return FVector(Transformed.X, Transformed.Y, Transformed.Z);
}

FVector UParticleModule::GetSimulationOrigin(const FParticleEmitterInstance* Owner)
{
	if (!Owner || Owner->IsLocalSpace() || !Owner->Component)
	{
		return FVector::ZeroVector;
	}
	return Owner->Component->GetWorldLocation();
}
