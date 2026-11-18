#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "ParticleModule.generated.h"

class UParticleModuleTypeDataBase;
struct FParticleEmitterInstance;
struct FBaseParticle;

/** X axis of a module's curve: each particle's age, or the emitter's position in its loop */
enum class EParticleCurveInput : uint8
{
	RelativeTime, // particle age, 0 at spawn .. 1 at death
	EmitterTime,  // emitter loop fraction, 0 .. 1 of EmitterDuration (same unit as Burst Time)
};

/**
 * 모든 파티클 모듈의 기반 클래스 (Unreal Cascade UParticleModule 대응)
 */
UCLASS()
class UParticleModule : public UObject
{
	GENERATED_BODY()

public:
	UParticleModule() = default;
	virtual ~UParticleModule() override = default;

	/** 파티클 스폰 시 1회 호출 */
	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) {}

	/** 매 프레임 살아있는 파티클 업데이트 시 호출 (bUpdateModule). 모듈이 Owner의 활성 파티클 전체를 순회한다. */
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) {}

	/** 스폰·적분이 모두 끝난 뒤 호출 (bFinalUpdateModule) */
	virtual void FinalUpdate(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) {}

	/** 파티클 1개당 추가로 필요한 Payload 바이트 크기 */
	virtual uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) const { return 0; }

	/** 이미터 인스턴스당 추가로 필요한 Payload 바이트 크기 */
	virtual uint32 RequiredBytesPerInstance() const { return 0; }

	/**
	 * What a curve property's X axis means, for the Curve Editor (labels, range).
	 * The runtime meaning is whatever this module passes to Eval / GetDistributionValue; this must match it.
	 * Default: spawn-time distributions read at the emitter's loop fraction (UE reads Owner->EmitterTime).
	 */
	virtual EParticleCurveInput GetCurveInput(const FProperty& CurveProperty) const { return EParticleCurveInput::EmitterTime; }

	/**
	 * Reads a distribution for a spawning particle (UE: FRawDistribution::GetValue(Owner->EmitterTime, ..., RandomStream)).
	 * Uniform draws per component from the component's random stream; Curve is evaluated at the emitter's loop fraction (0..1).
	 */
	static float GetDistributionValue(FParticleEmitterInstance* Owner, const FDistributionFloat& Distribution);
	static FVector GetDistributionValue(FParticleEmitterInstance* Owner, const FDistributionVector& Distribution);

protected:
	// Min~Max 균등 분포. Min == Max면 상수.
	static float RandomRange(FParticleEmitterInstance* Owner, float Min, float Max);
	static FVector RandomRange(FParticleEmitterInstance* Owner, const FVector& Min, const FVector& Max);

	/** 이미터(컴포넌트) 공간의 방향 벡터를 시뮬레이션 공간으로 (Unreal Owner->EmitterToSimulation.TransformVector) */
	static FVector EmitterToSimulationVector(const FParticleEmitterInstance* Owner, const FVector& Vector);
	/** 월드 공간의 방향 벡터를 시뮬레이션 공간으로 (Unreal SimulationToWorld.InverseTransformVector) */
	static FVector WorldToSimulationVector(const FParticleEmitterInstance* Owner, const FVector& Vector);
	/** 시뮬레이션 공간에서 이미터 원점 */
	static FVector GetSimulationOrigin(const FParticleEmitterInstance* Owner);

public:
	UPROPERTY(EditAnywhere)
	bool bSpawnModule = false;

	UPROPERTY(EditAnywhere)
	bool bUpdateModule = false;

	UPROPERTY(EditAnywhere)
	bool bFinalUpdateModule = false;

	UPROPERTY(EditAnywhere)
	bool bEnabled = true;
};
