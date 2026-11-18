#include "EnginePCH.h"
#include "GameFramework/Actor/Emitter.h"
#include "Asset/AssetManager.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleModuleSpawn.h"
#include "Particles/ParticleModuleTypeDataMesh.h"
#include "Particles/ParticleModuleTypeDataRibbon.h"
#include "Particles/ParticleModuleLifetime.h"
#include "Particles/ParticleModuleVelocity.h"
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

void AEmitter::SetupTestRibbonParticleSystem()
{
	if (!ParticleSystemComponent)
	{
		return;
	}

	UParticleSystem* TestSystem = NewObject<UParticleSystem>();
	UParticleEmitter* Emitter = NewObject<UParticleEmitter>();
	Emitter->SetEmitterName("TestRibbonEmitter");

	UParticleLODLevel* LOD0 = NewObject<UParticleLODLevel>();

	UParticleModuleRequired* Required = NewObject<UParticleModuleRequired>();
	// 월드 공간이어야 PSC가 움직일 때 이미 생긴 점이 제자리에 남아 꼬리가 된다
	Required->bUseLocalSpace = false;
	Required->EmitterDuration = 1.0f;
	Required->EmitterLoops = 0;

	UParticleModuleSpawn* Spawn = NewObject<UParticleModuleSpawn>();
	Spawn->Rate = 120.0f;
	Spawn->RateScale = 1.0f;

	UParticleModuleLifetime* Lifetime = NewObject<UParticleModuleLifetime>();
	Lifetime->LifetimeMin = 1.0f;
	Lifetime->LifetimeMax = 1.0f;

	// PSC가 멈춰 있어도 점들이 위로 흩어져 띠 모양이 보이게 한다
	UParticleModuleVelocity* Velocity = NewObject<UParticleModuleVelocity>();
	Velocity->StartVelocityMin = FVector(0.0f, 0.0f, 2.0f);
	Velocity->StartVelocityMax = FVector(0.0f, 0.0f, 2.0f);

	UParticleModuleTypeDataRibbon* RibbonTypeData = NewObject<UParticleModuleTypeDataRibbon>();

	LOD0->RequiredModule = Required;
	LOD0->SpawnModule = Spawn;
	LOD0->TypeDataModule = RibbonTypeData;
	LOD0->AddModule(Lifetime);
	LOD0->AddModule(Velocity);

	Emitter->AddLODLevel(LOD0);
	TestSystem->AddEmitter(Emitter);

	ParticleSystemComponent->SetTemplate(TestSystem);
}
