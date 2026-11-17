#include "EnginePCH.h"
#include "Particles/ParticleMeshEmitterInstance.h"
#include "Particles/ParticleDynamicData.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleTypeDataMesh.h"
#include "Math/Transform.h"
#include "Render/Mesh.h"
#include <algorithm>
#include <cmath>

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

float FParticleMeshEmitterInstance::GetParticleExtent(const FBaseParticle& Particle) const
{
	const UStaticMesh* Mesh = GetMesh();
	if (!Mesh)
	{
		return FParticleEmitterInstance::GetParticleExtent(Particle);
	}

	// 회전이 임의이므로 메시 AABB의 원점 기준 최대 반경 × 최대 스케일로 보수적으로 감싼다.
	const FBox& MeshBounds = Mesh->GetRenderBounds();
	const FVector FarCorner(
		std::max(std::abs(MeshBounds.Min.X), std::abs(MeshBounds.Max.X)),
		std::max(std::abs(MeshBounds.Min.Y), std::abs(MeshBounds.Max.Y)),
		std::max(std::abs(MeshBounds.Min.Z), std::abs(MeshBounds.Max.Z)));
	const float MeshRadius = std::sqrt(FarCorner.Dot(FarCorner));
	const float MaxScale = std::max({ std::abs(Particle.Size.X), std::abs(Particle.Size.Y), std::abs(Particle.Size.Z) });
	return MeshRadius * MaxScale;
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

TUniquePtr<FDynamicEmitterDataBase> FParticleMeshEmitterInstance::GetDynamicData(bool bSelected)
{
	if (!bEnabled || ActiveParticles <= 0)
	{
		return nullptr;
	}

	UStaticMesh* Mesh = GetMesh();
	if (!Mesh)
	{
		return nullptr;
	}

	auto Data = MakeUnique<FDynamicMeshEmitterData>();
	Data->bSelected = bSelected;
	Data->StaticMesh = Mesh;
	Data->OverrideMaterial = GetOverrideMaterial();
	Data->Source.bUseLocalSpace = (CurrentLODLevel && CurrentLODLevel->RequiredModule)
		? CurrentLODLevel->RequiredModule->bUseLocalSpace
		: false;
	Data->Source.SortMode = (CurrentLODLevel && CurrentLODLevel->RequiredModule)
		? static_cast<uint8>(CurrentLODLevel->RequiredModule->SortMode)
		: 0;
	Data->bCastShadows = MeshTypeData && MeshTypeData->bCastShadows;

	Data->Source.ParticleMatrices.Reserve(ActiveParticles);
	Data->Source.RelativeTimes.Reserve(ActiveParticles);
	for (int32 i = 0; i < ActiveParticles; ++i)
	{
		const FBaseParticle* Particle = GetParticleAtIndex(i);
		if (!Particle)
		{
			continue;
		}
		const FRotator Rot(Particle->Rotation.X, Particle->Rotation.Y, Particle->Rotation.Z);
		Data->Source.ParticleMatrices.Add(FTransform(Rot, Particle->Location, Particle->Size).GetLocalMatrix());
		Data->Source.RelativeTimes.Add(Particle->RelativeTime);
	}

	Data->Source.ActiveParticleCount = Data->Source.ParticleMatrices.Num();
	if (Data->Source.ActiveParticleCount == 0)
	{
		return nullptr;
	}
	return Data;
}
