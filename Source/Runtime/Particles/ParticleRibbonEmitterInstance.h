#pragma once

#include "Particles/ParticleEmitterInstance.h"

class UParticleModuleTypeDataRibbon;

/**
 * 파티클을 생성 순서대로 이어 띠를 만드는 Ribbon Emitter 런타임 인스턴스
 * (Unreal FParticleRibbonEmitterInstance 대응)
 */
struct FParticleRibbonEmitterInstance : public FParticleEmitterInstance
{
public:
	FParticleRibbonEmitterInstance() = default;
	FParticleRibbonEmitterInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent, UParticleModuleTypeDataRibbon* InRibbonTypeData);
	virtual ~FParticleRibbonEmitterInstance() override = default;

	virtual void Init() override;
	virtual TUniquePtr<FDynamicEmitterDataBase> GetDynamicData(bool bSelected) override;

public:
	UParticleModuleTypeDataRibbon* RibbonTypeData = nullptr;
};
