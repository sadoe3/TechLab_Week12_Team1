#pragma once

#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleDynamicData.h"

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
	virtual void UpdateBoundingBox(float DeltaTime) override;
	virtual TUniquePtr<FDynamicEmitterDataBase> GetDynamicData(bool bSelected) override;

	/** 로컬 공간 Source(원점) → Target을 Segments 구간으로 나눈 점 목록. 폭·색은 첫 번째 활성 파티클(빔 파티클)에서 가져온다 */
	void BuildBeamPoints(TArray<FDynamicRibbonEmitterReplayData::FRibbonPoint>& OutPoints) const;

	/** BeamMethod·bTargetAbsolute를 반영한 로컬 공간 끝점. 정할 수 없으면 false */
	bool GetLocalTarget(FVector& OutTarget) const;
	/** Alpha(0 = Source, 1 = Target) 위치의 폭 배율 (TaperMethod) */
	float GetTaperScale(float Alpha) const;

	/** RequiredModule->Material, 없으면 기본 RibbonMaterial */
	UMaterial* GetMaterial() const;

public:
	UParticleModuleTypeDataBeam2* BeamTypeData = nullptr;
	/** Init에서 LOD 모듈 목록을 훑어 찾아 둔다 (Unreal BeamModule_Target 캐시와 같은 역할) */
	UParticleModuleBeamTarget* BeamModule_Target = nullptr;
	/** 매 프레임 스냅샷에 넘겨주는 동적 VB/IB. 스냅샷은 매번 새로 만들지만 GPU 버퍼는 재사용한다 */
	TSharedPtr<FRibbonRenderBuffers> RenderBuffers;
};
