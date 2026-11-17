#include "EnginePCH.h"
#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleHelper.h"
#include "Particles/ParticleDynamicData.h"
#include "Engine/ParticleSystemSceneProxy.h"

UParticleSystemComponent::UParticleSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	SetTemplate(UParticleSystem::GetDefault());
}

UParticleSystemComponent::~UParticleSystemComponent()
{
	ReleaseEmitterInstances();
}

void UParticleSystemComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		Instance->Tick(DeltaTime);
	}

	// 이전 프레임 RenderData를 치우고, 인스턴스마다 GetDynamicData()로 다시 채운다
	ReleaseEmitterRenderData();
	
	for (int32 i = 0; i < EmitterInstances.Num(); ++i)
	{
		auto Data = EmitterInstances[i]->GetDynamicData();
		EmitterRenderData.Add(Data);
		Data->EmitterIndex = i;
	}

	// 프레임 마다 버전을 올린다
	++RenderDataVersion;
}

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	return new FParticleSystemSceneProxy(this);
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* InTemplate)
{
	// 에셋이 바뀌면 Stride도 바뀔 수 있어 기존 파티클 메모리는 쓸 수 없다
	ReleaseEmitterInstances();
	Template = InTemplate;
	if (!Template)
	{
		return;
	}

	// Emitters와 같은 인덱스로 맞춰 두어야 i번 인스턴스가 i번 이미터 규칙을 따른다
	for (UParticleEmitter* Emitter : Template->Emitters)
	{
		FParticleEmitterInstance* Instance = new FParticleEmitterInstance(Emitter, this);
		Instance->Init();
		EmitterInstances.Add(Instance);
	}
}

void UParticleSystemComponent::ReleaseEmitterRenderData()
{
	for (FDynamicEmitterDataBase* Data : EmitterRenderData)
	{
		delete Data;
	}
	EmitterRenderData.Empty();
}

void UParticleSystemComponent::ReleaseEmitterInstances()
{
	// 스냅샷의 Stride가 옛 인스턴스 기준이라 인스턴스와 함께 버린다
	ReleaseEmitterRenderData();

	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		delete Instance;
	}
	EmitterInstances.Empty();
}
