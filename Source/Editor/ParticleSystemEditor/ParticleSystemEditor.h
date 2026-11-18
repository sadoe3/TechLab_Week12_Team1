#pragma once

#include "Editor/AssetEditor/AssetEditor.h"
#include "Camera/ViewportCameraMovement.h"

class UObject;
class UClass;
class UParticleSystem;
class UParticleSystemComponent;
class UParticleEmitter;
class UParticleLODLevel;
class UParticleModule;

// 파티클 시스템 에디터 (Unreal Cascade 대응).
// 미리보기 월드에 UParticleSystemComponent 하나를 띄우고, 에디터가 직접 Tick을 돌려 시뮬레이션한다.
// (미리보기 월드의 Tick은 파티클 컴포넌트를 돌리지 않는다. Cascade도 프리뷰 컴포넌트를 에디터가 직접 Tick한다.)
class FParticleSystemEditor : public FAssetEditor
{
public:
	FParticleSystemEditor(
		class UEditorEngine& InEditorEngine,
		class FEditorManager& InEditorManager,
		const FString& InAssetPath,
		UParticleSystem& InParticleSystem
	);

	////////////////////////////////////////////////////////////
	// 에디터 공통 인터페이스
	////////////////////////////////////////////////////////////

	void Tick(float DeltaTime) override;
	void OnRenderPreview() override;

	const char* GetEditorTypeName() const override { return "ParticleSystem"; }
	bool IsDirty() const override { return bDirty; }

	// 저장하지 않은 변경이 있으면 탭이 확인 창을 띄우도록 닫기를 미룬다.
	bool ConfirmClose() override;
	void CloseWithoutPrompt();
	bool IsUnsavedPromptRequested() const { return bUnsavedPromptRequested; }
	void ClearUnsavedPromptRequest() { bUnsavedPromptRequested = false; }

	////////////////////////////////////////////////////////////
	// 에디터 전용 함수
	////////////////////////////////////////////////////////////

	// 시뮬레이션을 처음부터 다시 시작합니다. (Cascade의 Restart Sim)
	void RestartSimulation();

	// 에셋을 원래 파일에 저장합니다.
	bool Save();

	// 카메라를 파티클 바운드에 맞춥니다.
	void FocusParticles();

	// 뷰포트 상태를 설정합니다. (뷰포트가 보이지 않으면 입력을 무시합니다)
	void SetViewportState(uint32 Width, uint32 Height, const FViewportCameraInput& Input);

	// 속성이 바뀌었음을 알립니다. 편집이 끝나면 시뮬레이션을 다시 시작합니다.
	void NotifyPropertyEdited();

	////////////////////////////////////////////////////////////
	// 구조 편집 (Emitters 패널)
	// 이미터/모듈 구성이 바뀌면 파티클 메모리 배치(모듈 Payload 오프셋)가 달라지므로
	// 이 에셋을 쓰는 모든 컴포넌트의 인스턴스를 즉시 다시 만든다.
	// 모듈은 LOD 0 기준으로 편집하고, 다른 LOD에도 같은 자리에 같은 모듈을 넣고 뺀다 (Cascade와 같다).
	////////////////////////////////////////////////////////////

	// ModuleClass가 TypeData면 이미터의 TypeData를 바꾼다. 그 외에는 모듈 목록 끝에 붙인다.
	UParticleModule* AddModule(UParticleEmitter* Emitter, UClass* ModuleClass);
	// Required/Spawn은 지울 수 없다. TypeData를 지우면 Sprite 이미터가 된다.
	void DeleteModule(UParticleEmitter* Emitter, UParticleModule* Module);
	// 모듈을 TargetEmitter의 TargetIndex 자리로 옮긴다. 같은 이미터 안에서는 순서만 바뀐다.
	void MoveModule(UParticleEmitter* SourceEmitter, UParticleModule* Module, UParticleEmitter* TargetEmitter, int32 TargetIndex);

	// Cascade의 New Particle Sprite Emitter와 같은 기본 구성으로 이미터를 끝에 추가한다.
	UParticleEmitter* AddSpriteEmitter();
	// 바로 뒤에 복제본을 넣는다.
	UParticleEmitter* DuplicateEmitter(UParticleEmitter* Emitter);
	void DeleteEmitter(UParticleEmitter* Emitter);
	void RenameEmitter(UParticleEmitter* Emitter, const FString& NewName);

	// Required / Spawn / TypeData 모듈인지 (켜고 끄거나 순서를 바꿀 수 없다)
	static bool IsFixedModule(const UParticleLODLevel* LOD, const UObject* Module);

	// 시뮬레이션 제어
	bool IsPaused() const { return bPaused; }
	void SetPaused(bool bInPaused) { bPaused = bInPaused; }
	float GetSimulationSpeed() const { return SimulationSpeed; }
	void SetSimulationSpeed(float InSpeed) { SimulationSpeed = InSpeed; }
	bool IsAutoRestart() const { return bAutoRestart; }
	void SetAutoRestart(bool bInAutoRestart) { bAutoRestart = bInAutoRestart; }

	// 디테일 패널에 표시할 대상 (시스템, 이미터, 모듈)
	UObject* GetSelectedObject() const { return SelectedObject; }
	void SetSelectedObject(UObject* InObject) { SelectedObject = InObject; }

	////////////////////////////////////////////////////////////
	// Getter
	////////////////////////////////////////////////////////////

	UParticleSystem* GetParticleSystem() const { return ParticleSystem; }
	UParticleSystemComponent* GetPreviewComponent() const { return PreviewComponent; }
	int32 GetActiveParticleCount() const;
	const FString& GetStatusMessage() const { return StatusMessage; }

protected:
	TUniquePtr<IEditorTab> CreateTab() override;
	bool InitializeResources() override;
	void ShutdownResources() override;

private:
	// 구조가 바뀐 뒤 호출. 저장 표시를 하고 이 에셋을 쓰는 컴포넌트를 모두 다시 초기화한다.
	void NotifyStructureChanged();

	// 지운 이미터/모듈이 선택돼 있었으면 시스템을 선택한다.
	void ValidateSelection();

	UParticleSystem* ParticleSystem = nullptr;
	UParticleSystemComponent* PreviewComponent = nullptr;
	UObject* SelectedObject = nullptr;

	FViewportCameraInput CameraInput;
	uint32 ViewportWidth = 0;
	uint32 ViewportHeight = 0;
	bool bViewportVisible = false;
	bool bInitialFocusPending = true;
	float InitialFocusElapsed = 0.0f;

	bool bDirty = false;
	bool bCloseConfirmed = false;
	bool bUnsavedPromptRequested = false;
	bool bRestartPending = false;
	bool bPaused = false;
	bool bAutoRestart = true;
	float SimulationSpeed = 1.0f;

	// 시스템이 끝난 뒤 다시 시작하기까지 남은 시간
	float AutoRestartTimer = -1.0f;

	FString StatusMessage;
};
