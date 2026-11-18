#include "EnginePCH.h"
#include "Editor/ParticleSystemEditor/ParticleSystemEditor.h"
#include "Editor/ParticleSystemEditor/ParticleSystemEditorTab.h"
#include "Editor/HitoriEd/EditorEngine.h"
#include "Launch/LaunchEngineLoop.h"
#include "Engine/World.h"
#include "Math/BoxSphereBounds.h"
#include "Asset/AssetUtils.h"
#include "Core/Misc/Paths.h"
#include "Components/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemFile.h"
#include "Particles/ParticleEmitterInstance.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleLifetime.h"
#include "Particles/ParticleModuleSize.h"
#include "Particles/ParticleModuleVelocity.h"
#include "Particles/ParticleModuleColorOverLife.h"
#include "Particles/ParticleModuleTypeDataRibbon.h"
#include "Particles/ParticleModuleTypeDataBeam2.h"
#include "Asset/AssetManager.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"

#include <algorithm>

namespace
{
	// 시스템이 끝난 뒤 다시 시작하기까지 기다리는 시간 (Cascade 프리뷰처럼 잠깐 비었다가 반복한다)
	constexpr float AutoRestartDelay = 0.5f;

	// 파티클이 아직 없을 때 카메라를 맞출 기본 반지름. 이 엔진의 단위는 m 규모라 파티클이 1 안팎이다.
	// (Unreal의 cm 기준 100을 쓰면 카메라가 너무 멀어 파티클이 한 픽셀보다 작아진다)
	constexpr float DefaultFocusRadius = 3.0f;
	constexpr float MinFocusRadius = 0.5f;

	// 처음 열 때 파티클이 스폰되기를 기다렸다가 맞춘다. 이 시간이 지나도 없으면 기본 반지름으로 맞춘다.
	constexpr float InitialFocusTimeout = 1.0f;

	// Keep Required's material compatible with the emitter type: the Ribbon PSO uses a different vertex
	// layout from sprites, so switching TypeData to/from Ribbon swaps between the default materials.
	void SyncRequiredMaterialWithTypeData(UParticleLODLevel* LOD)
	{
		if (!LOD || !LOD->RequiredModule)
		{
			return;
		}

		UMaterial*& Material = LOD->RequiredModule->Material;
		// Beam도 Ribbon 정점 레이아웃(PSO)을 쓰므로 Ribbon 머티리얼이 필요하다
		const bool bRibbonEmitter = Cast<UParticleModuleTypeDataRibbon>(LOD->TypeDataModule) != nullptr
			|| Cast<UParticleModuleTypeDataBeam2>(LOD->TypeDataModule) != nullptr;
		const bool bRibbonMaterial = Material && Material->PSOType == EPSOType::Particle_Ribbon;

		if (bRibbonEmitter && !bRibbonMaterial)
		{
			Material = UAssetManager::GetAssetByPath<UMaterial>("RibbonMaterial");
		}
		else if (!bRibbonEmitter && bRibbonMaterial)
		{
			Material = UAssetManager::GetAssetByPath<UMaterial>("SubUVMaterial");
		}
	}
}

FParticleSystemEditor::FParticleSystemEditor(
	UEditorEngine& InEditorEngine,
	FEditorManager& InEditorManager,
	const FString& InAssetPath,
	UParticleSystem& InParticleSystem
)
	: FAssetEditor(InEditorEngine, InEditorManager, InAssetPath)
	, ParticleSystem(&InParticleSystem)
	, SelectedObject(&InParticleSystem)
{
}

bool FParticleSystemEditor::InitializeResources()
{
	if (!ParticleSystem)
	{
		return false;
	}

	// 부모 InitializeResources 호출 (FPreviewScene 초기화 및 세팅)
	if (!FAssetEditor::InitializeResources())
	{
		return false;
	}

	PreviewComponent = Cast<UParticleSystemComponent>(PreviewScene.AddComponent(UParticleSystemComponent::StaticClass()));
	if (!PreviewComponent)
	{
		return false;
	}

	// 미리보기 월드는 BeginPlay를 돌리지 않으므로 여기서 직접 활성화한다.
	PreviewComponent->SetTemplate(ParticleSystem);
	PreviewComponent->ActivateSystem();

	FocusParticles();
	return true;
}

