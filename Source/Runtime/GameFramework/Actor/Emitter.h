#pragma once

#include "GameFramework/Actor.h"
#include "Components/ParticleSystemComponent.h"
#include "Emitter.generated.h"

// UE의 AEmitter처럼 PSC 하나만 들고 레벨에 놓이는 액터
UCLASS()
class AEmitter : public AActor
{
	GENERATED_BODY()

public:
	AEmitter();

	UParticleSystemComponent* GetParticleSystemComponent() const { return ParticleSystemComponent; }

private:
	UParticleSystemComponent* ParticleSystemComponent = nullptr;
};
