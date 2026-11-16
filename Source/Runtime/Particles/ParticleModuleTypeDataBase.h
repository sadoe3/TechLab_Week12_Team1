#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleTypeDataBase.generated.h"

class UParticleEmitter;
class UParticleSystemComponent;

/**
 * 특수 이미터 타입(Mesh, Ribbon, Beam 등) 모듈의 기반 클래스 (Unreal UParticleModuleTypeDataBase 대응)
 */
UCLASS()
class UParticleModuleTypeDataBase : public UParticleModule
{
	GENERATED_BODY()

public:
	UParticleModuleTypeDataBase() = default;
	virtual ~UParticleModuleTypeDataBase() override = default;

	/** 해당 TypeData에 맞는 런타임 FParticleEmitterInstance를 생성하는 팩토리 메서드 */
	virtual FParticleEmitterInstance* CreateInstance(UParticleEmitter* InEmitterParent, UParticleSystemComponent* InComponent);
};