TUniquePtr<IEditorTab> FParticleSystemEditor::CreateTab()
{
	return MakeUnique<FParticleSystemEditorTab>(*this);
}

void FParticleSystemEditor::ShutdownResources()
{
	SelectedObject = nullptr;
	PreviewComponent = nullptr;     // 컴포넌트는 미리보기 월드가 정리한다
	ParticleSystem = nullptr;       // 에셋은 AssetManager 소유
	FAssetEditor::ShutdownResources();
}

void FParticleSystemEditor::RestartSimulation()
{
	if (!PreviewComponent)
	{
		return;
	}

	// 모듈 구성이 바뀌었을 수 있으므로 인스턴스를 새로 만든 뒤 활성화한다.
	PreviewComponent->InitParticles();
	PreviewComponent->ActivateSystem();
	AutoRestartTimer = -1.0f;
	bRestartPending = false;
}

void FParticleSystemEditor::SetEditLODIndex(int32 InIndex)
{
	EditLODIndex = ParticleSystem ? std::clamp(InIndex, -1, ParticleSystem->GetNumLODs() - 1) : -1;
	if (!PreviewComponent)
	{
		return;
	}
	// Auto가 아니면 고른 LOD로 고정해야 편집 중인 LOD의 모습을 거리와 상관없이 바로 볼 수 있다
	PreviewComponent->bAutoLOD = EditLODIndex < 0;
	PreviewComponent->SetLODLevel(EditLODIndex < 0 ? 0 : EditLODIndex);
}

void FParticleSystemEditor::AddLOD()
{
	if (!ParticleSystem)
	{
		return;
	}

	const TArray<float>& Distances = ParticleSystem->LODDistances;
	const float LastDistance = Distances.IsEmpty() ? 0.0f : Distances[Distances.Num() - 1];
	const int32 NewIndex = ParticleSystem->AddLODLevel(LastDistance + 1000.0f);
	if (NewIndex < 0)
	{
		return;
	}
	NotifyStructureChanged();
	SetEditLODIndex(NewIndex);
}

void FParticleSystemEditor::NotifyPropertyEdited()
{
	bDirty = true;
	bRestartPending = true;
}

////////////////////////////////////////////////////////////
// 구조 편집
////////////////////////////////////////////////////////////

bool FParticleSystemEditor::IsFixedModule(const UParticleLODLevel* LOD, const UObject* Module)
{
	if (!Module)
	{
		return false;
	}
	if (LOD && (Module == LOD->RequiredModule || Module == LOD->SpawnModule || Module == LOD->TypeDataModule))
	{
		return true;
	}
	const UClass* Class = Module->GetClass();
	return Class->IsChildOf(UParticleModuleRequired::StaticClass())
		|| Class->IsChildOf(UParticleModuleSpawn::StaticClass())
		|| Class->IsChildOf(UParticleModuleTypeDataBase::StaticClass());
}

void FParticleSystemEditor::NotifyStructureChanged()
{
	bDirty = true;
	ValidateSelection();

	if (!ParticleSystem)
	{
		return;
	}

	// 같은 에셋을 레벨에 놓은 컴포넌트도 예전 오프셋으로 파티클 메모리를 읽게 되므로 함께 다시 만든다.
	// (InitParticles가 UpdateAllModuleLists로 모듈 캐시와 오프셋도 다시 계산한다)
	TArray<UObject*> Components;
	GetObjectsOfClass(UParticleSystemComponent::StaticClass(), Components);
	for (UObject* Object : Components)
	{
		UParticleSystemComponent* Component = static_cast<UParticleSystemComponent*>(Object);
		if (!Component || Component == PreviewComponent || Component->GetTemplate() != ParticleSystem)
		{
			continue;
		}

		const bool bWasActive = Component->IsActive();
		Component->InitParticles();
		if (bWasActive)
		{
			Component->ActivateSystem();
		}
		Component->MarkRenderStateDirty();
	}

	if (PreviewComponent)
	{
		RestartSimulation();
		PreviewComponent->MarkRenderStateDirty();
	}
	else
	{
		ParticleSystem->UpdateAllModuleLists();
	}
}

