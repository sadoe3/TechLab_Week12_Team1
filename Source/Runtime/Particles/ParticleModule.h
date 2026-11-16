#pragma once

#include "UObject/Object.h"
#include "ParticleModule.generated.h"

struct FParticleEmitterInstance;
struct FBaseParticle;

UCLASS()
class UParticleModule : public UObject
{
	GENERATED_BODY()

public:
	UParticleModule();

	// 모듈은 에셋이라 여러 PSC가 공유하므로, 결과는 Owner의 파티클 메모리(Offset 위치)에 쓴다
	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) {}
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) {}

	// 파티클 한 개마다 FBaseParticle 뒤에 붙일 바이트 수. Emitter가 이 값으로 Payload 자리를 배정한다
	virtual int32 RequiredBytes() const { return 0; }

	// LODLevel이 이 값을 보고 SpawnModules/UpdateModules 목록을 나누므로 밖에서 읽을 수 있어야 한다
	bool bSpawnModule = false;
	bool bUpdateModule = false;
};
