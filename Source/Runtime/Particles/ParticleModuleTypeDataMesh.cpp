#include "EnginePCH.h"
#include "Particles/ParticleModuleTypeDataMesh.h"
#include "Particles/ParticleMeshEmitterInstance.h"

FParticleEmitterInstance* UParticleModuleTypeDataMesh::CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent)
{
	return new FParticleMeshEmitterInstance(InEmitterParent, InComponent, this);
}
