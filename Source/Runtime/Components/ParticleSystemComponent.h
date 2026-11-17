#pragma once

#include "PrimitiveComponent.h"
#include "ParticleSystemComponent.generated.h"

class UParticleSystem;
struct FParticleEmitterInstance;
struct FDynamicEmitterDataBase;

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

	// 기본 프록시는 메시가 없으면 아무것도 제출하지 않으므로 파티클 전용 프록시로 바꾼다
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	void SetTemplate(UParticleSystem* InTemplate);
	UParticleSystem* GetTemplate() const { return Template; }

	const TArray<FDynamicEmitterDataBase*>& GetEmitterRenderData() const { return EmitterRenderData; }
	uint32 GetRenderDataVersion() const { return RenderDataVersion; }
	int32 GetTranslucencySortPriority() const { return TranslucencySortPriority; }

	// 거리 정렬보다 먼저 비교된다. 클수록 나중에 그려져 앞에 보인다
	UPROPERTY(EditAnywhere)
	int32 TranslucencySortPriority = 0;

private:
	void ReleaseEmitterInstances();
	void ReleaseEmitterRenderData();

	UParticleSystem* Template = nullptr;

	// Template->Emitters와 인덱스가 1:1로 대응하는 실행 상태
	TArray<FParticleEmitterInstance*> EmitterInstances;

	// 매 프레임 Tick이 끝난 뒤 새로 만드는 렌더용 스냅샷. 렌더러는 읽기만 한다
	TArray<FDynamicEmitterDataBase*> EmitterRenderData;

	// 스냅샷을 새로 만들 때마다 오른다. 프록시는 이 값으로 뷰가 여러 개여도 정점을 한 번만 채운다
	uint32 RenderDataVersion = 0;
};
