#include "EnginePCH.h"
#include "Components/ParticleSystemComponent.h"
#include "Engine/ParticleSystemSceneProxy.h"
#include "Engine/Scene.h"
#include "Particles/ParticleDynamicData.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleTypeDataBase.h"
#include "Particles/ParticleSpriteEmitterInstance.h"
#include <algorithm>
#include <cfloat>

UParticleSystemComponent::UParticleSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Unreal 기본값과 동일. 켜 두면 바운드가 매 프레임 바뀔 때마다 그림자 캐시가 무효화된다.
	bCastShadow = false;
}

UParticleSystemComponent::~UParticleSystemComponent()
{
	ResetParticles(true);
}

void UParticleSystemComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Template && EmitterInstances.IsEmpty())
	{
		InitParticles();
	}

	if (bAutoActivate)
	{
		ActivateSystem();
	}
}

void UParticleSystemComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);

	if (!bIsActive || !Template)
	{
		return;
	}

	if (EmitterInstances.IsEmpty())
	{
		InitParticles();
	}

	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance && Instance->bEnabled)
		{
			Instance->Tick(DeltaTime, bSuppressSpawning);
		}
	}

	if (bWasDeactivated && !HasAnyActiveParticles())
	{
		bIsActive = false;
		bWasDeactivated = false;
	}

	// 모든 이미터가 루프를 다 돌고 파티클도 다 사라졌으면 시스템이 끝난 것 (Unreal: 완료 시 자동 비활성화)
	bool bAllCompleted = !EmitterInstances.IsEmpty();
	for (const FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance && Instance->bEnabled && !Instance->HasCompleted())
		{
			bAllCompleted = false;
			break;
		}
	}
	if (bAllCompleted)
	{
		bIsActive = false;
	}

	SendRenderDynamicData();
	UpdateRenderBounds();
}

FBox UParticleSystemComponent::CalcLocalBounds() const
{
	if (Template && Template->bUseFixedRelativeBoundingBox)
	{
		return Template->FixedRelativeBoundingBox;
	}

	if (EmitterInstances.IsEmpty())
	{
		return Super::CalcLocalBounds();
	}

	FBox Result = { FVector(FLT_MAX, FLT_MAX, FLT_MAX), FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX) };
	bool bHasAny = false;
	bool bHasWorldToLocal = false;
	FMatrix WorldToLocal;

	for (const FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (!Instance || Instance->ActiveParticles <= 0)
		{
			continue;
		}
		bHasAny = true;

		if (Instance->IsLocalSpace())
		{
			Result.Expand(Instance->ParticleBoundingBox);
		}
		else
		{
			// 월드 공간 이미터의 박스는 월드 좌표이므로, 프록시가 LocalToWorld를 곱하기 전에 로컬로 되돌려 둔다.
			if (!bHasWorldToLocal)
			{
				WorldToLocal = GetWorldMatrix().Inverse();
				bHasWorldToLocal = true;
			}
			Result.Expand(Instance->ParticleBoundingBox.GetWorldAABB(WorldToLocal));
		}
	}

	if (!bHasAny)
	{
		return Super::CalcLocalBounds();
	}

	return Result;
}

void UParticleSystemComponent::UpdateRenderBounds()
{
	// 프록시의 컬링 바운드(BVH, GPU 오클루전)는 Scene::MarkDirty → Proxy::UpdateTransform에서만 다시 계산된다.
	// 파티클이 움직여 박스가 바뀌었으면 알려 줘야 처음 만든 점 크기 박스로 계속 컬링되지 않는다.
	// (Unreal: UParticleSystemComponent가 바운드 변경 시 UpdateComponentToWorld/MarkRenderTransformDirty 호출)
	const FBox NewBounds = CalcLocalBounds();
	const auto SameVector = [](const FVector& A, const FVector& B)
	{
		return A.X == B.X && A.Y == B.Y && A.Z == B.Z;
	};
	if (SameVector(NewBounds.Min, CachedLocalBounds.Min) && SameVector(NewBounds.Max, CachedLocalBounds.Max))
	{
		// 바운드가 그대로여도 파티클은 매 프레임 움직이므로 캐시된 그림자 타일을 무효화해야 한다
		// (USkinnedMeshComponent::SendRenderDynamicData와 같은 방식)
		if (FPrimitiveSceneProxy* Proxy = GetSceneProxy())
		{
			if (Proxy->GetScene() && Proxy->bCastsShadow)
			{
				Proxy->GetScene()->InvalidateShadowsInBounds(Proxy->GetBounds());
			}
		}
		return;
	}
	CachedLocalBounds = NewBounds;
	OnTransformDirty();
}

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	FParticleSystemSceneProxy* Proxy = new FParticleSystemSceneProxy(this);
	Proxy->UpdateData(CreateDynamicData());
	return Proxy;
}

void UParticleSystemComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);

	if (Property.Name == "Template")
	{
		InitParticles();
		MarkRenderStateDirty();
	}
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* NewTemplate)
{
	if (Template == NewTemplate)
	{
		return;
	}

	ResetParticles(true);
	Template = NewTemplate;
	InitParticles();
	MarkRenderStateDirty();
}

void UParticleSystemComponent::ActivateSystem(bool bFlagAsJustAttached)
{
	if (Template && EmitterInstances.IsEmpty())
	{
		InitParticles();
	}
	else
	{
		// 이미 돌았던 이미터(루프 종료, 비활성화)를 처음부터 다시 돌린다. 살아있는 파티클은 그대로 둔다.
		for (FParticleEmitterInstance* Instance : EmitterInstances)
		{
			if (Instance)
			{
				Instance->Rewind();
			}
		}
	}
	bSuppressSpawning = false;
	bWasDeactivated = false;
	bIsActive = true;
}

void UParticleSystemComponent::DeactivateSystem()
{
	bSuppressSpawning = true;
	bWasDeactivated = true;

	// bKillOnDeactivate인 이미터는 남은 파티클을 즉시 지운다. 나머지는 수명이 다할 때까지 남는다.
	bool bKilledAny = false;
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance && Instance->ShouldKillOnDeactivate() && Instance->ActiveParticles > 0)
		{
			Instance->KillParticlesForced();
			bKilledAny = true;
		}
	}

	if (!HasAnyActiveParticles())
	{
		bIsActive = false;
		bWasDeactivated = false;
	}

	if (bKilledAny)
	{
		// Tick이 멈추면 프록시가 마지막 스냅샷을 계속 그리므로 비워 둔다
		SendRenderDynamicData();
	}
}

void UParticleSystemComponent::InitParticles()
{
	ResetParticles(true);

	if (!Template)
	{
		return;
	}

	Template->UpdateAllModuleLists();

	const TArray<UParticleEmitter*>& Emitters = Template->GetEmitters();
	bool bAnySoloing = false;
	for (const UParticleEmitter* Emitter : Emitters)
	{
		if (Emitter && Emitter->bIsSoloing)
		{
			bAnySoloing = true;
			break;
		}
	}

	for (UParticleEmitter* Emitter : Emitters)
	{
		if (!Emitter || Emitter->IsDisabled())
		{
			continue;
		}
		if (bAnySoloing && !Emitter->bIsSoloing)
		{
			continue;
		}

		UParticleLODLevel* LOD0 = Emitter->GetLODLevel(0);
		if (!LOD0 || !LOD0->bEnabled)
		{
			continue;
		}

		FParticleEmitterInstance* NewInstance = nullptr;
		if (LOD0->TypeDataModule)
		{
			NewInstance = LOD0->TypeDataModule->CreateInstance(Emitter, this);
		}
		else
		{
			// 언리얼 기본 규칙: TypeDataModule == nullptr이면 Sprite 이미터로 생성
			NewInstance = new FParticleSpriteEmitterInstance(Emitter, this, nullptr);
		}

		if (NewInstance)
		{
			NewInstance->InitParameters(Emitter, this);
			NewInstance->Init();
			EmitterInstances.Add(NewInstance);
		}
	}

	SendRenderDynamicData();
}

void UParticleSystemComponent::ResetParticles(bool bEmptyInstances)
{
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (!Instance)
		{
			continue;
		}
		Instance->KillParticlesForced(false);
		if (bEmptyInstances)
		{
			delete Instance;
		}
	}

	if (bEmptyInstances)
	{
		EmitterInstances.Empty();
	}

	SendRenderDynamicData();
}

bool UParticleSystemComponent::HasAnyActiveParticles() const
{
	for (const FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance && Instance->bEnabled && Instance->ActiveParticles > 0)
		{
			return true;
		}
	}
	return false;
}

TUniquePtr<FParticleDynamicData> UParticleSystemComponent::CreateDynamicData()
{
	auto DynamicData = MakeUnique<FParticleDynamicData>();

	for (int32 Index = 0; Index < EmitterInstances.Num(); ++Index)
	{
		FParticleEmitterInstance* Instance = EmitterInstances[Index];
		if (!Instance || !Instance->bEnabled || Instance->ActiveParticles <= 0)
		{
			continue;
		}

		TUniquePtr<FDynamicEmitterDataBase> EmitterData = Instance->GetDynamicData(false);
		if (EmitterData)
		{
			EmitterData->EmitterIndex = Index;
			DynamicData->DynamicEmitterDataArray.Add(std::move(EmitterData));
		}
	}

	return DynamicData;
}

void UParticleSystemComponent::SendRenderDynamicData()
{
	if (FParticleSystemSceneProxy* ParticleProxy = static_cast<FParticleSystemSceneProxy*>(GetSceneProxy()))
	{
		ParticleProxy->UpdateData(CreateDynamicData());
	}
}
