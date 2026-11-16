#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleSpawn.h"
#include "Particles/ParticleModuleTypeDataBase.h"
#include "ParticleLODLevel.generated.h"

class UParticleModule;

/**
 * 단일 LOD 단계의 파티클 모듈 스택 및 캐시 리스트 (Unreal UParticleLODLevel 대응)
 */
UCLASS()
class UParticleLODLevel : public UObject
{
	GENERATED_BODY()

public:
	UParticleLODLevel() = default;
	virtual ~UParticleLODLevel() override = default;

	/** Modules 배열을 순회하여 SpawnModules / UpdateModules 캐시 리스트를 갱신한다. */
	void UpdateModuleLists();

	void AddModule(UParticleModule* InModule);
	bool RemoveModule(UParticleModule* InModule);
	void ClearModules();

	const TArray<UParticleModule*>& GetModules() const { return Modules; }
	const TArray<UParticleModule*>& GetSpawnModules() const { return SpawnModules; }
	const TArray<UParticleModule*>& GetUpdateModules() const { return UpdateModules; }

public:
	UPROPERTY(EditAnywhere)
	int32 Level = 0;

	UPROPERTY(EditAnywhere)
	bool bEnabled = true;

	UPROPERTY(EditAnywhere)
	UParticleModuleRequired* RequiredModule = nullptr;

	UPROPERTY(EditAnywhere)
	UParticleModuleSpawn* SpawnModule = nullptr;

	UPROPERTY(EditAnywhere)
	UParticleModuleTypeDataBase* TypeDataModule = nullptr;

	UPROPERTY(EditAnywhere)
	int32 PeakActiveParticles = 0;

protected:
	TArray<UParticleModule*> Modules;
	TArray<UParticleModule*> SpawnModules;
	TArray<UParticleModule*> UpdateModules;
};
