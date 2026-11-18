#pragma once

#include "Particles/ParticleEmitterInstance.h"

class UParticleModuleTypeDataBeam2;
class UParticleModuleBeamTarget;
class UMaterial;

/**
 * PSC 위치(Source)에서 Target까지 분할 점을 이어 빔 1개를 그리는 Beam Emitter 런타임 인스턴스
 * (Unreal FParticleBeam2EmitterInstance 대응)
 */
struct FParticleBeamEmitterInstance : public FParticleEmitterInstance
{
public:
	FParticleBeamEmitterInstance() = default;
	FParticleBeamEmitterInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent, UParticleModuleTypeDataBeam2* InBeamTypeData);
	virtual ~FParticleBeamEmitterInstance() override = default;

	virtual void Init() override;
	virtual TUniquePtr<FDynamicEmitterDataBase> GetDynamicData(bool bSelected) override;

	/** RequiredModule->Material, 없으면 기본 RibbonMaterial */
	UMaterial* GetMaterial() const;

public:
	UParticleModuleTypeDataBeam2* BeamTypeData = nullptr;
	/** Init에서 LOD 모듈 목록을 훑어 찾아 둔다 (Unreal BeamModule_Target 캐시와 같은 역할) */
	UParticleModuleBeamTarget* BeamModule_Target = nullptr;
};
