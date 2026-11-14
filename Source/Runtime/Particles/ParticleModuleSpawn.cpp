#include "EnginePCH.h"
#include "Particles/ParticleModuleSpawn.h"

UParticleModuleSpawn::UParticleModuleSpawn()
{
	// 파티클 하나를 건드리지 않고, Instance가 "이번 프레임에 몇 개"를 계산할 때 읽기만 한다
	bSpawnModule = false;
	bUpdateModule = false;
}
