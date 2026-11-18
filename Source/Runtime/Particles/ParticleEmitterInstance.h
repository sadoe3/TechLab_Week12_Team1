#pragma once

#include "Math/Box.h"
#include "Particles/BaseParticle.h"

class UParticleEmitter;
class UParticleLODLevel;
class UParticleSystemComponent;
struct FDynamicEmitterDataBase;

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
	/** EmitterTime/루프/딜레이 진행 (Unreal Tick_EmitterTimeSetup) */
	virtual void Tick_EmitterTimeSetup(float DeltaTime);
	/** Velocity·Size·RotationRate를 Base 값으로 되돌리고 RelativeTime을 진행한다 (Unreal ResetParticleParameters) */
	virtual void ResetParticleParameters(float DeltaTime);
	virtual void Tick_ModuleUpdate(float DeltaTime);
	virtual void Tick_ModuleFinalUpdate(float DeltaTime);
	virtual float Spawn(float DeltaTime);
	/** 이번 프레임에 터질 BurstList 파티클 수 (Unreal GetCurrentBurstRateOffset). OutSpawnTime = 버스트 후 경과 시간 */
	int32 GetCurrentBurstCount(float DeltaTime, float& OutSpawnTime);
	/** 이미터 시간을 처음으로 되돌린다 (재활성화 시) */
	virtual void Rewind();
	/** 모든 루프를 끝냈고 살아있는 파티클도 없는가 */
	bool HasCompleted() const;
	/** RequiredModule->bKillOnDeactivate */
	virtual bool ShouldKillOnDeactivate() const;
	virtual void SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity);
	virtual void KillParticles();
	virtual void KillParticlesForced(bool bFireEvents = false);
	virtual void UpdateBoundingBox(float DeltaTime);

	/** 렌더 스레드/SceneProxy에 전달할 동적 렌더 데이터 스냅샷을 생성한다. */
	virtual TUniquePtr<FDynamicEmitterDataBase> GetDynamicData(bool bSelected);

	/** 파티클 위치가 컴포넌트 로컬 공간인지 (RequiredModule->bUseLocalSpace). false면 월드 공간. */
	bool IsLocalSpace() const;

	/** How far the current loop is, 0..1 of EmitterDuration, after the loop's delay. X axis of EmitterTime curves (same unit as Burst Time). */
	float GetEmitterTimeFraction() const
	{
		const float Duration = EmitterDuration > 0.0f ? EmitterDuration : 1.0f;
		const float Fraction = (EmitterTime - CurrentDelay) / Duration;
		return Fraction < 0.0f ? 0.0f : (Fraction > 1.0f ? 1.0f : Fraction);
	}

	/** 바운딩 박스 계산 시 파티클 중심에서 더할 반경. 타입별(Sprite/Mesh)로 실제 그려지는 크기를 반영한다. */
	virtual float GetParticleExtent(const FBaseParticle& Particle) const;

	UParticleLODLevel* GetCurrentLODLevel() const { return CurrentLODLevel; }
	/** 사용할 LOD를 바꾼다. 살아 있는 파티클은 그대로 두고 다음 Spawn/Update부터 새 LOD 모듈을 쓴다 */
	virtual void SetCurrentLODIndex(int32 InLODIndex);
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

	/** 모듈 Payload가 시작되는 위치 (= sizeof(FBaseParticle)) */
	int32 PayloadOffset = 0;
	/** TypeData Payload 시작 위치 (UParticleEmitter::TypeDataOffset 복사본). 0이면 없음 */
	int32 TypeDataOffset = 0;
	int32 ParticleStride = 0;
	int32 ParticleSize = 0;
	int32 ActiveParticles = 0;
	int32 MaxActiveParticles = 0;

	float EmitterTime = 0.0f;
	float EmitterDuration = 1.0f;
	/** 현재 루프의 EmitterDelay. EmitterTime이 이 값보다 작으면 스폰하지 않는다. */
	float CurrentDelay = 0.0f;
	/** SpawnModule->BurstList 항목별로 이번 루프에서 이미 터졌는지 (Unreal FLODBurstFired) */
	/** (TArray<bool>은 std::vector<bool> 특수화라 참조를 돌려줄 수 없어 uint8로 둔다) */
	TArray<uint8> BurstFired;
	int32 LoopCount = 0;
	float SpawnFraction = 0.0f;

	bool bEnabled = true;
	bool bEmitterIsDone = false;

	/** 파티클 시뮬레이션 공간(IsLocalSpace()에 따라 로컬 또는 월드) 기준 바운딩 박스 */
	FBox ParticleBoundingBox = { FVector::ZeroVector, FVector::ZeroVector };
};