void FParticleSystemEditor::ValidateSelection()
{
	if (!ParticleSystem || !SelectedObject || SelectedObject == ParticleSystem)
	{
		return;
	}

	for (UParticleEmitter* Emitter : ParticleSystem->GetEmitters())
	{
		if (!Emitter)
		{
			continue;
		}
		if (SelectedObject == Emitter)
		{
			return;
		}
		for (UParticleLODLevel* LOD : Emitter->GetLODLevels())
		{
			if (!LOD)
			{
				continue;
			}
			if (SelectedObject == LOD->RequiredModule || SelectedObject == LOD->SpawnModule || SelectedObject == LOD->TypeDataModule)
			{
				return;
			}
			for (UParticleModule* Module : LOD->GetModules())
			{
				if (SelectedObject == Module)
				{
					return;
				}
			}
		}
	}

	SelectedObject = ParticleSystem;
}

UParticleModule* FParticleSystemEditor::AddModule(UParticleEmitter* Emitter, UClass* ModuleClass)
{
	if (!Emitter || !ModuleClass || !ModuleClass->Constructor || !ModuleClass->IsChildOf(UParticleModule::StaticClass()))
	{
		return nullptr;
	}

	// Required/Spawn은 이미터마다 하나뿐이고 따로 추가하지 않는다.
	if (ModuleClass->IsChildOf(UParticleModuleRequired::StaticClass()) || ModuleClass->IsChildOf(UParticleModuleSpawn::StaticClass()))
	{
		return nullptr;
	}

	UParticleModule* Module = NewObject<UParticleModule>(nullptr, ModuleClass);
	if (!Module)
	{
		return nullptr;
	}

	const bool bTypeData = ModuleClass->IsChildOf(UParticleModuleTypeDataBase::StaticClass());
	for (UParticleLODLevel* LOD : Emitter->GetLODLevels())
	{
		if (!LOD)
		{
			continue;
		}
		if (bTypeData)
		{
			LOD->TypeDataModule = static_cast<UParticleModuleTypeDataBase*>(Module);
			SyncRequiredMaterialWithTypeData(LOD);
		}
		else
		{
			LOD->AddModule(Module);
		}
	}

	SelectedObject = Module;
	NotifyStructureChanged();
	return Module;
}

void FParticleSystemEditor::DeleteModule(UParticleEmitter* Emitter, UParticleModule* Module)
{
	UParticleLODLevel* LOD0 = Emitter ? Emitter->GetLODLevel(0) : nullptr;
	if (!LOD0 || !Module || Module == LOD0->RequiredModule || Module == LOD0->SpawnModule)
	{
		return;
	}

	if (Module == LOD0->TypeDataModule)
	{
		for (UParticleLODLevel* LOD : Emitter->GetLODLevels())
		{
			if (LOD)
			{
				LOD->TypeDataModule = nullptr;
				SyncRequiredMaterialWithTypeData(LOD);
			}
		}
	}
	else
	{
		const int32 Index = LOD0->FindModuleIndex(Module);
		if (Index < 0)
		{
			return;
		}
		for (UParticleLODLevel* LOD : Emitter->GetLODLevels())
		{
			if (LOD)
			{
				LOD->RemoveModuleAt(Index);
			}
		}
	}

	NotifyStructureChanged();
}

