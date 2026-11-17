#pragma once

#include "PrimitiveSceneProxy.h"
#include "Render/Buffer.h"
#include "Particles/ParticleDynamicData.h"

class UParticleSystemComponent;

// 파티클은 메시가 없어 매 뷰마다 PSC의 EmitterRenderData를 읽어 패킷을 만든다
class FParticleSystemSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FParticleSystemSceneProxy(UParticleSystemComponent* InComponent);

	void UpdateRenderState() override;
	bool HasDynamicMeshElements() const override { return true; }

	void GetDynamicMeshElements(FRenderQueue& Queue, const FViewContext& View) override;

private:
	// 이번 프레임 스냅샷으로 정점 버퍼를 채운다. 스냅샷과 카메라가 그대로면 다시 채우지 않는다
	void UpdateDynamicBuffers(const UParticleSystemComponent* PSC, const FViewContext& View);

	// WRITE_DISCARD로 덮어쓰므로 이미터끼리 공유하면 마지막 이미터 내용만 남는다. [EmitterIndex]
	TArray<TUniquePtr<FVertexBuffer>> EmitterVertexBuffers;
	// 쿼드 인덱스 패턴은 내용이 같으니 가장 큰 이미터 기준으로 하나만 만들어 앞부분을 나눠 쓴다
	TUniquePtr<FIndexBuffer> IndexBuffer;

	// 정렬 결과는 카메라에 따라 달라지므로 버전과 함께 카메라 위치도 비교한다
	uint32 FilledVersion = ~0u;
	FVector FilledCameraPosition = FVector::ZeroVector;

	// 매 프레임 할당을 피하려고 이미터끼리 돌려 쓴다
	TArray<FParticleOrder> SortOrder;
};
