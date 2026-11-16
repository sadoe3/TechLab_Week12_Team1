#include "EnginePCH.h"
#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitterInstance.h"

UParticleSystemComponent::UParticleSystemComponent()
{
	// 우리 엔진은 기본값이 Tick 꺼짐이라, 시뮬레이션을 돌리려면 직접 켜야 한다
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

void UParticleSystemComponent::ReleaseEmitterInstances()
{
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		delete Instance;
	}
	EmitterInstances.Empty();
}
