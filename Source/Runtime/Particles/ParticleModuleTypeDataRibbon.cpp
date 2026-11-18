#include "EnginePCH.h"
#include "Particles/ParticleModuleTypeDataRibbon.h"
#include "Particles/ParticleRibbonEmitterInstance.h"

FParticleEmitterInstance* UParticleModuleTypeDataRibbon::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent)
{
	return new FParticleRibbonEmitterInstance(InEmitterParent, InComponent, this);
}
