#pragma once

#include "UObject/Object.h"
#include "ParticleLODLevel.generated.h"

class UParticleModule;
class UParticleModuleRequired;
class UParticleModuleSpawn;
class UParticleModuleTypeDataBase;

UCLASS()
class UParticleLODLevel : public UObject
{
	GENERATED_BODY()

public:
	UParticleLODLevel();

	// Modules의 플래그를 보고 SpawnModules/UpdateModules를 다시 채운다
	void UpdateModuleLists();

	int32 Level = 0;
	bool bEnabled = true;

	UParticleModuleRequired* RequiredModule = nullptr;
	UParticleModuleSpawn* SpawnModule = nullptr;
	UParticleModuleTypeDataBase* TypeDataModule = nullptr;
	TArray<UParticleModule*> Modules;

	// Modules를 미리 나눠 둔 캐시. 매 파티클마다 플래그를 검사하지 않으려고 둔다
	TArray<UParticleModule*> SpawnModules;
	TArray<UParticleModule*> UpdateModules;
};
