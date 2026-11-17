#pragma once

#include "Engine/PrimitiveSceneProxy.h"

class UParticleSystemComponent;
struct FParticleDynamicData;

/**
 * UParticleSystemComponent의 런타임 이미터 인스턴스들을 렌더 큐에 제출하는 씬 프록시
 * (Unreal FParticleSystemSceneProxy 대응)
 */
class FParticleSystemSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FParticleSystemSceneProxy(UParticleSystemComponent* InComponent);
	virtual ~FParticleSystemSceneProxy() override;

	virtual bool HasDynamicMeshElements() const override { return true; }
	virtual void GetDynamicMeshElements(FRenderQueue& Queue, const FViewContext& View) override;

	/** 게임 스레드에서 생성된 동적 렌더 데이터 스냅샷을 갱신한다. */
	void UpdateData(TUniquePtr<FParticleDynamicData> NewDynamicData);

private:
	/** 게임 스레드에서 전달받은 이미터별 동적 렌더 데이터 배열 */
	TUniquePtr<FParticleDynamicData> DynamicData;
};
