#pragma once

#include "Particles/ParticleModule.h"
#include "Render/Buffer.h"
#include "Render/Material.h"
#include "ParticleModuleRequired.generated.h"

/**
 * 모든 LODLevel에 필수로 포함되는 이미터 기본 설정 모듈 (Unreal UParticleModuleRequired 대응)
 */
UCLASS()
class UParticleModuleRequired : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleRequired() = default;
	virtual ~UParticleModuleRequired() override = default;

public:
	UPROPERTY(EditAnywhere)
	UMaterial* Material = nullptr;

	UPROPERTY(EditAnywhere)
	bool bUseLocalSpace = false;

	UPROPERTY(EditAnywhere)
	bool bKillOnDeactivate = false;

	UPROPERTY(EditAnywhere)
	bool bKillOnCompleted = false;

	UPROPERTY(EditAnywhere)
	float EmitterDuration = 1.0f;

	UPROPERTY(EditAnywhere)
	float EmitterDelay = 0.0f;

	/** 0이면 무한 루프 */
	UPROPERTY(EditAnywhere)
	int32 EmitterLoops = 0;

	UPROPERTY(EditAnywhere)
	int32 MaxDrawCount = 500;
};
