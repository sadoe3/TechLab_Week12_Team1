#pragma once

#include "PrimitiveSceneProxy.h"

class UParticleSystemComponent;

// 파티클은 메시가 없어 매 뷰마다 PSC의 EmitterRenderData를 읽어 패킷을 만든다
class FParticleSystemSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FParticleSystemSceneProxy(UParticleSystemComponent* InComponent);

	void UpdateRenderState() override;
	bool HasDynamicMeshElements() const override { return true; }

	void GetDynamicMeshElements(FRenderQueue& Queue, const FViewContext& View) override;
};
