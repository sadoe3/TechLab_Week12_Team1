#pragma once

#include "UObject/Object.h"
#include "ParticleSystem.generated.h"

class UParticleEmitter;

UCLASS()
class UParticleSystem : public UObject
{
	GENERATED_BODY()

public:
	UParticleSystem();

	// 파일 로드가 생기기 전까지 쓸 기본 에셋. 처음 부를 때 한 번만 만들고 모든 PSC가 공유한다
	static UParticleSystem* GetDefault();

	TArray<UParticleEmitter*> Emitters;
};