void FParticleSystemEditor::MoveModule(UParticleEmitter* SourceEmitter, UParticleModule* Module, UParticleEmitter* TargetEmitter, int32 TargetIndex)
{
	UParticleLODLevel* SourceLOD0 = SourceEmitter ? SourceEmitter->GetLODLevel(0) : nullptr;
	if (!SourceLOD0 || !TargetEmitter || !TargetEmitter->GetLODLevel(0) || IsFixedModule(SourceLOD0, Module))
	{
		return;
	}

	const int32 SourceIndex = SourceLOD0->FindModuleIndex(Module);
	if (SourceIndex < 0)
	{
		return;
	}

	if (SourceEmitter == TargetEmitter)
	{
		const int32 Count = SourceLOD0->GetModules().Num();
		const int32 FinalIndex = std::clamp(TargetIndex, 0, Count - 1);
		if (FinalIndex == SourceIndex)
		{
			return;
		}

		for (UParticleLODLevel* LOD : SourceEmitter->GetLODLevels())
		{
			if (LOD)
			{
				LOD->InsertModule(LOD->RemoveModuleAt(SourceIndex), FinalIndex);
			}
		}
	}
	else
	{
		for (UParticleLODLevel* LOD : SourceEmitter->GetLODLevels())
		{
			if (LOD)
			{
				LOD->RemoveModuleAt(SourceIndex);
			}
		}
		for (UParticleLODLevel* LOD : TargetEmitter->GetLODLevels())
		{
			if (LOD)
			{
				LOD->InsertModule(Module, std::clamp(TargetIndex, 0, LOD->GetModules().Num()));
			}
		}
	}

	NotifyStructureChanged();
}

UParticleEmitter* FParticleSystemEditor::AddSpriteEmitter()
{
	if (!ParticleSystem)
	{
		return nullptr;
	}

	UParticleEmitter* Emitter = CreateDefaultSpriteEmitter();
	// 모든 이미터의 LOD 개수가 시스템 LOD 개수와 같아야 LOD 인덱스 하나로 함께 바꿀 수 있다
	for (int32 LODIndex = Emitter->GetNumLODLevels(); LODIndex < ParticleSystem->GetNumLODs(); ++LODIndex)
	{
		UParticleLODLevel* NewLOD = ParticleSystemFile::DuplicateLODLevel(*Emitter, 0);
		NewLOD->Level = LODIndex;
		Emitter->AddLODLevel(NewLOD);
	}
	ParticleSystem->AddEmitter(Emitter);
	SelectedObject = Emitter;
	NotifyStructureChanged();
	return Emitter;
}

UParticleEmitter* FParticleSystemEditor::CreateDefaultSpriteEmitter()
{
	// Cascade의 기본 Sprite 이미터: Required, Spawn, Lifetime, Initial Size, Initial Velocity, Color Over Life.
	// 값은 Unreal 기본값(cm)을 이 엔진의 m 규모로 옮긴 것이다.
	UParticleLODLevel* LOD = NewObject<UParticleLODLevel>();
	LOD->Level = 0;

	UParticleModuleRequired* Required = NewObject<UParticleModuleRequired>();
	Required->Material = UAssetManager::GetAssetByPath<UMaterial>("SubUVMaterial");
	Required->SubImages_Horizontal = 8;    // 기본 텍스처(StarParticle)가 8x8 아틀라스다
	Required->SubImages_Vertical = 8;
	LOD->RequiredModule = Required;

	UParticleModuleSpawn* Spawn = NewObject<UParticleModuleSpawn>();
	Spawn->Rate = 20.0f;
	LOD->SpawnModule = Spawn;

	UParticleModuleLifetime* Lifetime = NewObject<UParticleModuleLifetime>();
	Lifetime->LifetimeMin = 1.0f;
	Lifetime->LifetimeMax = 1.0f;
	LOD->AddModule(Lifetime);

	UParticleModuleSize* Size = NewObject<UParticleModuleSize>();
	Size->StartSizeMin = FVector(0.25f, 0.25f, 0.25f);
	Size->StartSizeMax = FVector(0.25f, 0.25f, 0.25f);
	LOD->AddModule(Size);

	UParticleModuleVelocity* Velocity = NewObject<UParticleModuleVelocity>();
	Velocity->StartVelocityMin = FVector(-0.1f, -0.1f, 0.0f);
	Velocity->StartVelocityMax = FVector(0.1f, 0.1f, 1.0f);
	LOD->AddModule(Velocity);

	LOD->AddModule(NewObject<UParticleModuleColorOverLife>());

	UParticleEmitter* Emitter = NewObject<UParticleEmitter>();
	Emitter->SetEmitterName("Particle Emitter");
	Emitter->AddLODLevel(LOD);
	return Emitter;
}

