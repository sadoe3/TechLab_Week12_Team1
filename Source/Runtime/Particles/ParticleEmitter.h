#pragma once

#include "UObject/Object.h"
#include "ParticleEmitter.generated.h"

class UParticleLODLevel;
class UParticleModule;

UCLASS()
class UParticleEmitter : public UObject
{
	GENERATED_BODY()

public:
	UParticleEmitter();

	// Required/Spawn 모듈이 꽂힌 LODLevel 하나를 만들어 LODLevels에 추가하고 돌려준다
	UParticleLODLevel* CreateLODLevel();

	// 모듈 구성이 바뀔 때마다 부른다. LOD별 목록을 나눈 뒤 크기·오프셋을 다시 계산한다
	void UpdateModuleLists();

	// 모듈 구성만으로 정해지는 값이라 인스턴스마다가 아니라 에셋에서 한 번만 계산한다
	void CacheEmitterModuleInfo();

	FName EmitterName;
	TArray<UParticleLODLevel*> LODLevels;

	int32 ParticleSize = 0;
	TMap<UParticleModule*, int32> ModuleOffsetMap;
};
