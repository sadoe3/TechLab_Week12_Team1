#include "EnginePCH.h"
#include "Particles/ParticleModuleTypeDataSprite.h"
#include "Particles/ParticleSpriteEmitterInstance.h"

FParticleEmitterInstance* UParticleModuleTypeDataSprite::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent)
{
	return new FParticleSpriteEmitterInstance(InEmitterParent, InComponent, this);
}
