#include "EnginePCH.h"
#include "Emitter.h"

AEmitter::AEmitter()
{
	ParticleSystemComponent = CreateDefaultSubobject<UParticleSystemComponent>("UParticleSystemComponent");
	ParticleSystemComponent->SetupAttachment(GetRootComponent());
}
