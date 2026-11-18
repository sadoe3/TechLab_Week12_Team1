#pragma once

#include "GameFramework/Actor.h"
#include "Components/ParticleSystemComponent.h"
#include "Emitter.generated.h"

/**
 * UParticleSystemComponent를 기본으로 보유하는 파티클 이미터 액터 (Unreal AEmitter 대응)
 */
UCLASS()
class AEmitter : public AActor
{
	GENERATED_BODY()

public:
	AEmitter();
	virtual ~AEmitter() override = default;

	virtual void BeginPlay() override;

	UParticleSystemComponent* GetParticleSystemComponent() const { return ParticleSystemComponent; }

	/** 코드 기반 테스트용 MeshEmitter UParticleSystem 템플릿을 조립해 설정한다. */
	void SetupTestMeshParticleSystem();

	/** SpriteEmitter 테스트용 UParticleSystem 템플릿을 로드/조립해 설정한다. */
	void SetupTestSpriteParticleSystem();

	/** 코드 기반 테스트용 RibbonEmitter UParticleSystem 템플릿을 조립해 설정한다. */
	void SetupTestRibbonParticleSystem();

private:
	UParticleSystemComponent* ParticleSystemComponent = nullptr;
};
