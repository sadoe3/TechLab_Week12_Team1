#pragma once

#include "Math/Box.h"
#include "Particles/BaseParticle.h"

class UParticleEmitter;
class UParticleLODLevel;
class UParticleSystemComponent;

/**
 * UParticleEmitter 템플릿을 바탕으로 런타임에 파티클 풀을 관리·시뮬레이션하는 기반 인스턴스
 * (Unreal FParticleEmitterInstance 대응)
 */
struct FParticleEmitterInstance
{
public:
	FParticleEmitterInstance() = default;
	virtual ~FParticleEmitterInstance();

	virtual void InitParameters(UParticleEmitter* InTemplate, UParticleSystemComponent* InComponent);
	virtual void Init();
	virtual void Resize(int32 NewMaxActiveParticles, bool bSetMaxActiveCount = true);

	virtual void Tick(float DeltaTime, bool bSuppressSpawning);
	virtual float Spawn(float DeltaTime);
	virtual void SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity);
	virtual void KillParticles();
	virtual void KillParticlesForced(bool bFireEvents = false);
	virtual void UpdateBoundingBox(float DeltaTime);

	virtual bool IsMeshEmitter() const { return false; }

	UParticleLODLevel* GetCurrentLODLevel() const { return CurrentLODLevel; }
	FBaseParticle* GetParticleAtIndex(int32 ActiveIndex) const;

public:
	UParticleEmitter* SpriteTemplate = nullptr;
	UParticleSystemComponent* Component = nullptr;
	UParticleLODLevel* CurrentLODLevel = nullptr;
	int32 CurrentLODLevelIndex = 0;

	/** 파티클 데이터 버퍼 (MaxActiveParticles * ParticleStride 바이트) */
	uint8* ParticleData = nullptr;
	/** 활성/비활성 슬롯 인덱스 배열 */
	uint16* ParticleIndices = nullptr;

	int32 ParticleStride = 0;
	int32 ParticleSize = 0;
	int32 ActiveParticles = 0;
	int32 MaxActiveParticles = 0;

	float EmitterTime = 0.0f;
	float EmitterDuration = 1.0f;
	int32 LoopCount = 0;
	float SpawnFraction = 0.0f;

	bool bEnabled = true;
	bool bEmitterIsDone = false;

	FBox ParticleBoundingBox = { FVector::ZeroVector, FVector::ZeroVector };
};
