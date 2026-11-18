#pragma once

#include "Particles/ParticleEmitterInstance.h"

class UParticleModuleTypeDataRibbon;
class UMaterial;
struct FRibbonTypeDataPayload;
struct FRibbonRenderBuffers;

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
	virtual void SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity) override;
	virtual void KillParticles() override;
	virtual void KillParticlesForced(bool bFireEvents = false) override;
	virtual TUniquePtr<FDynamicEmitterDataBase> GetDynamicData(bool bSelected) override;

	/** 슬롯 번호로 그 파티클의 리본 Payload를 꺼낸다 */
	FRibbonTypeDataPayload* GetRibbonPayload(int32 SlotIndex) const;
	/** 슬롯 번호로 FBaseParticle을 꺼낸다 (GetParticleAtIndex는 ParticleIndices 위치 기준이라 다르다) */
	FBaseParticle* GetParticleAtSlot(int32 SlotIndex) const;

	/** RequiredModule->Material, 없으면 기본 RibbonMaterial */
	UMaterial* GetMaterial() const;

public:
	UParticleModuleTypeDataRibbon* RibbonTypeData = nullptr;
	/** 가장 최근에 태어난 파티클(머리)의 슬롯 번호. 여기서 Next를 따라가면 생성 역순으로 꼬리까지 간다 */
	int32 HeadSlot = INDEX_NONE;
	/** 매 프레임 스냅샷에 넘겨주는 동적 VB/IB. 스냅샷은 매번 새로 만들지만 GPU 버퍼는 재사용한다 */
	TSharedPtr<FRibbonRenderBuffers> RenderBuffers;
};
