#include "EnginePCH.h"
#include "GameFramework/Actor/Emitter.h"
#include "Asset/AssetManager.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleSpawn.h"
#include "Particles/ParticleModuleTypeDataMesh.h"
#include "UObject/UObjectGlobals.h"

AEmitter::AEmitter()
{
	ParticleSystemComponent = CreateDefaultSubobject<UParticleSystemComponent>("ParticleSystemComponent");
	ParticleSystemComponent->SetupAttachment(GetRootComponent());
}

void AEmitter::BeginPlay()
{
	if (ParticleSystemComponent && !ParticleSystemComponent->GetTemplate())
	{
		SetupTestMeshParticleSystem();
	}

	Super::BeginPlay();
}

void AEmitter::SetupTestMeshParticleSystem()
{
	if (!ParticleSystemComponent)
	{
		return;
	}

	if (UParticleSystem* LoadedSystem = UAssetManager::LoadParticleSystem("Assets/Particles/TestMeshParticle.particle"))
	{
		ParticleSystemComponent->SetTemplate(LoadedSystem);
		return;
	}

	UParticleSystem* TestSystem = NewObject<UParticleSystem>();
	UParticleEmitter* Emitter = NewObject<UParticleEmitter>();
	Emitter->SetEmitterName("TestMeshEmitter");

	UParticleLODLevel* LOD0 = NewObject<UParticleLODLevel>();

	UParticleModuleRequired* Required = NewObject<UParticleModuleRequired>();
	Required->bUseLocalSpace = true;
	Required->EmitterDuration = 1.0f;
	Required->EmitterLoops = 0;

	UParticleModuleSpawn* Spawn = NewObject<UParticleModuleSpawn>();
	Spawn->Rate = 2.0f;
	Spawn->RateScale = 1.0f;

	UParticleModuleTypeDataMesh* MeshTypeData = NewObject<UParticleModuleTypeDataMesh>();
	MeshTypeData->SetMesh(UAssetManager::GetAssetByPath<UStaticMesh>("Arrow"));

	LOD0->RequiredModule = Required;
	LOD0->SpawnModule = Spawn;
	LOD0->TypeDataModule = MeshTypeData;

	Emitter->AddLODLevel(LOD0);
	TestSystem->AddEmitter(Emitter);

	ParticleSystemComponent->SetTemplate(TestSystem);
}

void AEmitter::SetupTestSpriteParticleSystem()
{
	if (!ParticleSystemComponent)
	{
		return;
	}

	if (UParticleSystem* LoadedSystem = UAssetManager::LoadParticleSystem("Assets/Particles/TestSpriteParticle.particle"))
	{
		ParticleSystemComponent->SetTemplate(LoadedSystem);
	}
}
