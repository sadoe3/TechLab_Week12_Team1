#include "EnginePCH.h"
#include "Particles/ParticleModuleLifetime.h"

UParticleModuleLifetime::UParticleModuleLifetime()
{
	// 태어날 때 수명을 한 번 정하면 끝이고, 나이 먹이기와 소멸은 Instance가 맡는다
	bSpawnModule = true;
	bUpdateModule = false;
}
