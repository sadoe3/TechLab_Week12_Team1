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
#include "Particles/ParticleModuleTypeDataBeam2.h"
#include "Particles/ParticleModuleBeamTarget.h"
#include "Particles/ParticleModuleSize.h"
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

	UParticleModuleTypeDataRibbon* RibbonTypeData = NewObject<UParticleModuleTypeDataRibbon>();

	LOD0->RequiredModule = Required;
	LOD0->SpawnModule = Spawn;
	LOD0->TypeDataModule = RibbonTypeData;
	LOD0->AddModule(Lifetime);

	Emitter->AddLODLevel(LOD0);
	TestSystem->AddEmitter(Emitter);

	ParticleSystemComponent->SetTemplate(TestSystem);
}

void AEmitter::SetupTestBeamParticleSystem()
{
	if (!ParticleSystemComponent)
	{
		return;
	}

	UParticleSystem* TestSystem = NewObject<UParticleSystem>();
	UParticleEmitter* Emitter = NewObject<UParticleEmitter>();
	Emitter->SetEmitterName("TestBeamEmitter");

	UParticleLODLevel* LOD0 = NewObject<UParticleLODLevel>();

	UParticleModuleRequired* Required = NewObject<UParticleModuleRequired>();
	Required->bUseLocalSpace = true;
	Required->EmitterDuration = 1.0f;
	// 무한 루프면 루프마다 Burst가 다시 터져 죽지 않는 빔 파티클이 쌓인다
	Required->EmitterLoops = 1;
	Required->Material = UAssetManager::GetAssetByPath<UMaterial>("RibbonMaterial");

	// 빔 파티클 1개만 처음에 만든다
	UParticleModuleSpawn* Spawn = NewObject<UParticleModuleSpawn>();
	Spawn->Rate = 0.0f;
	Spawn->bProcessSpawnRate = false;
	FParticleBurst Burst;
	Burst.Count = 1;
	Burst.Time = 0.0f;
	Spawn->AddBurst(Burst);

	// 수명 0 = OneOverMaxLifetime 0 = 죽지 않음
	UParticleModuleLifetime* Lifetime = NewObject<UParticleModuleLifetime>();
	Lifetime->LifetimeMin = 0.0f;
	Lifetime->LifetimeMax = 0.0f;

	// Size.X가 빔 폭이 된다
	UParticleModuleSize* Size = NewObject<UParticleModuleSize>();
	Size->StartSizeMin = FVector(0.3f, 0.3f, 0.3f);
	Size->StartSizeMax = FVector(0.3f, 0.3f, 0.3f);

	UParticleModuleTypeDataBeam2* BeamTypeData = NewObject<UParticleModuleTypeDataBeam2>();
	BeamTypeData->Segments = 10;

	UParticleModuleBeamTarget* BeamTarget = NewObject<UParticleModuleBeamTarget>();
	BeamTarget->TargetLocation = FVector(0.0f, 0.0f, 5.0f);

	LOD0->RequiredModule = Required;
	LOD0->SpawnModule = Spawn;
	LOD0->TypeDataModule = BeamTypeData;
	LOD0->AddModule(Lifetime);
	LOD0->AddModule(Size);
	LOD0->AddModule(BeamTarget);

	Emitter->AddLODLevel(LOD0);
	TestSystem->AddEmitter(Emitter);

	ParticleSystemComponent->SetTemplate(TestSystem);
}
