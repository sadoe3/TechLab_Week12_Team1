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

	/** 현재 프레임에서 스폰할 개수를 계산한다 (Rate 기반). */
	virtual float GetSpawnRate() const { return bProcessSpawnRate ? Rate : 0.0f; }

	const TArray<FParticleBurst>& GetBurstList() const { return BurstList; }
	/** 에디터의 Burst 목록 편집용 */
	TArray<FParticleBurst>& GetMutableBurstList() { return BurstList; }
	void AddBurst(const FParticleBurst& InBurst) { BurstList.Add(InBurst); }
	void ClearBurstList() { BurstList.Empty(); }

public:
	UPROPERTY(EditAnywhere)
	float Rate = 10.0f;

	UPROPERTY(EditAnywhere)
	float RateScale = 1.0f;

	UPROPERTY(EditAnywhere)
	bool bProcessSpawnRate = true;

	UPROPERTY(EditAnywhere)
	bool bProcessBurstList = true;

protected:
	TArray<FParticleBurst> BurstList;
};
