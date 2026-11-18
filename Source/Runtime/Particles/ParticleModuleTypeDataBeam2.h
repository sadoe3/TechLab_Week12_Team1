#pragma once

#include "Particles/ParticleModuleTypeDataBase.h"
#include "ParticleModuleTypeDataBeam2.generated.h"

/**
 * 빔 끝점을 정하는 방식 (Unreal EBeam2Method 대응, 순서·이름 동일. Branch는 아직 없음)
 */
UENUM()
enum class EBeam2Method : uint8
{
	PEBM2_Distance = 0,	// PSC 앞(로컬 X) 방향으로 Distance만큼
	PEBM2_Target = 1,	// BeamTarget 모듈의 TargetLocation까지
};

/**
 * 빔 폭을 끝으로 갈수록 줄이는 방식 (Unreal EBeamTaperMethod 대응. Partial은 Speed가 없어 Full과 같아서 뺐다)
 */
UENUM()
enum class EBeamTaperMethod : uint8
{
	PEBTM_None = 0,
	PEBTM_Full = 1,		// Source에서 1배 → Target에서 TaperFactor배
};

/**
 * Source → Target 사이를 잇는 빔으로 렌더링하는 Beam Emitter용 TypeData 모듈
 */
UCLASS()
class UParticleModuleTypeDataBeam2 : public UParticleModuleTypeDataBase
{
	GENERATED_BODY()

public:
	UParticleModuleTypeDataBeam2() = default;
	virtual ~UParticleModuleTypeDataBeam2() override = default;

	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent) override;

public:
	/** Source~Target을 몇 구간으로 나눌지. 점 개수 = Segments + 1 */
	UPROPERTY(EditAnywhere)
	int32 Segments = 10;

	UPROPERTY(EditAnywhere)
	EBeam2Method BeamMethod = EBeam2Method::PEBM2_Target;

	/** BeamMethod가 Distance일 때 빔 길이 */
	UPROPERTY(EditAnywhere)
	float Distance = 5.0f;

	/** 텍스처가 한 번 반복되는 빔 길이. 0이면 점 하나마다 0.5씩 */
	UPROPERTY(EditAnywhere)
	float TextureTileDistance = 0.0f;

	UPROPERTY(EditAnywhere)
	EBeamTaperMethod TaperMethod = EBeamTaperMethod::PEBTM_None;

	/** Target 쪽 끝의 폭 배율 (Size.X 기준) */
	UPROPERTY(EditAnywhere)
	float TaperFactor = 1.0f;
};
