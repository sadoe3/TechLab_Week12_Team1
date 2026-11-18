#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleSubUV.generated.h"

/**
 * SubUV 프레임을 고르는 방식 (Unreal EParticleSubUVInterpMethod 대응)
 * Unreal은 이 값을 RequiredModule->InterpolationMethod에 두지만, 이 엔진은 SubUV 모듈 하나에 모아 둔다.
 * Blend 계열(두 칸을 섞기)은 셰이더가 한 칸만 샘플하므로 아직 없다.
 */
UENUM()
enum class EParticleSubUVInterpMethod : uint8
{
	Linear = 0,     // 수명(RelativeTime 0~1) 동안 StartFrame→EndFrame을 고르게 넘긴다 (Cycles번 반복)
	Random = 1,     // 스폰 때 범위 안의 칸 하나를 골라 수명 내내 유지한다
	FrameRate = 2,  // 파티클 나이(초) 기준으로 초당 FramesPerSecond 칸씩, 범위를 반복한다 (Unreal SubUVMovie)
};

/** SubUV 모듈이 파티클마다 쓰는 Payload (Unreal FFullSubUVPayload 대응) */
struct FSubUVPayload
{
	/** 이번 프레임에 그릴 아틀라스 칸 번호 (정수 값) */
	float ImageIndex = 0.0f;
	/** 스폰 때 뽑은 범위 내 오프셋 (Random 방식의 칸, 또는 bRandomStartFrame의 시작 칸) */
	float RandomOffset = 0.0f;
};

/**
 * 스프라이트가 그릴 텍스처 아틀라스 칸을 정하는 Spawn+Update 모듈
 * (Unreal UParticleModuleSubUV / UParticleModuleSubUVMovie 대응, Cascade의 "Sub Image Index")
 *
 * 아틀라스 칸 수는 RequiredModule->SubImages_Horizontal × SubImages_Vertical이며,
 * 칸 번호는 왼쪽 위부터 가로로 0, 1, 2 ... 순서다.
 * Unreal은 SubImageIndex를 커브(FRawDistributionFloat)로 갖지만 이 엔진은 아직 Distribution이 없어
 * StartFrame→EndFrame 구간 + 재생 방식으로 대신한다.
 */
UCLASS()
class UParticleModuleSubUV : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleSubUV();
	virtual ~UParticleModuleSubUV() override = default;

	virtual void Spawn(FParticleEmitterInstance* Owner, int32 Offset, float SpawnTime, FBaseParticle* ParticleBase) override;
	virtual void Update(FParticleEmitterInstance* Owner, int32 Offset, float DeltaTime) override;
	virtual uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) const override { return sizeof(FSubUVPayload); }

	/** 파티클 Payload에서 이번 프레임 칸 번호를 읽는다. 렌더 스냅샷이 쓴다. */
	static float GetImageIndex(const FBaseParticle& Particle, int32 Offset);

private:
	void Apply(const FParticleEmitterInstance* Owner, FBaseParticle& Particle, FSubUVPayload& Payload) const;

public:
	UPROPERTY(EditAnywhere)
	EParticleSubUVInterpMethod InterpolationMethod = EParticleSubUVInterpMethod::Linear;

	/** 재생 구간 첫 칸 */
	UPROPERTY(EditAnywhere)
	int32 StartFrame = 0;

	/** 재생 구간 마지막 칸. -1이면 아틀라스의 마지막 칸. StartFrame보다 작으면 거꾸로 재생한다. */
	UPROPERTY(EditAnywhere)
	int32 EndFrame = -1;

	/** Linear: 수명 동안 구간을 몇 번 반복할지 */
	UPROPERTY(EditAnywhere)
	float Cycles = 1.0f;

	/** FrameRate: 초당 넘길 칸 수 */
	UPROPERTY(EditAnywhere)
	float FramesPerSecond = 30.0f;

	/** Linear/FrameRate: 파티클마다 구간 안의 임의 칸에서 시작한다 (모든 파티클이 같은 칸으로 깜빡이는 것 방지) */
	UPROPERTY(EditAnywhere)
	bool bRandomStartFrame = false;
};
