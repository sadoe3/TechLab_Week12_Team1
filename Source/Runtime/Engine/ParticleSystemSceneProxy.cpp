#include "EnginePCH.h"
#include "Engine/ParticleSystemSceneProxy.h"
#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleDynamicData.h"

FParticleSystemSceneProxy::FParticleSystemSceneProxy(UParticleSystemComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	// 렌더러가 Show Flag(bShowParticles)로 이 프록시를 걸러낼 수 있게 표시한다
	bParticle = true;
}

FParticleSystemSceneProxy::~FParticleSystemSceneProxy() = default;

void FParticleSystemSceneProxy::UpdateData(TUniquePtr<FParticleDynamicData> NewDynamicData)
{
	DynamicData = std::move(NewDynamicData);
}

void FParticleSystemSceneProxy::GetDynamicMeshElements(FRenderQueue& Queue, const FViewContext& View)
{
	if (!DynamicData)
	{
		return;
	}

	for (const TUniquePtr<FDynamicEmitterDataBase>& EmitterData : DynamicData->DynamicEmitterDataArray)
	{
		if (EmitterData)
		{
			EmitterData->GetDynamicMeshElements(this, View, Queue);
		}
	}
}
