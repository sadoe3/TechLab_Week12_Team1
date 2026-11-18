#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleSpawn.generated.h"

struct FParticleBurst
{
	int32 Count = 0;
	int32 CountLow = -1;
	float Time = 0.0f;
};

/**
 * 파티클 스폰 속도 및 버스트를 제어하는 필수 모듈 (Unreal UParticleModuleSpawn 대응)
 */
UCLASS()
class UParticleModuleSpawn : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleSpawn() = default;
	virtual ~UParticleModuleSpawn() override = default;

	/** Particles per second right now. Rate is read at the emitter's loop fraction (UE: Rate.GetValue(EmitterTime)). */
	virtual float GetSpawnRate(FParticleEmitterInstance* Owner) const { return bProcessSpawnRate ? GetDistributionValue(Owner, Rate) : 0.0f; }

	const TArray<FParticleBurst>& GetBurstList() const { return BurstList; }
	/** 에디터의 Burst 목록 편집용 */
	TArray<FParticleBurst>& GetMutableBurstList() { return BurstList; }
	void AddBurst(const FParticleBurst& InBurst) { BurstList.Add(InBurst); }
	void ClearBurstList() { BurstList.Empty(); }

public:
	UPROPERTY(EditAnywhere)
	FDistributionFloat Rate = FDistributionFloat::MakeConstant(10.0f);

	UPROPERTY(EditAnywhere)
	float RateScale = 1.0f;

	UPROPERTY(EditAnywhere)
	bool bProcessSpawnRate = true;

	UPROPERTY(EditAnywhere)
	bool bProcessBurstList = true;

protected:
	TArray<FParticleBurst> BurstList;
};
