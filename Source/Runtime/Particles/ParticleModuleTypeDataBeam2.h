#pragma once

#include "Particles/ParticleModuleTypeDataBase.h"
#include "ParticleModuleTypeDataBeam2.generated.h"

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
};
