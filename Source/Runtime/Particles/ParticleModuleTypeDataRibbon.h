#pragma once

#include "Particles/ParticleModuleTypeDataBase.h"
#include "ParticleModuleTypeDataRibbon.generated.h"

/**
 * 파티클을 생성 순서대로 이어 띠(트레일)로 렌더링하는 Ribbon Emitter용 TypeData 모듈
 * (Unreal UParticleModuleTypeDataRibbon 대응)
 */
UCLASS()
class UParticleModuleTypeDataRibbon : public UParticleModuleTypeDataBase
{
	GENERATED_BODY()

public:
	UParticleModuleTypeDataRibbon() = default;
	virtual ~UParticleModuleTypeDataRibbon() override = default;

	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent) override;
};
