#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "ParticleModule.generated.h"

class UParticleModuleTypeDataBase;
struct FParticleEmitterInstance;
struct FBaseParticle;

/**
 * 모든 파티클 모듈의 기반 클래스 (Unreal Cascade UParticleModule 대응)
 */
UCLASS()
class UParticleModule : public UObject
{
	GENERATED_BODY()

public:
	UParticleModule() = default;
	virtual ~UParticleModule() override = default;

	/** 파티클 스폰 시 1회 호출 */
	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) {}

	/** 매 프레임 살아있는 파티클 업데이트 시 호출 */
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) {}

	/** 파티클 1개당 추가로 필요한 Payload 바이트 크기 */
	virtual uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) const { return 0; }

	/** 이미터 인스턴스당 추가로 필요한 Payload 바이트 크기 */
	virtual uint32 RequiredBytesPerInstance() const { return 0; }

public:
	UPROPERTY(EditAnywhere)
	bool bSpawnModule = false;

	UPROPERTY(EditAnywhere)
	bool bUpdateModule = false;

	UPROPERTY(EditAnywhere)
	bool bFinalUpdateModule = false;

	UPROPERTY(EditAnywhere)
	bool bEnabled = true;
};