bool FParticleSystemEditor::CreateDefaultAsset(const FString& InFilePath)
{
	// New particle assets start with one default sprite emitter so the preview shows something right away.
	UParticleSystem* NewSystem = NewObject<UParticleSystem>();
	NewSystem->AddEmitter(CreateDefaultSpriteEmitter());

	const FString FilePath = FAssetUtils::ResolveAssetFilePath(InFilePath);

	FString Error;
	const bool bWritten = ParticleSystemFile::Write(FPaths::FromUtf8(FilePath.empty() ? InFilePath : FilePath), *NewSystem, Error);
	if (bWritten)
	{
		HTR_LOG(Info, "Created ParticleSystem: {}", InFilePath);
	}
	else
	{
		HTR_LOG(Warning, "Failed to create ParticleSystem: {} ({})", InFilePath, Error);
	}

	// Only the file is needed; the editor loads its own copy through UAssetManager when it opens.
	NewSystem->ClearEmitters();
	delete NewSystem;
	return bWritten;
}

UParticleEmitter* FParticleSystemEditor::DuplicateEmitter(UParticleEmitter* Emitter)
{
	if (!ParticleSystem || !Emitter)
	{
		return nullptr;
	}

	const int32 Index = ParticleSystem->FindEmitterIndex(Emitter);
	if (Index < 0)
	{
		return nullptr;
	}

	UParticleEmitter* Copy = ParticleSystemFile::DuplicateEmitter(*Emitter);
	if (!Copy)
	{
		return nullptr;
	}

	ParticleSystem->InsertEmitter(Copy, Index + 1);
	SelectedObject = Copy;
	NotifyStructureChanged();
	return Copy;
}

void FParticleSystemEditor::DeleteEmitter(UParticleEmitter* Emitter)
{
	if (!ParticleSystem || !ParticleSystem->RemoveEmitter(Emitter))
	{
		return;
	}
	NotifyStructureChanged();
}

void FParticleSystemEditor::RenameEmitter(UParticleEmitter* Emitter, const FString& NewName)
{
	if (!Emitter || NewName.empty() || Emitter->GetEmitterName() == NewName)
	{
		return;
	}

	// 이름은 시뮬레이션에 영향이 없으므로 다시 시작하지 않는다.
	Emitter->SetEmitterName(NewName);
	bDirty = true;
}

bool FParticleSystemEditor::Save()
{
	if (!ParticleSystem)
	{
		return false;
	}

	const FString& Path = GetAssetPath();
	const FString FilePath = FAssetUtils::ResolveAssetFilePath(Path);

	FString Error;
	if (!ParticleSystemFile::Write(FPaths::FromUtf8(FilePath.empty() ? Path : FilePath), *ParticleSystem, Error))
	{
		StatusMessage = "Save failed: " + Error;
		HTR_LOG(Warning, "Failed to save ParticleSystem: {} ({})", Path, Error);
		return false;
	}

	bDirty = false;
	StatusMessage = "Saved";
	HTR_LOG(Info, "Saved ParticleSystem: {}", Path);
	return true;
}

bool FParticleSystemEditor::ConfirmClose()
{
	if (!IsDirty() || bCloseConfirmed)
	{
		return true;
	}

	// 탭이 다음 프레임에 확인 창을 연다
	bUnsavedPromptRequested = true;
	return false;
}

void FParticleSystemEditor::CloseWithoutPrompt()
{
	bCloseConfirmed = true;
	Close();
}

int32 FParticleSystemEditor::GetActiveParticleCount() const
{
	if (!PreviewComponent)
	{
		return 0;
	}

	int32 Count = 0;
	for (const FParticleEmitterInstance* Instance : PreviewComponent->GetEmitterInstances())
	{
		if (Instance)
		{
			Count += Instance->ActiveParticles;
		}
	}
	return Count;
}

