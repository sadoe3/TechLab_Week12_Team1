#pragma once

#include "Particles/ParticleEmitterInstance.h"

class UParticleModuleTypeDataMesh;
class UStaticMesh;
class UMaterial;

/**
 * StaticMesh를 파티클 단위로 렌더링하는 Mesh Emitter 런타임 인스턴스
 * (Unreal FParticleMeshEmitterInstance 대응)
 */
struct FParticleMeshEmitterInstance : public FParticleEmitterInstance
{
public:
	FParticleMeshEmitterInstance() = default;
	FParticleMeshEmitterInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent, UParticleModuleTypeDataMesh* InMeshTypeData);
	virtual ~FParticleMeshEmitterInstance() override = default;

	virtual void Init() override;
	virtual void Tick(float DeltaTime, bool bSuppressSpawning) override;
	virtual void UpdateBoundingBox(float DeltaTime) override;
	virtual float GetParticleExtent(const FBaseParticle& Particle) const override;
	virtual TUniquePtr<FDynamicEmitterDataBase> GetDynamicData(bool bSelected) override;

	UStaticMesh* GetMesh() const;
	UMaterial* GetOverrideMaterial() const;

	/** 활성 파티클들의 월드 변환 행렬을 OutMatrices에 채운다. */
	void BuildInstanceMatrices(const FMatrix& ComponentToWorld, TArray<FMatrix>& OutMatrices) const;

public:
	UParticleModuleTypeDataMesh* MeshTypeData = nullptr;
};
