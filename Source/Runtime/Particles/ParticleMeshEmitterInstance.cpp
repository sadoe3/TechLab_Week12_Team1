#include "EnginePCH.h"
#include "Particles/ParticleMeshEmitterInstance.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleTypeDataMesh.h"
#include "Math/Transform.h"

FParticleMeshEmitterInstance::FParticleMeshEmitterInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent, UParticleModuleTypeDataMesh* InMeshTypeData)
	: MeshTypeData(InMeshTypeData)
{
	InitParameters(InEmitterParent, InComponent);
}

void FParticleMeshEmitterInstance::Init()
{
	FParticleEmitterInstance::Init();

	if (!MeshTypeData && CurrentLODLevel)
	{
		MeshTypeData = Cast<UParticleModuleTypeDataMesh>(CurrentLODLevel->TypeDataModule);
	}
}

void FParticleMeshEmitterInstance::Tick(float DeltaTime, bool bSuppressSpawning)
{
	FParticleEmitterInstance::Tick(DeltaTime, bSuppressSpawning);
}

void FParticleMeshEmitterInstance::UpdateBoundingBox(float DeltaTime)
{
	FParticleEmitterInstance::UpdateBoundingBox(DeltaTime);
}

UStaticMesh* FParticleMeshEmitterInstance::GetMesh() const
{
	return MeshTypeData ? MeshTypeData->GetMesh() : nullptr;
}

UMaterial* FParticleMeshEmitterInstance::GetOverrideMaterial() const
{
	if (MeshTypeData && MeshTypeData->bOverrideMaterial && CurrentLODLevel && CurrentLODLevel->RequiredModule)
	{
		return CurrentLODLevel->RequiredModule->Material;
	}
	return nullptr;
}

void FParticleMeshEmitterInstance::BuildInstanceMatrices(const FMatrix& ComponentToWorld, TArray<FMatrix>& OutMatrices) const
{
	OutMatrices.Empty();
	if (ActiveParticles <= 0)
	{
		return;
	}

	const bool bUseLocalSpace = (CurrentLODLevel && CurrentLODLevel->RequiredModule)
		? CurrentLODLevel->RequiredModule->bUseLocalSpace
		: false;

	OutMatrices.Reserve(ActiveParticles);
	for (int32 i = 0; i < ActiveParticles; ++i)
	{
		const FBaseParticle* Particle = GetParticleAtIndex(i);
		if (!Particle)
		{
			continue;
		}

		const FRotator Rot(Particle->Rotation.X, Particle->Rotation.Y, Particle->Rotation.Z);
		const FMatrix LocalMat = FTransform(Rot, Particle->Location, Particle->Size).GetLocalMatrix();

		if (bUseLocalSpace)
		{
			OutMatrices.Add(LocalMat * ComponentToWorld);
		}
		else
		{
			// 월드 스페이스 파티클일 때도 컴포넌트 위치 오프셋을 스폰 시 반영하지 않은 경우를 위해
			// 초기 프로토타입에서는 LocalMat + ComponentToWorld Translation 또는 LocalMat 그대로 사용
			OutMatrices.Add(LocalMat);
		}
	}
}
