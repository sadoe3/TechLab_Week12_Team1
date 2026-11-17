#pragma once

#include "Particles/ParticleEmitterInstance.h"

class UParticleModuleTypeDataSprite;
class UMaterial;
struct FViewContext;

/**
 * 빌보드(Quad) 스프라이트를 파티클 단위로 렌더링하는 Sprite Emitter 런타임 인스턴스
 * (Unreal FParticleSpriteEmitterInstance 대응)
 */
struct FParticleSpriteEmitterInstance : public FParticleEmitterInstance
{
public:
	FParticleSpriteEmitterInstance() = default;
	FParticleSpriteEmitterInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent, UParticleModuleTypeDataSprite* InSpriteTypeData = nullptr);
	virtual ~FParticleSpriteEmitterInstance() override = default;

	virtual void Init() override;
	virtual void Tick(float DeltaTime, bool bSuppressSpawning) override;
	virtual void UpdateBoundingBox(float DeltaTime) override;
	virtual TUniquePtr<FDynamicEmitterDataBase> GetDynamicData(bool bSelected) override;

	UMaterial* GetMaterial() const;

	/** 현재 뷰 기준으로 활성 파티클들의 빌보드 월드 변환 행렬을 OutMatrices에 채운다. */
	void BuildBillboardMatrices(const FViewContext& View, const FMatrix& ComponentToWorld, TArray<FMatrix>& OutMatrices) const;

public:
	UParticleModuleTypeDataSprite* SpriteTypeData = nullptr;
};
