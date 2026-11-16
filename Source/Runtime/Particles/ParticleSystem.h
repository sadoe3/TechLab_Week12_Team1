#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Math/Box.h"
#include "ParticleEmitter.h"
#include "ParticleSystem.generated.h"

/**
 * 하나 이상의 UParticleEmitter를 포함하는 파티클 시스템 에셋 (Unreal UParticleSystem 대응)
 */
UCLASS()
class UParticleSystem : public UObject
{
	GENERATED_BODY()

public:
	UParticleSystem() = default;
	virtual ~UParticleSystem() override = default;

	int32 GetNumEmitters() const { return Emitters.Num(); }
	UParticleEmitter* GetEmitter(int32 Index) const;
	const TArray<UParticleEmitter*>& GetEmitters() const { return Emitters; }

	void AddEmitter(UParticleEmitter* InEmitter);
	bool RemoveEmitter(UParticleEmitter* InEmitter);
	bool RemoveEmitterAt(int32 Index);
	void ClearEmitters();

	/** 모든 이미터의 모듈/LOD 캐시를 갱신한다. */
	void UpdateAllModuleLists();

public:
	UPROPERTY(EditAnywhere)
	float UpdateTime_FPS = 60.0f;

	UPROPERTY(EditAnywhere)
	float UpdateTime_Delta = 1.0f / 60.0f;

	UPROPERTY(EditAnywhere)
	float WarmupTime = 0.0f;

	UPROPERTY(EditAnywhere)
	float WarmupTickRate = 0.0f;

	UPROPERTY(EditAnywhere)
	bool bUseFixedRelativeBoundingBox = false;

	FBox FixedRelativeBoundingBox = { FVector(-100.0f, -100.0f, -100.0f), FVector(100.0f, 100.0f, 100.0f) };

	UPROPERTY(EditAnywhere)
	bool bOrientZAxisTowardCamera = false;

protected:
	TArray<UParticleEmitter*> Emitters;
};
