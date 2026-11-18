#pragma once

#include "Editor/AssetEditor/AssetEditorPanel.h"

#include <functional>
#include <map>

class FParticleSystemEditor;

using FParticleSystemEditorPanel = TAssetEditorPanel<FParticleSystemEditor>;

// 미리보기 렌더 결과와 시뮬레이션 툴바를 그리고, 카메라 입력과 화면 크기를 에디터에 전달한다.
class FParticleViewportPanel : public FParticleSystemEditorPanel
{
public:
	explicit FParticleViewportPanel(FParticleSystemEditor& InEditor);

protected:
	void DrawContent() override;
	ImGuiWindowFlags GetWindowFlags() const override { return ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse; }

private:
	void DrawToolbar();
};

// 시스템 안의 이미터를 열로, 각 이미터의 모듈을 행으로 나열한다 (Cascade Emitters 패널).
//  - 이미터 열 빈 곳 우클릭: 모듈 추가 / 헤더 우클릭: 이미터 이름 변경·복제·삭제
//  - 패널 오른쪽 빈 곳 우클릭: 새 Sprite 이미터
//  - 모듈 행 드래그: 순서 변경 (다른 이미터로 옮기기도 된다)
class FParticleEmittersPanel : public FParticleSystemEditorPanel
{
public:
	explicit FParticleEmittersPanel(FParticleSystemEditor& InEditor);

protected:
	void DrawContent() override;
	ImGuiWindowFlags GetWindowFlags() const override { return ImGuiWindowFlags_HorizontalScrollbar; }

private:
	void DrawEmitterColumn(class UParticleEmitter* Emitter, float Width, float Height);
	// Index는 모듈 목록 안의 자리. Required/Spawn/TypeData는 -1 (켜기·끄기, 드래그 불가)
	void DrawModuleRow(class UParticleEmitter* Emitter, class UParticleLODLevel* LOD, class UParticleModule* Module, const char* Kind, int32 Index);
	void DrawAddModuleMenu(class UParticleEmitter* Emitter);
	void DrawRenamePopup();

	// 이미터/모듈 배열을 순회하는 도중에 바꾸지 않도록, 메뉴·드롭으로 생긴 편집은 다 그린 뒤에 실행한다.
	std::function<void()> PendingAction;

	class UParticleEmitter* RenameTarget = nullptr;
	char RenameBuffer[128] = {};
	bool bOpenRenamePopup = false;
};

// 선택한 시스템 / 이미터 / 모듈의 속성을 편집한다.
class FParticleDetailsPanel : public FParticleSystemEditorPanel
{
public:
	explicit FParticleDetailsPanel(FParticleSystemEditor& InEditor);

protected:
	void DrawContent() override;

private:
	// Required의 Material이 이 이미터에서 쓰이는지(Sprite / Mesh + bOverrideMaterial) 알려 준다.
	void DrawRequiredMaterialHint(const class UParticleModuleRequired* Required);
	// Spawn 모듈의 Burst 목록 추가/삭제/편집 (리플렉션 프로퍼티가 아니라 직접 그린다)
	void DrawBurstList(class UParticleModuleSpawn* Spawn);
};

// View / selection / drag state of the Curve Editor graph. Lives across frames; reset when the selected module changes.
struct FCurveGraphState
{
	enum class EDrag : uint8 { None, Key, ArriveTangent, LeaveTangent, Pan };

	// Visible range in curve units (X = curve input, Y = curve output)
	float ViewMinX = 0.0f;
	float ViewMaxX = 1.0f;
	float ViewMinY = 0.0f;
	float ViewMaxY = 1.0f;
	bool bFitRequested = true;

	// Selected key: which curve property, which key, which component (0 for float, 0..2 = X/Y/Z for vector)
	FString SelectedCurve;
	int32 SelectedKey = -1;
	int32 SelectedComponent = 0;

	EDrag Drag = EDrag::None;

	// Key the right-click menu acts on
	FString MenuCurve;
	int32 MenuKey = -1;

	// Bit per component: 1 = hidden. Keyed by curve property name.
	std::map<FString, uint8> HiddenMask;

	const UObject* LastObject = nullptr;
};

// Curves of the selected module: a graph (keys and tangents can be dragged) plus a key table per curve.
//  - Left drag key: move time/value   - Left drag tangent handle: set tangent (Auto keys become CurveUser)
//  - Double-click empty space: add a key   - Right-click key: mode / delete   - Delete: remove selected key
//  - Right or middle drag: pan   - Wheel: zoom (Shift = X only, Ctrl = Y only)   - F or "Fit": frame all curves
class FParticleCurveEditorPanel : public FParticleSystemEditorPanel
{
public:
	explicit FParticleCurveEditorPanel(FParticleSystemEditor& InEditor);

protected:
	void DrawContent() override;

private:
	FCurveGraphState Graph;
};
