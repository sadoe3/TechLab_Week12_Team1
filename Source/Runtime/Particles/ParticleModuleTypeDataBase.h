#pragma once

#include "Particles/ParticleModule.h"
#include "ParticleModuleTypeDataBase.generated.h"

// nullptr이면 Sprite Emitter. Mesh/Beam/Ribbon은 나중에 이 클래스를 상속한다.
UCLASS()
class UParticleModuleTypeDataBase : public UParticleModule
{
	GENERATED_BODY()
};
