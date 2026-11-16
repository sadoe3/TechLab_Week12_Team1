#include "EnginePCH.h"
#include "ParticleSystemSceneProxy.h"

#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleDynamicData.h"

FParticleSystemSceneProxy::FParticleSystemSceneProxy(UParticleSystemComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
}

void FParticleSystemSceneProxy::UpdateRenderState()
{
	// RenderState를 비워 두면 그림자 패스가 이 프록시를 건너뛴다
	RenderState.reset();
	UpdateCommonFlags();
}

void FParticleSystemSceneProxy::GetDynamicMeshElements(FRenderQueue& Queue, const FViewContext& View)
{
	const UParticleSystemComponent* PSC = static_cast<const UParticleSystemComponent*>(GetComponent());
	for (const FDynamicEmitterDataBase* Data : PSC->GetEmitterRenderData())
	{
		// TODO: Step 5 - 이미터마다 정점을 채우고 패킷 1개를 제출한다
	}
}
