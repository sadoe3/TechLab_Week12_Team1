#pragma once

#include "Particles/ParticleModuleTypeDataBase.h"
#include "ParticleModuleTypeDataRibbon.generated.h"

/**
 * 리본 파티클마다 FBaseParticle 뒤에 붙는 Payload
 * Prev/Next는 ParticleData 안의 슬롯 번호다.
 */
struct FRibbonTypeDataPayload
{
	/** 머리 쪽 이웃 = 나보다 늦게 태어난 파티클 슬롯. INDEX_NONE이면 내가 머리 */
	int32 Prev;
	/** 꼬리 쪽 이웃 = 나보다 먼저 태어난 파티클 슬롯. INDEX_NONE이면 내가 꼬리 */
	int32 Next;
};

/**
 * 띠의 폭을 어느 방향으로 펼칠지 (Unreal ETrailsRenderAxisOption 대응, 순서·이름 동일)
 */
UENUM()
enum class ETrailsRenderAxisOption : uint8
{
	Trails_CameraUp = 0,	// 진행 방향 × 카메라 앞 방향 = 항상 카메라를 향함
	Trails_SourceUp = 1,	// 컴포넌트의 위(Z) 방향
	Trails_WorldUp = 2,		// 월드 위(Z) 방향
};

/**
 * 파티클을 생성 순서대로 이어 띠(트레일)로 렌더링하는 Ribbon Emitter용 TypeData 모듈
 * (Unreal UParticleModuleTypeDataRibbon 대응)
 */
UCLASS()
class UParticleModuleTypeDataRibbon : public UParticleModuleTypeDataBase
{
	GENERATED_BODY()

public:
	UParticleModuleTypeDataRibbon() = default;
	virtual ~UParticleModuleTypeDataRibbon() override = default;

	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent) override;

	virtual uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) const override { return sizeof(FRibbonTypeDataPayload); }

public:
	/** 띠에 쓰는 점의 최대 개수 (머리부터 셈). 0이면 제한 없음 */
	UPROPERTY(EditAnywhere)
	int32 MaxParticleInTrailCount = 0;

	/** 텍스처가 한 번 반복되는 띠 길이. 0이면 점 하나마다 0.5씩 */
	UPROPERTY(EditAnywhere)
	float TilingDistance = 0.0f;

	UPROPERTY(EditAnywhere)
	ETrailsRenderAxisOption RenderAxis = ETrailsRenderAxisOption::Trails_CameraUp;

	/** false면 머리 파티클과 현재 이미터 위치 사이도 이어 그린다 (UE 철자 그대로) */
	UPROPERTY(EditAnywhere)
	bool bClipSourceSegement = false;

	/** Deactivate 하면 남은 띠를 수명을 기다리지 않고 바로 지운다 */
	UPROPERTY(EditAnywhere)
	bool bDeadTrailsOnDeactivate = false;
};
