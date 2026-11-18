#include "EnginePCH.h"
#include "Particles/ParticleModuleTypeDataBeam2.h"
#include "Particles/ParticleBeamEmitterInstance.h"

FParticleEmitterInstance* UParticleModuleTypeDataBeam2::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent)
{
	return new FParticleBeamEmitterInstance(InEmitterParent, InComponent, this);
}
