#pragma once

#include "PrimitiveComponent.h"
#include "ParticleSystemComponent.generated.h"

class UParticleSystem;
class UWorld;
class FLineBatcher;
struct FParticleEmitterInstance;

// 스프라이트 렌더링 전까지 파티클 위치를 선으로 확인한다 (DrawCollisionShapes와 같은 방식)
void DrawParticleSystems(UWorld* World, FLineBatcher* LineBatcher);

UCLASS()
class UParticleSystemComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UParticleSystemComponent();
	virtual ~UParticleSystemComponent() override;

	// 파티클은 메시가 없어 바운드가 0이 되므로, 뷰포트에서 클릭할 수 있게 작은 상자를 준다
	virtual FBox CalcLocalBounds() const override { return FBox{ FVector(-0.5f, -0.5f, -0.5f), FVector(0.5f, 0.5f, 0.5f) }; }

	virtual void TickComponent(float DeltaTime) override;

	void SetTemplate(UParticleSystem* InTemplate);
	UParticleSystem* GetTemplate() const { return Template; }

	void DrawDebug(FLineBatcher* LineBatcher) const;

private:
	void ReleaseEmitterInstances();

	UParticleSystem* Template = nullptr;

	// Phase 2: Template->Emitters와 인덱스가 1:1로 대응하는 실행 상태
	TArray<FParticleEmitterInstance*> EmitterInstances;
};
