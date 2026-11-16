#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "ParticleEmitter.generated.h"

class UParticleLODLevel;

/**
 * 파티클 시스템 내 단일 이미터의 템플릿 정의 (Unreal Cascade UParticleEmitter 대응)
 */
UCLASS()
class UParticleEmitter : public UObject
{
	GENERATED_BODY()

public:
	UParticleEmitter() = default;
	virtual ~UParticleEmitter() override = default;

	const FString& GetEmitterName() const { return EmitterName; }
	void SetEmitterName(const FString& InName) { EmitterName = InName; }

	int32 GetInitialAllocationCount() const { return InitialAllocationCount; }
	void SetInitialAllocationCount(int32 InCount) { InitialAllocationCount = InCount; }

	bool IsDisabled() const { return bDisabled; }
	void SetDisabled(bool bInDisabled) { bDisabled = bInDisabled; }

	int32 GetNumLODLevels() const { return LODLevels.Num(); }
	UParticleLODLevel* GetLODLevel(int32 LODLevel) const;
	void AddLODLevel(UParticleLODLevel* InLODLevel);
	void ClearLODLevels();

	const TArray<UParticleLODLevel*>& GetLODLevels() const { return LODLevels; }

	/** 모듈 정보를 캐싱해 런타임 인스턴스 생성에 필요한 메타데이터를 갱신한다. */
	virtual void CacheEmitterModuleInfo();

public:
	UPROPERTY(EditAnywhere)
	FString EmitterName = "Particle Emitter";

	UPROPERTY(EditAnywhere)
	int32 InitialAllocationCount = 0;

	UPROPERTY(EditAnywhere)
	bool bDisabled = false;

	UPROPERTY(EditAnywhere)
	bool bIsSoloing = false;

protected:
	TArray<UParticleLODLevel*> LODLevels;
};
