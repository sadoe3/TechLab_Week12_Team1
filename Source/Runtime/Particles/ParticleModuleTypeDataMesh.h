#pragma once

#include "Particles/ParticleModuleTypeDataBase.h"
#include "Render/Mesh.h"
#include "ParticleModuleTypeDataMesh.generated.h"

enum class EMeshScreenAlignment : uint8
{
	Normal = 0,
	MeshFaceCameraWithRoll,
	MeshFaceCameraWithSpin,
	MeshFaceCameraWithLockedAxis
};

/**
 * 파티클을 StaticMesh로 렌더링하는 Mesh Emitter용 TypeData 모듈 (Unreal UParticleModuleTypeDataMesh 대응)
 */
UCLASS()
class UParticleModuleTypeDataMesh : public UParticleModuleTypeDataBase
{
	GENERATED_BODY()

public:
	UParticleModuleTypeDataMesh() = default;
	virtual ~UParticleModuleTypeDataMesh() override = default;

	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent) override;

	UStaticMesh* GetMesh() const { return Mesh; }
	void SetMesh(UStaticMesh* InMesh) { Mesh = InMesh; }

public:
	UPROPERTY(EditAnywhere)
	UStaticMesh* Mesh = nullptr;

	UPROPERTY(EditAnywhere)
	float LODSizeScale = 1.0f;

	UPROPERTY(EditAnywhere)
	bool bUseStaticMeshLODs = true;

	UPROPERTY(EditAnywhere)
	bool bCastShadows = false;

	UPROPERTY(EditAnywhere)
	bool bOverrideMaterial = false;

	EMeshScreenAlignment MeshAlignment = EMeshScreenAlignment::Normal;
};
