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

	/** 카메라 거리로 쓸 LOD 인덱스를 고른다 (Unreal UParticleSystemComponent::DetermineLODLevel) */
	int32 DetermineLODLevel() const;
	/** 모든 이미터 인스턴스의 LOD를 바꾼다. 범위 밖이면 맞춰 자른다 */
	void SetLODLevel(int32 InLODLevel);
	int32 GetLODLevel() const { return LODLevel; }

	/** [0, 1) 균등 난수. 모듈의 랜덤 분포가 사용한다 (Unreal UParticleSystemComponent::RandomStream 대응) */
	float GetRandomFraction() { return RandomDistribution(RandomStream); }

public:
	UPROPERTY(EditAnywhere)
	UParticleSystem* Template = nullptr;

	UPROPERTY(EditAnywhere)
	bool bAutoActivate = true;

	UPROPERTY(EditAnywhere)
	bool bSuppressSpawning = false;

	// 거리를 이 간격(초)마다만 잰다. LOD는 자주 바뀔 필요가 없다 (Unreal LODDistanceCheckTime)
	UPROPERTY(EditAnywhere)
	float LODDistanceCheckTime = 0.25f;

	// false면 거리로 고르지 않고 SetLODLevel로 정한 LOD를 유지한다 (에디터 프리뷰의 LOD 고정용)
	bool bAutoLOD = true;

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

	int32 LODLevel = 0;
	float LODTimer = 0.0f;
};
