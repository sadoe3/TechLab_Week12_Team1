#pragma once

#include "Core/Types.h"

class UParticleEmitter;
class UParticleLODLevel;
class UParticleSystemComponent;

// PSC마다 따로 갖는 실행 상태. 규칙은 SpriteTemplate(에셋)에서 읽고, 결과는 여기 메모리에 쓴다
struct FParticleEmitterInstance
{
	FParticleEmitterInstance(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent);
	~FParticleEmitterInstance();

	// 메모리 블록을 직접 들고 있으므로 복사되면 두 번 해제된다
	FParticleEmitterInstance(const FParticleEmitterInstance&) = delete;
	FParticleEmitterInstance& operator=(const FParticleEmitterInstance&) = delete;

	void Init();

	// PSC가 매 프레임 부른다. Spawn → Update → Kill 순서로 채운다
	void Tick(float DeltaTime);

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
};
