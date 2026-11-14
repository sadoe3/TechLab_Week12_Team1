#include "EnginePCH.h"
#include "Particles/ParticleModuleRequired.h"
#include "Render/Material.h"

UParticleModuleRequired::UParticleModuleRequired()
{
	// 렌더러가 LODLevel->RequiredModule로 직접 읽으므로 Spawn/Update 목록에는 넣지 않는다
	bSpawnModule = false;
	bUpdateModule = false;
}