void FParticleSystemEditor::FocusParticles()
{
	if (!ViewportClient)
	{
		return;
	}

	FBoxSphereBounds Bounds
	{
		.Origin = FVector::ZeroVector,
		.BoxExtent = FVector(DefaultFocusRadius, DefaultFocusRadius, DefaultFocusRadius),
		.SphereRadius = DefaultFocusRadius
	};

	// 고정 바운드가 있거나 파티클이 살아 있으면 그 박스에 맞춘다.
	if (PreviewComponent && (GetActiveParticleCount() > 0 ||
		(ParticleSystem && ParticleSystem->bUseFixedRelativeBoundingBox)))
	{
		// 바운드는 파티클 중심만 담으므로 스프라이트/메시 크기만큼 조금 키운다.
		const FBox Box = PreviewComponent->CalcLocalBounds();
		const FVector Extent = (Box.Max - Box.Min) * 0.5f;
		const float Radius = std::max(Extent.Size() * 1.2f, MinFocusRadius);
		Bounds.Origin = (Box.Min + Box.Max) * 0.5f;
		Bounds.BoxExtent = Extent;
		Bounds.SphereRadius = Radius;
	}

	ViewportClient->FocusOnBounds(Bounds);
}

void FParticleSystemEditor::SetViewportState(uint32 Width, uint32 Height, const FViewportCameraInput& Input)
{
	ViewportWidth = Width;
	ViewportHeight = Height;
	bViewportVisible = Width > 0 && Height > 0;
	CameraInput = Input;
}

void FParticleSystemEditor::Tick(float DeltaTime)
{
	if (!ViewportClient || !ViewportClient->GetWorld())
	{
		return;
	}

	// 뷰포트가 보이면 카메라 이동을 적용
	if (bViewportVisible)
	{
		FViewportCameraState Camera{
			.Location = ViewportClient->GetViewLocation(),
			.Rotation = ViewportClient->GetViewRotation(),
			.OrthoWidth = ViewportClient->GetOrthoWidth(),
			.ViewWidth = static_cast<float>(ViewportWidth),
			.bPerspective = true
		};
		ApplyViewportCameraMovement(Camera, CameraInput, DeltaTime, 5.0f, 0.2f);
		ViewportClient->SetViewLocation(Camera.Location);
		ViewportClient->SetViewRotation(Camera.Rotation);
	}
	CameraInput = {};

	if (bInitialFocusPending)
	{
		InitialFocusElapsed += DeltaTime;
	}

	// 드래그 같은 연속 편집이 끝난 뒤에 한 번만 다시 시작한다.
	if (bRestartPending && !ImGui::IsAnyItemActive())
	{
		RestartSimulation();
	}

	// 파티클 시뮬레이션. 미리보기 월드의 Tick은 파티클 컴포넌트를 돌리지 않으므로 직접 돌린다.
	if (PreviewComponent && !bPaused)
	{
		const float SimDelta = DeltaTime * SimulationSpeed;
		if (PreviewComponent->IsActive())
		{
			PreviewComponent->TickComponent(SimDelta);
		}
		else if (bAutoRestart)
		{
			// 루프가 끝난 시스템은 잠깐 쉬었다가 다시 시작한다.
			if (AutoRestartTimer < 0.0f)
			{
				AutoRestartTimer = AutoRestartDelay;
			}
			AutoRestartTimer -= SimDelta;
			if (AutoRestartTimer <= 0.0f)
			{
				RestartSimulation();
			}
		}
	}

	// 부모 Tick 호출 (PreviewScene Tick)
	FAssetEditor::Tick(DeltaTime);
}

void FParticleSystemEditor::OnRenderPreview()
{
	// Viewport가 보이지 않으면 렌더링하지 않음
	const bool bRender = bViewportVisible;
	bViewportVisible = false;
	if (!bRender || !ViewportClient || !ViewportClient->GetWorld())
	{
		return;
	}

	ViewportClient->Resize(ViewportWidth, ViewportHeight);
	if (!ViewportClient->GetViewTargets().IsValidFor(ViewportWidth, ViewportHeight))
	{
		return;
	}

	// 처음 열었을 때는 파티클이 아직 없으므로, 스폰된 뒤(또는 기다리다 시간이 지나면) 한 번 맞춘다.
	if (bInitialFocusPending)
	{
		if (GetActiveParticleCount() > 0 || InitialFocusElapsed >= InitialFocusTimeout)
		{
			bInitialFocusPending = false;
		}
		FocusParticles();
	}

	ViewportClient->Draw(EditorEngine.GetEngineLoop().GetRenderer());
}
