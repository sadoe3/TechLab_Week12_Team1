#pragma once

#include "Particles/ParticleModuleTypeDataBase.h"
#include "ParticleModuleTypeDataRibbon.generated.h"

/**
 * 리본 파티클마다 FBaseParticle 뒤에 붙는 Payload
 * Prev/Next는 ParticleData 안의 슬롯 번호다.
 */
struct FRibbonTypeDataPayload
{
	/** 머리 쪽 이웃 = 나보다 늦게 태어난 파티클 슬롯. INDEX_NONE이면 내가 머리 */
	int32 Prev;
	/** 꼬리 쪽 이웃 = 나보다 먼저 태어난 파티클 슬롯. INDEX_NONE이면 내가 꼬리 */
	int32 Next;
};

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

	virtual uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) const override { return sizeof(FRibbonTypeDataPayload); }
};
