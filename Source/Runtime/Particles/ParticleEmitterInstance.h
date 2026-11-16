#pragma once

#include "Core/Types.h"
#include "Math/Vector.h"

struct FBaseParticle;
class UParticleEmitter;
class UParticleLODLevel;
class UParticleSystemComponent;
struct FDynamicEmitterDataBase;

// PSC마다 따로 갖는 실행 상태. 규칙은 SpriteTemplate(에셋)에서 읽고, 결과는 여기 메모리에 쓴다
struct FParticleEmitterInstance
{
	FParticleEmitterInstance(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent);
	// Mesh 인스턴스 같은 파생형을 PSC가 기본 포인터로 지우므로 virtual
	virtual ~FParticleEmitterInstance();

	// 메모리 블록을 직접 들고 있으므로 복사되면 두 번 해제된다
	FParticleEmitterInstance(const FParticleEmitterInstance&) = delete;
	FParticleEmitterInstance& operator=(const FParticleEmitterInstance&) = delete;

	void Init();

	// PSC가 매 프레임 부른다. Spawn → Update → Kill 순서로 채운다
	void Tick(float DeltaTime);

	// StartTime부터 Increment 간격으로 프레임 안 생성 시점을 나눠 주어 한 덩어리로 뭉치지 않게 한다
	void SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity);
	// 재사용 슬롯이라 이전 파티클 값이 남아 있으므로 먼저 지우고 기본값을 넣는다
	void PreSpawn(FBaseParticle* Particle, const FVector& InitialLocation, const FVector& InitialVelocity);
	// 프레임 안에서 먼저 태어난 만큼(SpawnTime) 위치를 앞당긴다
	void PostSpawn(FBaseParticle* Particle, float Interp, float SpawnTime);

	// 지난 프레임에 수명이 다한 파티클을 한 곳에서 한 번에 지워, 모듈 루프들이 같은 집합을 보게 한다
	void KillParticles();

	// 렌더용 스냅샷을 new 해서 돌려준다. 소유권은 호출한 PSC로 넘어간다
	virtual FDynamicEmitterDataBase* GetDynamicData();

	UParticleEmitter* SpriteTemplate = nullptr;
	UParticleSystemComponent* Component = nullptr;

	int32 CurrentLODLevelIndex = 0;
	UParticleLODLevel* CurrentLODLevel = nullptr;

	uint8* ParticleData = nullptr;      // 한 블록의 시작. 해제도 이 포인터로 한다
	uint16* ParticleIndices = nullptr;  // 따로 할당하지 않고 같은 블록의 ParticleData 뒤를 가리킨다

	int32 PayloadOffset = 0;
	int32 ParticleSize = 0;
	int32 ParticleStride = 0;

	int32 ActiveParticles = 0;
	uint32 ParticleCounter = 0;
	int32 MaxActiveParticles = 0;

	float SpawnFraction = 0.0f;         // 정수로 못 만든 소수 개수를 다음 프레임으로 넘긴다
};
