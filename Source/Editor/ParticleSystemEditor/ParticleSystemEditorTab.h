#pragma once

#include "Editor/AssetEditor/DockedAssetEditorTab.h"

class FParticleSystemEditor;

// 파티클 시스템 에디터 탭. Cascade와 같이 Viewport / Emitters / Details / Curve Editor 패널을 배치한다.
class FParticleSystemEditorTab : public FDockedAssetEditorTab
{
public:
	explicit FParticleSystemEditorTab(FParticleSystemEditor& InEditor);

	// 메뉴 바: File / Edit / Window / Help
	void DrawMenuBar() override;
	void OnRender(bool bActive) override;

private:
	void BeforeRender() override;
	void BuildDefaultLayout(ImGuiID InDockSpaceId, const ImVec2& Size) override;
	void DrawUnsavedPrompt();
	void DrawAboutPopup();

	FParticleSystemEditor& ParticleEditor;
	bool bAboutRequested = false;
};
