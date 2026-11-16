#include "EnginePCH.h"
#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleHelper.h"
#include "Render/LineBatcher.h"
#include "UObject/UObjectIterator.h"

void DrawParticleSystems(UWorld* World, FLineBatcher* LineBatcher)
{
	if (!World || !LineBatcher) return;
	for (TObjectIterator<UParticleSystemComponent> It; It; ++It)
	{
		UParticleSystemComponent* PSC = *It;
		if (!PSC || PSC->GetWorld() != World || !PSC->IsRegistered()) continue;
		PSC->DrawDebug(LineBatcher);
	}
}

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

void UParticleSystemComponent::DrawDebug(FLineBatcher* LineBatcher) const
{
	const float HalfSize = 0.05f;
	for (FParticleEmitterInstance* Owner : EmitterInstances)
	{
		BEGIN_UPDATE_LOOP
			// 흰색에서 빨강으로 바뀌어 소멸 직전인 파티클이 보이게 한다
			const float T = Particle.RelativeTime < 1.0f ? Particle.RelativeTime : 1.0f;
			const FVector4 Color(1.0f, 1.0f - T, 1.0f - T, 1.0f);
			const FVector Extent(HalfSize, HalfSize, HalfSize);
			LineBatcher->AddBox(FBox{ Particle.Location - Extent, Particle.Location + Extent }, Color);
		END_UPDATE_LOOP
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
