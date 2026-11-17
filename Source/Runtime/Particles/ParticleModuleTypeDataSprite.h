#pragma once

#include "Particles/ParticleModuleTypeDataBase.h"
#include "ParticleModuleTypeDataSprite.generated.h"

/**
 * 빌보드 스프라이트 파티클용 TypeData 모듈
 * (참고: 언리얼 기본 Sprite 이미터는 TypeDataModule == nullptr를 사용하지만,
 *  명시적 타입 구분 및 SubUV/정렬 옵션 확장을 위해 제공)
 */
UCLASS()
class UParticleModuleTypeDataSprite : public UParticleModuleTypeDataBase
{
	GENERATED_BODY()

public:
	UParticleModuleTypeDataSprite() = default;
	virtual ~UParticleModuleTypeDataSprite() override = default;

	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent) override;

public:
	UPROPERTY(EditAnywhere)
	int32 SubImages_Horizontal = 1;

	UPROPERTY(EditAnywhere)
	int32 SubImages_Vertical = 1;

	UPROPERTY(EditAnywhere)
	bool bFaceCamera = true;
};
