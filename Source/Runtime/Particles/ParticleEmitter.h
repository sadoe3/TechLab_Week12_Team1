#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Containers/Map.h"
#include "ParticleEmitter.generated.h"

class UParticleLODLevel;
class UParticleModule;
class UParticleSystemComponent;
struct FParticleEmitterInstance;

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

	/**
	 * 런타임 이미터 인스턴스 생성 (Unreal UParticleSpriteEmitter::CreateInstance 대응)
	 * LOD0의 TypeDataModule이 있으면 그 TypeData(Mesh 등)에 생성을 위임하고, 없으면 기본 Sprite 인스턴스를 만든다.
	 */
	virtual FParticleEmitterInstance* CreateInstance(UParticleSystemComponent* InComponent);

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

	// 모듈 구성만으로 정해지는 값이라 인스턴스마다가 아니라 에셋에서 한 번만 계산한다
	int32 ParticleSize = 0;
	// 모듈별 Payload 시작 위치(파티클 슬롯 시작 기준 바이트). 모듈은 공유 에셋이라 자기 자리를 여기서 찾는다
	TMap<UParticleModule*, int32> ModuleOffsetMap;

	/** 모듈의 Payload 오프셋. 등록되지 않은 모듈이면 0 */
	int32 GetModuleOffset(UParticleModule* Module) const;

protected:
	TArray<UParticleLODLevel*> LODLevels;
};
