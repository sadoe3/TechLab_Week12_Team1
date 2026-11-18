#pragma once

#include "Components/PrimitiveComponent.h"
#include "Particles/ParticleSystem.h"
#include <random>
#include "ParticleSystemComponent.generated.h"

struct FParticleEmitterInstance;
struct FParticleDynamicData;

/**
 * UParticleSystem 에셋을 월드에 배치하고 런타임 이미터 인스턴스들을 구동하는 컴포넌트
 * (Unreal UParticleSystemComponent 대응)
 */
UCLASS()
class UParticleSystemComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UParticleSystemComponent();
	virtual ~UParticleSystemComponent() override;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;
	virtual FBox CalcLocalBounds() const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual void PostEditChangeProperty(const FProperty& Property) override;

	void SetTemplate(UParticleSystem* NewTemplate);
	UParticleSystem* GetTemplate() const { return Template; }

	void ActivateSystem(bool bFlagAsJustAttached = false);
	void DeactivateSystem();
	bool IsActive() const { return bIsActive; }

	void UpdateInEditor(float DeltaTime);

	void InitParticles();
	void ResetParticles(bool bEmptyInstances = false);

	const TArray<FParticleEmitterInstance*>& GetEmitterInstances() const { return EmitterInstances; }

	/** [0, 1) 균등 난수. 모듈의 랜덤 분포가 사용한다 (Unreal UParticleSystemComponent::RandomStream 대응) */
	float GetRandomFraction() { return RandomDistribution(RandomStream); }

public:
	UPROPERTY(EditAnywhere)
	UParticleSystem* Template = nullptr;

	UPROPERTY(EditAnywhere)
	bool bAutoActivate = true;

	UPROPERTY(EditAnywhere)
	bool bSuppressSpawning = false;

protected:
	bool HasAnyActiveParticles() const;
	TUniquePtr<FParticleDynamicData> CreateDynamicData();
	void SendRenderDynamicData();
	/** 파티클 바운드가 바뀌었으면 씬 프록시의 컬링 바운드를 갱신하도록 요청한다. */
	void UpdateRenderBounds();

	TArray<FParticleEmitterInstance*> EmitterInstances;
	/** 마지막으로 프록시에 반영한 로컬 바운드 (변경 감지용) */
	FBox CachedLocalBounds = { FVector::ZeroVector, FVector::ZeroVector };

	std::mt19937 RandomStream{ std::random_device{}() };
	std::uniform_real_distribution<float> RandomDistribution{ 0.0f, 1.0f };
	bool bIsActive = false;
	bool bWasDeactivated = false;

	bool bEditorAutoActivated = false;
};
