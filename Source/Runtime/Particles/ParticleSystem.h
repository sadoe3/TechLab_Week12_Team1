#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Math/Box.h"
#include "ParticleEmitter.h"
#include "ParticleSystem.generated.h"

UENUM()
enum class EParticleSystemLODMethod : uint8
{
	Automatic = 0,		// 카메라 거리로 LODDistances를 보고 고른다
	DirectSet = 1,		// 코드나 에디터가 정한 LOD를 그대로 쓴다
};

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
	/** Index 자리에 넣는다. 범위를 벗어나면 끝에 붙인다. */
	void InsertEmitter(UParticleEmitter* InEmitter, int32 Index);
	/** 없으면 -1 */
	int32 FindEmitterIndex(const UParticleEmitter* InEmitter) const;
	bool RemoveEmitter(UParticleEmitter* InEmitter);
	bool RemoveEmitterAt(int32 Index);
	void ClearEmitters();

	/** 모든 이미터의 모듈/LOD 캐시를 갱신한다. */
	void UpdateAllModuleLists();

	int32 GetNumLODs() const { return LODDistances.Num(); }
	/** 모든 이미터에 LOD0 복제본을 하나씩 붙이고 새 LOD 인덱스를 돌려준다. 이미터가 없으면 -1 */
	int32 AddLODLevel(float Distance);

	void SetPath(const FString& InPath) { AssetPath = InPath; }
	const FString& GetPath() const { return AssetPath; }

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

	UPROPERTY(EditAnywhere)
	EParticleSystemLODMethod LODMethod = EParticleSystemLODMethod::Automatic;

	// LODDistances[i] = 카메라가 이 거리 이상이면 LOD i. [0]은 항상 0이고, 개수 = 모든 이미터의 LOD 개수
	TArray<float> LODDistances = { 0.0f };

protected:
	TArray<UParticleEmitter*> Emitters;
	FString AssetPath;
};
