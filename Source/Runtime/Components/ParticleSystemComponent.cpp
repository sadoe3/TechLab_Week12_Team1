#include "EnginePCH.h"
#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"

UParticleSystemComponent::UParticleSystemComponent()
{
	SetTemplate(UParticleSystem::GetDefault());
}

UParticleSystemComponent::~UParticleSystemComponent()
{
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* InTemplate)
{
	// Phase 2: 여기서 EmitterInstances를 버리고 새 Template 기준으로 다시 만든다
	Template = InTemplate;
}
